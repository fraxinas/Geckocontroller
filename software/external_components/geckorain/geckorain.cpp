#include "geckorain.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace geckorain {

static const char *const TAG = "geckorain";

// Bumped when the reservoir went from pump runtime to water (ml at the measured flow): what was
// stored before means nothing now. Dropping it starts from a full reservoir and the YAML's values.
static const uint32_t PREF_HASH = 0x6752'6538;

struct ReservoirPref {
  uint32_t config_hash;  // the YAML flow and reservoir_volume this was stored under
  float flow;            // ml/s, calibrated or from the YAML
  float reservoir_ml;    // ml the pump draws from one filling, calibrated or from the YAML
  float remaining_ml;
};

void RainPumpOutput::setup() {
  this->turn_off();
  this->current_state_ = false;
  this->period_start_time_ = millis();

  this->pref_ = global_preferences->make_preference<ReservoirPref>(PREF_HASH);
  ReservoirPref p;
  if (!this->pref_.load(&p)) {
    ESP_LOGI(TAG, "nothing stored: flow and reservoir volume from the YAML, full reservoir");
    this->remaining_ml_ = this->reservoir_ml_;
  } else {
    if (p.config_hash != this->config_hash_) {
      // flow or reservoir_volume were edited in the YAML: that wins over an earlier calibration
      ESP_LOGI(TAG, "flow / reservoir_volume changed in the YAML, dropping the calibrated %.2f ml/s, %.0f ml", p.flow,
               p.reservoir_ml);
    } else {
      if (p.flow > 0.0f && std::isfinite(p.flow))
        this->flow_ = p.flow;
      if (this->reservoir_tracked_() && p.reservoir_ml > 0.0f && std::isfinite(p.reservoir_ml))
        this->reservoir_ml_ = p.reservoir_ml;
    }
    this->remaining_ml_ = std::isfinite(p.remaining_ml) ? clamp(p.remaining_ml, 0.0f, this->reservoir_ml_)
                                                        : this->reservoir_ml_;
  }
  // Marked empty (0 ml; the tracking alone stops at the reserve): the line ran dry.
  this->dry_ = this->reservoir_tracked_() && this->remaining_ml_ <= 0.0f;
  this->publish_model_();
  this->publish_level_(true);
  this->publish_calibration_();
  if (this->pump_state_sens_ != nullptr)
    this->pump_state_sens_->publish_initial_state(false);
}

// ------------------- Calibration -------------------

void RainPumpOutput::publish_model_() {
  if (this->flow_sens_ != nullptr)
    this->flow_sens_->publish_state(this->flow_);
  if (this->volume_sens_ != nullptr)
    this->volume_sens_->publish_state(this->reservoir_tracked_() ? this->reservoir_ml_ : NAN);
}

void RainPumpOutput::start_reservoir_calibration() {
  if (!this->reservoir_tracked_()) {
    ESP_LOGW(TAG, "flow calibration refused: reservoir tracking is off (reservoir_volume: 0)");
    return;
  }
  if (this->calibrating_)
    return;
  this->set_pump_(false);  // whatever ran is settled against the previous filling
  this->manual_duration_ = 0;
  this->manual_pending_ = false;
  this->reset_spray_();
  this->pulse_ms_ = 0;
  this->calibrating_ = true;
  this->calibration_start_ = millis();
  this->calibration_runtime_ = 0;
  this->remaining_ml_ = this->reservoir_ml_;  // started with a full reservoir
  this->release_empty_lockout_();
  this->publish_level_(true);
  ESP_LOGI(TAG, "flow calibration: pumping continuously through the nozzles in %u s; press \"Reservoir empty\" "
                "once the reservoir is empty, then enter the measured ml", CALIBRATION_DELAY / 1000);
  // loop() switches the pump on after the delay and keeps it on
}

void RainPumpOutput::stop_reservoir_calibration() {
  if (!this->calibrating_)
    return;
  uint32_t on = this->current_state_ ? millis() - this->run_start_ : 0;
  this->calibrating_ = false;
  this->calibration_runtime_ = 0;
  this->set_pump_(false);
  // what is left, as far as the old model can tell
  this->remaining_ml_ = std::max(this->reservoir_ml_ - this->water_ml_(on), 0.0f);
  this->save_reservoir_();
  this->publish_level_(true);
  this->publish_calibration_();
  ESP_LOGW(TAG, "flow calibration aborted after %u s, flow unchanged", on / 1000);
}

void RainPumpOutput::reservoir_empty() {
  if (!this->reservoir_tracked_()) {
    ESP_LOGW(TAG, "reservoir empty, but reservoir tracking is off (reservoir_volume: 0)");
    return;
  }
  if (this->calibrating_) {
    uint32_t on = this->current_state_ ? millis() - this->run_start_ : 0;
    this->calibrating_ = false;
    this->set_pump_(false);
    if (on < CALIBRATION_MIN) {
      ESP_LOGW(TAG, "flow calibration: %u s of pumping is no measurement, flow unchanged", on / 1000);
    } else {
      this->calibration_runtime_ = on;
      ESP_LOGI(TAG, "flow calibration: pumped for %.1f s; enter the ml in the measuring cup", on / 1000.0f);
    }
    this->publish_calibration_();
  } else {
    this->set_pump_(false);
    ESP_LOGI(TAG, "reservoir marked empty");
  }
  // The pump is locked out and the automation switches off (loop) until "Reservoir refilled"; the
  // line has run dry and wants the long prime then.
  this->remaining_ml_ = 0;
  this->dry_ = true;
  this->save_reservoir_();
  this->publish_level_(true);
}

void RainPumpOutput::calibrate_flow(float ml) {
  if (this->calibration_runtime_ == 0) {
    ESP_LOGW(TAG, "flow calibration result %.0f ml ignored: no flow calibration run awaiting it", ml);
    return;
  }
  if (!(ml > 0.0f)) {
    ESP_LOGW(TAG, "flow calibration: %.0f ml is no measurement", ml);
    return;
  }
  uint32_t on = this->calibration_runtime_;
  float flow = ml / ((float) on / 1000.0f);
  ESP_LOGI(TAG, "flow calibration: %.0f ml in %.1f s, flow %.2f ml/s (was %.2f), reservoir volume %.0f ml (was %.0f)",
           ml, on / 1000.0f, flow, this->flow_, ml, this->reservoir_ml_);
  this->flow_ = flow;
  if (this->reservoir_tracked_())
    this->reservoir_ml_ = ml;
  this->calibration_runtime_ = 0;
  this->remaining_ml_ = 0;  // the run emptied it
  this->save_reservoir_();
  this->publish_model_();
  this->publish_level_(true);
  this->publish_calibration_();
}

// ------------------- Reservoir -------------------

void RainPumpOutput::run_manual(uint32_t ms) {
  if (this->calibrating_) {
    ESP_LOGW(TAG, "Let it rain refused: flow calibration running");
    return;
  }
  if (this->reservoir_empty_()) {
    ESP_LOGW(TAG, "Let it rain refused: reservoir empty, refill it and press \"Reservoir refilled\"");
    return;
  }
  ESP_LOGI(TAG, "Let it rain: pump on for %u ms (~%.1f ml)", ms, this->water_ml_(ms));
  this->manual_pending_ = true;  // timed from the loop pass that switches the pump on
  this->manual_duration_ = ms;
}

void RainPumpOutput::refill_reservoir() {
  if (!this->reservoir_tracked_()) {
    ESP_LOGW(TAG, "reservoir refilled, but reservoir tracking is off (reservoir_volume: 0)");
    return;
  }
  if (this->calibrating_) {
    ESP_LOGW(TAG, "reservoir refilled during the flow calibration: ignored, press \"Reservoir empty\" once empty");
    return;
  }
  // Settle the water of a pump that is running right now, so it is not deducted from the new filling.
  this->account_(millis());
  this->remaining_ml_ = this->reservoir_ml_;
  this->save_reservoir_();
  this->publish_level_(true);
  ESP_LOGI(TAG, "reservoir refilled: %.0f ml", this->remaining_ml_);
  this->release_empty_lockout_();
}

// Switch the rain automation back on if it was the empty reservoir that switched it off.
void RainPumpOutput::release_empty_lockout_() {
  if (!this->disabled_by_empty_)
    return;
  this->disabled_by_empty_ = false;
  if (this->enabled_ != nullptr) {
    ESP_LOGI(TAG, "re-enabling the rain automation the empty reservoir had switched off");
    *this->enabled_ = true;
  }
}

// Deduct the water of the running pump since the last call from the reservoir. Not while the flow
// calibration runs: that empties the reservoir on purpose.
void RainPumpOutput::account_(uint32_t now) {
  uint32_t ran = now - this->last_account_time_;
  this->last_account_time_ = now;
  if (!this->current_state_ || !this->reservoir_tracked_() || this->calibrating_)
    return;
  this->remaining_ml_ = std::max(this->remaining_ml_ - this->water_ml_(ran), 0.0f);
  this->publish_level_(false);
}

void RainPumpOutput::save_reservoir_() {
  ReservoirPref p{this->config_hash_, this->flow_, this->reservoir_ml_, this->remaining_ml_};
  this->pref_.save(&p);
}

// The pump on-time of a flow calibration in progress or awaiting its result, unknown otherwise.
void RainPumpOutput::publish_calibration_() {
  if (this->calibration_sens_ == nullptr)
    return;
  if (this->calibrating_ && this->current_state_)
    this->calibration_sens_->publish_state((millis() - this->run_start_) / 1000.0f);
  else if (this->calibration_runtime_ > 0)
    this->calibration_sens_->publish_state(this->calibration_runtime_ / 1000.0f);
  else
    this->calibration_sens_->publish_state(NAN);
}

// Publishes the level in % of the usable water (above the reserve), throttled to whole-percent
// changes unless forced. Unknown (NaN) while the reservoir is not tracked or being calibrated.
void RainPumpOutput::publish_level_(bool force) {
  if (this->level_sens_ == nullptr)
    return;
  if (!this->reservoir_tracked_() || this->calibrating_) {
    if (force || this->published_level_ != -2) {
      this->published_level_ = -2;
      this->level_sens_->publish_state(NAN);
    }
    return;
  }
  float usable = this->reservoir_ml_ - this->reserve_ml_();
  float pct = usable > 0.0f ? clamp(100.0f * (this->remaining_ml_ - this->reserve_ml_()) / usable, 0.0f, 100.0f) : 0.0f;
  int whole = (int) std::ceil(pct);  // 0 only when really empty
  if (!force && whole == this->published_level_)
    return;
  this->published_level_ = whole;
  this->level_sens_->publish_state(pct);
}

// The single place the pump is switched, so every run is accounted for.
void RainPumpOutput::set_pump_(bool on) {
  if (on == this->current_state_)
    return;
  uint32_t now = millis();
  if (on) {
    ESP_LOGD(TAG, "rain pump ON");
    this->run_start_ = this->last_account_time_ = now;
    this->current_state_ = true;
    this->pump_->turn_on();
    if (this->pump_state_sens_ != nullptr)
      this->pump_state_sens_->publish_state(true);
    this->high_freq_.start();
  } else {
    ESP_LOGD(TAG, "rain pump OFF after %u ms (~%.1f ml)", now - this->run_start_, this->water_ml_(now - this->run_start_));
    this->account_(now);
    this->pump_->turn_off();
    if (this->pump_state_sens_ != nullptr)
      this->pump_state_sens_->publish_state(false);
    this->current_state_ = false;
    this->last_off_ = now;
    this->ran_since_boot_ = true;
    if (this->dry_ && !this->calibrating_ && now - this->run_start_ >= this->dry_prime_ms_()) {
      this->dry_ = false;
      ESP_LOGD(TAG, "line primed after running dry");
    }
    this->high_freq_.stop();
    this->save_reservoir_();
    this->publish_level_(true);
  }
}

// Latest humidify demand from the hygrostat PID (heat_output). Stored, not applied directly;
// loop() arbitrates what the pump sprays against the weather/humidity rules.
void RainPumpOutput::write_state(float state) {
  ESP_LOGV(TAG, "humidify demand %.3f", state);
  this->humidify_demand_ = state;
}

// ------------------- Rain and misting -------------------

// mm on the floor -> ml, of which rain_share is sprayed.
float RainPumpOutput::rain_ml_per_hour_(float mm) const {
  return std::min(mm, this->max_rain_) * this->rain_share_ * this->floor_area_ * 1000.0f;
}

// The water the weather and the humidity call for, ml per hour, and the pulse length to spray it in:
//   - historic rain: its water, until humidity is 20 pts above target; each pulse the water of
//     rain_interval x sqrt(max_rain / mm), its length at the flow within min_spray .. max_spray
//   - no rain: once humidity has fallen 10 pts below target, max_misting x the hygrostat's humidify
//     demand, in pulses of min_spray
float RainPumpOutput::spray_rate_(bool &rain, uint32_t &pulse_ms) const {
  float target_h = (this->hygrostat_ != nullptr) ? this->hygrostat_->target_temperature : 0.0f;
  float cur_h = (this->humidity_sens_ != nullptr && this->humidity_sens_->has_state()) ? this->humidity_sens_->state
                                                                                       : target_h;
  float mm = (this->weather_sens_ != nullptr && this->weather_sens_->has_state() && !std::isnan(this->weather_sens_->state))
                 ? this->weather_sens_->state
                 : 0.0f;
  rain = mm > 0.0f;
  if (rain) {
    float ml_per_hour = this->rain_ml_per_hour_(mm);
    float heavy = this->max_rain_ > 0.0f ? std::min(mm / this->max_rain_, 1.0f) : 1.0f;
    float interval_h = (float) this->rain_interval_ / std::sqrt(heavy) / 3600000.0f;
    pulse_ms = clamp(this->run_ms_(ml_per_hour * interval_h), this->min_spray_, this->max_spray_);
    return cur_h < target_h + 20.0f ? ml_per_hour : 0.0f;
  }
  pulse_ms = this->min_spray_;
  if (cur_h <= target_h - 10.0f)
    return clamp(this->humidify_demand_, 0.0f, 1.0f) * this->max_misting_;
  return 0.0f;
}

void RainPumpOutput::reset_spray_() {
  this->spraying_ = false;
  this->owed_ml_ = 0;
}

// Build up the water at `ml_per_hour` and spray it in pulses of `pulse_ms` as soon as there is enough
// for one. A running pulse always finishes. With nothing to spray (no rain, or the humidity is where
// it should be) what was built up is dropped rather than sprayed later.
bool RainPumpOutput::spray_wants_pump_(uint32_t now, float ml_per_hour, uint32_t pulse_ms, bool rain) {
  if (ml_per_hour <= 0.0f)
    this->owed_ml_ = 0;
  else if (this->spraying_)
    this->owed_ml_ += ml_per_hour * (float) (now - this->spray_last_) / 3600000.0f;
  this->spray_last_ = now;
  this->spraying_ = true;

  if (this->pulse_ms_ > 0) {
    if (now - this->pulse_start_ < this->pulse_ms_)
      return true;
    this->pulse_ms_ = 0;
  }
  float ml = this->water_ml_(pulse_ms);
  if (ml_per_hour <= 0.0f || ml <= 0.0f || this->owed_ml_ < ml)
    return false;
  // An idle pump first spins up and pressurizes the line: prime it, on top of the pulse's water.
  uint32_t prime = 0;
  if (this->prime_ > 0 && !this->current_state_) {
    if (this->dry_)
      prime = this->dry_prime_ms_();
    else if (!this->ran_since_boot_ || now - this->last_off_ >= this->prime_after_)
      prime = this->prime_;
  }
  this->pulse_ms_ = pulse_ms + prime;
  this->pulse_start_ = now;
  this->owed_ml_ -= ml;
  ESP_LOGD(TAG, "%s: %u ms pulse%s, %.1f ml (%.1f ml per hour, one every %.0f s)", rain ? "rain" : "misting",
           this->pulse_ms_, prime == 0 ? "" : this->dry_ ? " (primed, line ran dry)" : " (primed)", ml, ml_per_hour, ml / ml_per_hour * 3600.0f);
  return true;
}

void RainPumpOutput::loop() {
  uint32_t now = millis();
  this->account_(now);

  // Flow calibration: after the delay the pump runs continuously, nothing else; the dry-run guard
  // ends it.
  if (this->calibrating_) {
    if (now - this->calibration_start_ < CALIBRATION_DELAY) {
      this->set_pump_(false);
      return;
    }
    this->set_pump_(true);
    if (now - this->run_start_ >= this->calibration_limit_) {
      ESP_LOGW(TAG, "flow calibration: %u s without \"Reservoir empty\" -- ended, flow unchanged, reservoir "
                    "taken as empty", (now - this->run_start_) / 1000);
      this->calibrating_ = false;
      this->set_pump_(false);
      this->remaining_ml_ = 0;
      this->dry_ = true;
      this->save_reservoir_();
      this->publish_level_(true);
      this->publish_calibration_();
    } else if (now - this->last_calibration_publish_ >= 1000) {
      this->last_calibration_publish_ = now;
      this->publish_calibration_();
    }
    this->mode_ = Mode::MODE_FLOW_CALIBRATION;
    this->rate_ml_h_ = this->current_state_ ? this->flow_ * 3600.0f : 0.0f;
    this->publish_decision_(now);
    return;
  }

  // Empty reservoir: switch the rain automation off (it stays off until the refill, which switches
  // it back on), and lock the pump out below whatever asks for it.
  bool empty = this->reservoir_empty_();
  if (empty && this->enabled_ != nullptr && *this->enabled_) {
    ESP_LOGW(TAG, "reservoir empty: rain automation off, refill it and press \"Reservoir refilled\"");
    *this->enabled_ = false;
    this->disabled_by_empty_ = true;
  }

  bool manual = false;
  if (this->manual_pending_) {
    this->manual_pending_ = false;
    this->manual_start_ = now;
  }
  if (this->manual_duration_ > 0) {
    manual = now - this->manual_start_ < this->manual_duration_;
    if (!manual || empty) {
      ESP_LOGI(TAG, empty ? "Let it rain: stopped, reservoir empty" : "Let it rain: done");
      this->manual_duration_ = 0;
      manual = false;
    }
  }

  bool automatic = this->automation_wants_pump_(now);
  this->set_pump_((automatic || manual) && !empty);

  if (empty) {
    this->mode_ = Mode::MODE_RESERVOIR_EMPTY;
    this->rate_ml_h_ = 0.0f;
  } else if (manual) {
    this->mode_ = Mode::MODE_MANUAL;  // the rate stays the automation's
  }
  this->publish_decision_(now);
}

// Whether the automation wants the pump on right now.
bool RainPumpOutput::automation_wants_pump_(uint32_t now) {
  // Master enable off: no automatic rain/humidification.
  if (this->enabled_ != nullptr && !*this->enabled_) {
    this->pwm_active_ = false;
    this->reset_spray_();
    this->pulse_ms_ = 0;
    this->mode_ = Mode::MODE_DISABLED;
    this->rate_ml_h_ = 0.0f;
    return false;
  }

  // Humidify calibration: the PID relay drives the pump straight through the slow-PWM -- the
  // autotune needs the plain on/off it asks for.
  if (this->calib_target_ != nullptr && globals::id(this->calib_target_) == CAL_HUMIDIFY) {
    this->reset_spray_();
    this->pulse_ms_ = 0;
    bool on = this->calibration_wants_pump_(now);
    this->mode_ = Mode::MODE_HUMIDIFY_CALIBRATION;
    this->rate_ml_h_ = this->duty_ * this->flow_ * 3600.0f;
    return on;
  }
  this->pwm_active_ = false;

  bool rain;
  uint32_t pulse_ms;
  float ml_per_hour = this->spray_rate_(rain, pulse_ms);
  if (rain)
    this->mode_ = ml_per_hour > 0.0f ? Mode::MODE_RAIN : Mode::MODE_RAIN_PAUSED;
  else
    this->mode_ = ml_per_hour > 0.0f ? Mode::MODE_MISTING : Mode::MODE_IDLE;
  this->rate_ml_h_ = ml_per_hour;
  return this->spray_wants_pump_(now, ml_per_hour, pulse_ms, rain);
}

const char *RainPumpOutput::mode_str_(Mode m) {
  switch (m) {
    case Mode::MODE_DISABLED:
      return "Disabled";
    case Mode::MODE_RESERVOIR_EMPTY:
      return "Reservoir empty";
    case Mode::MODE_IDLE:
      return "Idle";
    case Mode::MODE_RAIN:
      return "Rain";
    case Mode::MODE_RAIN_PAUSED:
      return "Rain (humid enough)";
    case Mode::MODE_MISTING:
      return "Misting";
    case Mode::MODE_MANUAL:
      return "Let it rain";
    case Mode::MODE_HUMIDIFY_CALIBRATION:
      return "Humidify calibration";
    case Mode::MODE_FLOW_CALIBRATION:
      return "Flow calibration";
    default:
      return "Unknown";
  }
}

// The decision of this loop pass to Home Assistant. The mode, and the rate starting or stopping, go
// out at once; the misting rate follows the hygrostat's demand continuously, so smaller changes are
// throttled: a change of 1 ml/h or more at most every 10 s, any change at most every minute.
void RainPumpOutput::publish_decision_(uint32_t now) {
  if (this->spray_mode_sens_ != nullptr && this->mode_ != this->published_mode_)
    this->spray_mode_sens_->publish_state(mode_str_(this->mode_));
  if (this->spray_rate_sens_ != nullptr) {
    float rate = std::round(this->rate_ml_h_ * 10.0f) / 10.0f;
    float last = this->published_rate_;
    uint32_t since = now - this->last_rate_publish_;
    bool publish = std::isnan(last) || this->mode_ != this->published_mode_ || ((rate > 0.0f) != (last > 0.0f)) ||
                   (std::fabs(rate - last) >= 1.0f && since >= 10 * 1000) || (rate != last && since >= 60 * 1000);
    if (publish) {
      this->published_rate_ = rate;
      this->last_rate_publish_ = now;
      this->spray_rate_sens_->publish_state(rate);
    }
  }
  this->published_mode_ = this->mode_;
}

// The slow-PWM over the humidify calibration's relay output.
bool RainPumpOutput::calibration_wants_pump_(uint32_t now) {
  // The period clock is frozen while not in use, so on entering it would otherwise still point at
  // whenever it was last used. Start a fresh period instead, at its head.
  if (!this->pwm_active_) {
    this->pwm_active_ = true;
    this->period_start_time_ = now;
    ESP_LOGD(TAG, "humidify calibration: starting a new %u ms period", this->period_);
  }

  this->duty_ = clamp(this->humidify_demand_, 0.0f, 1.0f);
  float scaled_state = this->duty_ * (float) this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGD(TAG, "now=%u period=%u End of period. duty: %.3f, on-time: %.0f ms", now, this->period_, this->duty_,
             scaled_state);
    this->period_start_time_ += this->period_;
    // More than one period behind (a starved loop): snap to now rather than crawling forward one
    // period per loop() call, which would keep the pump off for the whole catch-up.
    if (now - this->period_start_time_ > this->period_)
      this->period_start_time_ = now;
  }

  return scaled_state > now - this->period_start_time_;
}

void RainPumpOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Rain Pump Output (humidify + rain):");
  ESP_LOGCONFIG(TAG, "  Humidify calibration period: %u s", this->period_ / 1000);
  ESP_LOGCONFIG(TAG, "  Flow: %.2f ml/s, pulses of at least %u ms (%.2f ml)", this->flow_, this->min_spray_,
                this->water_ml_(this->min_spray_));
  if (this->weather_sens_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Rain: %.0f %% of the precipitation on %.2f m², at most %.1f mm/h (%.0f ml/h)",
                  this->rain_share_ * 100.0f, this->floor_area_, this->max_rain_,
                  this->rain_ml_per_hour_(this->max_rain_));
    ESP_LOGCONFIG(TAG, "  Rain pulses: every %u s at max_rain, %u .. %u ms", this->rain_interval_ / 1000,
                  this->min_spray_, this->max_spray_);
  }
  if (this->prime_ > 0)
    ESP_LOGCONFIG(TAG, "  Priming: %u ms on the first pulse after %u s idle, %u ms after running dry%s", this->prime_,
                  this->prime_after_ / 1000, this->dry_prime_ms_(), this->dry_ ? " (pending)" : "");
  ESP_LOGCONFIG(TAG, "  Misting: at most %.0f ml/h in pulses of %u ms", this->max_misting_, this->min_spray_);
  if (this->reservoir_tracked_()) {
    ESP_LOGCONFIG(TAG, "  Reservoir: %.0f ml, %.0f %% reserve, remaining %.0f ml", this->reservoir_ml_,
                  this->reserve_ * 100.0f, this->remaining_ml_);
    ESP_LOGCONFIG(TAG, "  Flow calibration limit: %u s", this->calibration_limit_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "  Reservoir: not tracked");
  }
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace geckorain
}  // namespace esphome
