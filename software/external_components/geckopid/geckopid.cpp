#include "geckopid.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/pid/pid_autotuner.h"
#include <cmath>

namespace esphome {
namespace geckopid {

static const char *const TAG = "geckopid";
static const uint32_t CALIB_MAGIC = 0xCA11B003;
static const uint32_t PREF_HASH = 0x6743'5044;  // "gPID"

void GeckoPid::setup() {
  this->pref_ = global_preferences->make_preference<CalibData>(PREF_HASH);
  if (!this->pref_.load(&this->data_) || this->data_.magic != CALIB_MAGIC) {
    this->data_ = CalibData{};
    this->data_.magic = CALIB_MAGIC;
    ESP_LOGI(TAG, "No stored calibration; keeping YAML PID defaults");
  }
  // Apply any valid stored sets so tuned values are active from boot; the runtime swap takes
  // over once each climate reports a heating/cooling action.
  this->apply_set_(this->thermostat_, this->data_.heat);
  this->apply_set_(this->hygrostat_, this->data_.humid);
}

void GeckoPid::apply_set_(pid::PIDClimate *c, const PidSet &s) {
  if (c == nullptr || !s.valid)
    return;
  c->set_kp(s.kp);
  c->set_ki(s.ki);
  c->set_kd(s.kd);
  ESP_LOGI(TAG, "%s: applied kp=%.5f ki=%.5f kd=%.5f (vicinity@tune=%.1f°C)", c->get_name().c_str(), s.kp, s.ki, s.kd,
           s.vicinity);
}

PidSet *GeckoPid::set_for_(int target) {
  switch (target) {
    case 1:
      return &this->data_.heat;
    case 2:
      return &this->data_.cool;
    case 3:
      return &this->data_.humid;
    case 4:
      return &this->data_.dehumid;
    default:
      return nullptr;
  }
}

pid::PIDClimate *GeckoPid::climate_for_(int target) {
  return (target == 1 || target == 2) ? this->thermostat_ : this->hygrostat_;
}

void GeckoPid::loop() {
  if (this->active_ != 0) {
    uint32_t now = millis();
    if (now - this->start_ms_ > this->timeout_ms_) {
      ESP_LOGW(TAG, "Autotune timed out for target %d", this->active_);
      this->finish_(false);
      return;
    }
    // ESPHome writes the autotuned parameters exactly once, at completion. A change in kp is
    // therefore our completion signal (the runtime swap is suspended while calibrating).
    if (this->active_climate_->get_kp() != this->baseline_kp_) {
      PidSet *ps = this->set_for_(this->active_);
      ps->kp = this->active_climate_->get_kp();
      ps->ki = this->active_climate_->get_ki();
      ps->kd = this->active_climate_->get_kd();
      ps->vicinity = (this->vicinity_sens_ != nullptr && this->vicinity_sens_->has_state())
                         ? this->vicinity_sens_->state
                         : NAN;
      ps->valid = true;
      ESP_LOGI(TAG, "Autotune target %d complete: kp=%.5f ki=%.5f kd=%.5f vicinity=%.1f°C", this->active_, ps->kp,
               ps->ki, ps->kd, ps->vicinity);
      this->save_();
      this->finish_(true);
    }
    return;
  }
  this->runtime_swap_();
}

void GeckoPid::runtime_swap_() {
  if (this->thermostat_ != nullptr) {
    climate::ClimateAction a = this->thermostat_->action;
    if (a != this->last_therm_) {
      if (a == climate::CLIMATE_ACTION_HEATING)
        this->apply_set_(this->thermostat_, this->data_.heat);
      else if (a == climate::CLIMATE_ACTION_COOLING)
        this->apply_set_(this->thermostat_, this->data_.cool);
      this->last_therm_ = a;
    }
  }
  if (this->hygrostat_ != nullptr) {
    climate::ClimateAction a = this->hygrostat_->action;
    if (a != this->last_hygro_) {
      if (a == climate::CLIMATE_ACTION_HEATING)
        this->apply_set_(this->hygrostat_, this->data_.humid);
      else if (a == climate::CLIMATE_ACTION_COOLING)
        this->apply_set_(this->hygrostat_, this->data_.dehumid);
      this->last_hygro_ = a;
    }
  }
}

void GeckoPid::start_calibration() {
  if (this->active_ != 0) {
    ESP_LOGW(TAG, "Calibration already running (target %d)", this->active_);
    return;
  }
  int t = this->selected_;
  pid::PIDClimate *c = this->climate_for_(t);
  if (c == nullptr) {
    ESP_LOGW(TAG, "No climate configured for target %d", t);
    return;
  }
  this->active_ = t;
  this->active_climate_ = c;
  if (this->calib_target_ != nullptr)
    globals::id(this->calib_target_) = t;  // actuators -> direct pass-through

  // Ensure the controller runs so the relay output is written to the actuator.
  this->saved_mode_ = c->mode;
  auto mode_call = c->make_call();
  mode_call.set_mode(climate::CLIMATE_MODE_HEAT_COOL);
  mode_call.perform();

  // Restrict the relay to a single direction so a heat_cool climate yields one direction's params.
  bool heat_dir = (t == 1 || t == 3);  // heating or humidification
  auto tuner = make_unique<pid::PIDAutotuner>();
  tuner->set_noiseband(this->noiseband_);
  tuner->set_output_positive(heat_dir ? 1.0f : 0.0f);
  tuner->set_output_negative(heat_dir ? 0.0f : -1.0f);

  this->baseline_kp_ = c->get_kp();
  this->start_ms_ = millis();
  // Humidity responds slowly (coarse slow-PWM rain pump) -> allow much longer.
  this->timeout_ms_ = (t == 3 || t == 4) ? 3600000UL : 1800000UL;

  c->start_autotune(std::move(tuner));
  ESP_LOGI(TAG, "Started calibration target %d (%s)", t, heat_dir ? "heat/humidify" : "cool/dehumidify");
}

void GeckoPid::abort_calibration() {
  if (this->active_ == 0)
    return;
  ESP_LOGW(TAG, "Calibration aborted");
  this->finish_(false);
}

void GeckoPid::finish_(bool ok) {
  if (this->calib_target_ != nullptr)
    globals::id(this->calib_target_) = 0;  // actuators -> normal operation
  if (this->active_climate_ != nullptr) {
    auto call = this->active_climate_->make_call();
    call.set_mode(this->saved_mode_);
    call.perform();
  }
  this->active_ = 0;
  this->active_climate_ = nullptr;
  // Force the runtime swap to re-apply the correct stored set on the next action change.
  this->last_therm_ = climate::CLIMATE_ACTION_OFF;
  this->last_hygro_ = climate::CLIMATE_ACTION_OFF;
  ESP_LOGI(TAG, "Calibration finished (%s)", ok ? "saved" : "aborted/failed");
}

void GeckoPid::save_() {
  if (this->pref_.save(&this->data_)) {
    ESP_LOGI(TAG, "Saved calibration to flash");
  } else {
    ESP_LOGW(TAG, "Failed to save calibration");
  }
}

}  // namespace geckopid
}  // namespace esphome
