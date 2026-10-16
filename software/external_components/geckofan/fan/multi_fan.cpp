#include <algorithm>
#include <float.h>
#include <cmath>
#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace geckofan {

static const char *const TAG = "geckofan";

// How far over setpoint the terrarium has to run before the fan is allowed out of the simulated
// windspeed band. The band is realism; this is the animal's safety, and it outranks the weather.
static const float OVERHEAT_RELEASE_K = 1.5f;

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MultiFan...");
  // auto traits = fan::FanTraits(true, true, false, 100);
  // this->fan_->set_traits(traits);
  this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}

void MultiFan::dump_config() {
  ESP_LOGCONFIG(TAG, "MultiFan '%s':", this->fan_->get_name().c_str());
  if (this->hygro_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Hygro sensor name='%s'", this->hygro_->get_name().c_str());
  }
  if (this->thermo_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Thermo sensor name='%s'", this->thermo_->get_name().c_str());
  }
  if (this->vicinity_sens_ != NULL) {
    ESP_LOGCONFIG(TAG, "Vicinity temperature sensor name='%s'", this->vicinity_sens_->get_name().c_str());
  }
  if (this->windspeed_sens_ != NULL) {
    ESP_LOGCONFIG(TAG, "Windspeed sensor name='%s'", this->windspeed_sens_->get_name().c_str());
  }
}

void MultiFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  bool enable = this->auto_mode_();
  if (enable != this->enable_auto_mode_) {
    this->enable_auto_mode_ = enable;
    ESP_LOGD(TAG, "Setting auto mode: %s", ONOFF(enable));
  }

  float speed = 0.0f;
  if (this->fan_->state) {
    speed = static_cast<float>(this->fan_->speed) / 100.f;
  }
  ESP_LOGD(TAG, "Setting speed: %.2f", speed);
  this->output_->set_level(speed);

  // The fan speed just changed -> don't make the tacho wait out its slow update_interval. Sample
  // now (closes the previous counting window and publishes the final old-speed reading), then once
  // more after a short window so the new speed shows up promptly in Home Assistant.
  if (this->tacho_ != nullptr && (!this->have_applied_ || std::fabs(speed - this->last_applied_level_) > 0.005f)) {
    this->tacho_->update();
    this->set_timeout("tacho_resample", TACHO_SAMPLE_MS, [this]() { this->tacho_->update(); });
  }
  this->have_applied_ = true;
  this->last_applied_level_ = speed;
}

void MultiFan::write_state(float state) {
  // During cooling / dehumidification calibration the PID relay output is routed here as the
  // climate's cool_output; drive the PWM directly so the autotune relay reaches the fan.
  if (this->calibrating_fan_()) {
    float level = clamp(state, 0.0f, 1.0f);
    ESP_LOGD(TAG, "calibration pass-through: fan output %.2f", level);
    this->output_->set_level(level);
    return;
  }
  ESP_LOGV(TAG, "write_state %.2f (ignored)", state);
}

float MultiFan::get_windspeed() {
  if (this->windspeed_sens_ && this->windspeed_sens_->has_state()) {
    return this->windspeed_sens_->state;
  }
  return 0.f;
}

bool MultiFan::get_vicinity_temperature(float *vicinity) {
  if (this->vicinity_sens_ && this->vicinity_sens_->has_state()) {
    *vicinity = this->vicinity_sens_->state;
    return true;
  }
  return false;
}

void MultiFan::set_combined_fan_value() {
  climate::ClimateMode therm_mode = this->thermo_->mode;
  climate::ClimateMode hygro_mode = this->hygro_->mode;
  bool dehumidification = (hygro_mode == climate::CLIMATE_MODE_HEAT_COOL || hygro_mode == climate::CLIMATE_MODE_COOL);
  bool cooling = (therm_mode == climate::CLIMATE_MODE_HEAT_COOL || therm_mode == climate::CLIMATE_MODE_COOL);

  // The Home Assistant windspeed sensor reports the simulated ambient wind in km/h.
  // It defines a band the fan output must stay within (see README.md):
  //   windspeed == 0 km/h  -> fan must never exceed 10%   (ceil = 10)
  //   windspeed >  5 km/h  -> fan must be at least 10%     (floor >= 10)
  //   windspeed > 30 km/h  -> fan must be at least 90%
  //   windspeed > 50 km/h  -> fan must run at 100%
  // The floor is a hard minimum that the PID demand may not pull below; the ceiling
  // only caps the fan in calm weather and ramps up to 100% by 5 km/h.
  float windspeed = get_windspeed();
  float wind_ceil = clamp(10.0f + windspeed / 5.0f * 90.0f, 10.0f, 100.0f);  // 10% @ 0 .. 100% @ >=5 km/h
  float wind_floor;                                                          // piecewise ambient-wind minimum
  if (windspeed <= 5.0f)
    wind_floor = windspeed / 5.0f * 10.0f;                       // 0% .. 10%
  else if (windspeed <= 30.0f)
    wind_floor = 10.0f + (windspeed - 5.0f) / 25.0f * 80.0f;     // 10% .. 90%
  else if (windspeed <= 50.0f)
    wind_floor = 90.0f + (windspeed - 30.0f) / 20.0f * 10.0f;    // 90% .. 100%
  else
    wind_floor = 100.0f;

  float hum_ctrl_val = this->hygro_->get_output_value() * (-1.0f);
  float therm_ctrl_val = this->thermo_->get_output_value() * (-1.0f);

  // In calm simulated weather the ceiling pins the fan at 10% no matter how hot it gets in here.
  // That is the right call for a terrarium sitting at its setpoint and the wrong one for a
  // terrarium running away, so a real overheat lifts the ceiling and lets the PID have the fan.
  float current = this->thermo_->current_temperature;
  float excess = current - this->thermo_->target_temperature;
  if (cooling && !std::isnan(current) && excess > OVERHEAT_RELEASE_K) {
    ESP_LOGD(TAG, "%.2f K over setpoint -> releasing the calm-weather ceiling (was %.1f%%)", excess, wind_ceil);
    wind_ceil = 100.0f;
  }

  // PID-demanded cooling/dehumidification share of the fan output (0..100%).
  float set_val = 0.0f;
  if (dehumidification || cooling) {
    set_val = hum_ctrl_val * 100.0f;  // dehumidification baseline
    if (cooling) {
      float vt;
      bool have_vt = get_vicinity_temperature(&vt);
      if (have_vt && this->thermo_->target_temperature < vt - 1.0f && therm_ctrl_val > 1.0f) {
        // The fan can only equalize towards the vicinity temperature, never cool below it.
        // The thermostat is asking for the impossible, so ignore its demand here and let the
        // ambient windspeed band drive the fan instead of blasting it pointlessly.
        ESP_LOGD(TAG, "target_temp: %.2f°C < vicinity_temp: %.2f-1.0°C -> ignore thermostat, follow ambient wind %.2f km/h",
            this->thermo_->target_temperature, vt, windspeed);
        set_val = hum_ctrl_val * 100.0f;
      } else {
        // Two independent consumers share one fan, so the operator is max(), not a weighted mean:
        // a mean *dilutes* a saturated demand against an idle one. This used to be a mean with the
        // hygrostat weighted 4 against the thermostat's 1, so a full cooling demand came out as 20%
        // fan -- the thermostat could never ask for more than a fifth of the actuator and the
        // morning heat simply stayed in the terrarium. A maximum cannot be weighted, so the
        // per-climate `weight:` option went with it.
        set_val = std::max(hum_ctrl_val, therm_ctrl_val) * 100.0f;
      }
    }
  }

  // Clamp the PID demand into the windspeed band: the floor enforces the minimum ambient
  // wind, the ceiling enforces the calm-weather cap.
  float speed = clamp(clamp(set_val, wind_floor, wind_ceil), 0.0f, 100.0f);
  ESP_LOGD(TAG, "fan value: windspeed=%.2f km/h, humidifier=%.2f, thermostat=%.2f, set_val=%.2f, band=[%.1f, %.1f] -> speed=%.0f%%",
      windspeed, hum_ctrl_val, therm_ctrl_val, set_val, wind_floor, wind_ceil, speed);

  auto fan = this->fan_->make_call();
  fan.set_state(speed > 0.0f);
  fan.set_speed((int) speed);
  fan.perform();
}

void MultiFan::set_hygro(pid::PIDClimate *hygro) {
  this->hygro_ = hygro;
  hygro->add_on_pid_computed_callback([this]() { this->update_from_pid_(true); });
}
void MultiFan::set_thermo(pid::PIDClimate *thermo) {
  this->thermo_ = thermo;
  thermo->add_on_pid_computed_callback([this]() { this->update_from_pid_(false); });
}

void MultiFan::update_from_pid_(bool hygro_action) {
  // While the fan is being autotuned its output is driven directly via write_state();
  // don't let the combined formula clobber the relay test.
  if (this->calibrating_fan_()) {
    return;
  }
  // Manual mode: the HA slider drives the fan, so leave it alone.
  if (!this->auto_mode_()) {
    return;
  }

  // The PID callbacks fire on every sensor update; only recompute the fan value once a minute,
  // or sooner if a control value changed meaningfully. Avoids churning the fan every few seconds.
  float hum = this->hygro_->get_output_value() * -1.0f;
  float therm = this->thermo_->get_output_value() * -1.0f;
  uint32_t now = millis();
  bool due = !this->have_last_ || (now - this->last_compute_ms_) >= RECOMPUTE_INTERVAL_MS;
  bool changed = !this->have_last_ || std::fabs(hum - this->last_hum_ctrl_) >= RECOMPUTE_THRESHOLD ||
                 std::fabs(therm - this->last_therm_ctrl_) >= RECOMPUTE_THRESHOLD;
  if (!due && !changed) {
    return;
  }
  this->have_last_ = true;
  this->last_compute_ms_ = now;
  this->last_hum_ctrl_ = hum;
  this->last_therm_ctrl_ = therm;
  set_combined_fan_value();
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace geckofan
}  // namespace esphome
