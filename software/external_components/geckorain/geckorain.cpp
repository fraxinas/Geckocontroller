#include "geckorain.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace geckorain {

static const char *const TAG = "geckorain";

static const uint32_t PREF_HASH = 0x6752'6532;  // "gRe2"

struct ReservoirPref {
  uint32_t configured;  // the YAML reservoir_runtime this was stored under
  uint32_t runtime;     // effective runtime per filling (configured or calibrated)
  uint32_t remaining;
};

void RainPumpOutput::setup() {
  this->turn_off();
  this->current_state_ = false;
  this->period_start_time_ = millis();

  if (this->reservoir_tracked_()) {
    this->pref_ = global_preferences->make_preference<ReservoirPref>(PREF_HASH);
    ReservoirPref p;
    if (!this->pref_.load(&p)) {
      ESP_LOGI(TAG, "no stored reservoir level, assuming a full reservoir");
      this->reservoir_runtime_ = this->reservoir_remaining_ = this->configured_runtime_;
    } else if (p.configured != this->configured_runtime_) {
      // reservoir_runtime was edited in the YAML: that wins over an earlier calibration.
      ESP_LOGI(TAG, "reservoir_runtime changed, dropping the calibrated %u ms", p.runtime);
      this->reservoir_runtime_ = this->configured_runtime_;
      this->reservoir_remaining_ = std::min(p.remaining, this->reservoir_runtime_);
    } else {
      this->reservoir_runtime_ = p.runtime;
      this->reservoir_remaining_ = std::min(p.remaining, this->reservoir_runtime_);
    }
    if (this->runtime_sens_ != nullptr)
      this->runtime_sens_->publish_state(this->reservoir_runtime_ / 1000.0f);
  }
  this->publish_level_(true);
}

void RainPumpOutput::start_reservoir_calibration() {
  if (!this->reservoir_tracked_()) {
    ESP_LOGW(TAG, "reservoir calibration refused: reservoir tracking is off (reservoir_runtime: 0s)");
    return;
  }
  if (this->calibrating_)
    return;
  ESP_LOGI(TAG, "reservoir calibration: pump on, stop it once the pump draws air");
  this->manual_duration_ = 0;
  this->calibrating_ = true;
  this->calibration_start_ = millis();
}

void RainPumpOutput::stop_reservoir_calibration() {
  if (this->calibrating_)
    this->finish_reservoir_calibration_(true);
}

void RainPumpOutput::finish_reservoir_calibration_(bool adopt) {
  uint32_t measured = millis() - this->calibration_start_;
  this->calibrating_ = false;
  this->set_pump_(false);
  if (!adopt) {
    ESP_LOGW(TAG, "reservoir calibration: gave up after %u ms, pump off, runtime unchanged", measured);
    return;
  }
  if (measured < CALIBRATION_MIN) {
    ESP_LOGW(TAG, "reservoir calibration: %u ms is too short, runtime unchanged", measured);
    return;
  }
  this->reservoir_runtime_ = (uint32_t) (measured * CALIBRATION_MARGIN);
  this->reservoir_remaining_ = 0;  // the calibration has emptied the reservoir
  this->save_reservoir_();
  this->publish_level_(true);
  if (this->runtime_sens_ != nullptr)
    this->runtime_sens_->publish_state(this->reservoir_runtime_ / 1000.0f);
  ESP_LOGI(TAG, "reservoir calibration: measured %u ms, reservoir runtime now %u ms; refill and press "
                "\"Reservoir refilled\"", measured, this->reservoir_runtime_);
}

void RainPumpOutput::run_manual(uint32_t ms) {
  if (this->calibrating_)
    return;
  if (this->reservoir_empty_()) {
    ESP_LOGW(TAG, "Let it rain refused: reservoir empty, refill it and press \"Reservoir refilled\"");
    return;
  }
  ESP_LOGI(TAG, "Let it rain: pump on for %u ms", ms);
  this->manual_start_ = millis();
  this->manual_duration_ = ms;
}

void RainPumpOutput::refill_reservoir() {
  if (!this->reservoir_tracked_()) {
    ESP_LOGW(TAG, "reservoir refilled, but reservoir tracking is off (reservoir_runtime: 0s)");
    return;
  }
  // Settle the runtime of a pump that is running right now, so it is not deducted from the new filling.
  this->account_(millis());
  this->reservoir_remaining_ = this->reservoir_runtime_;
  this->save_reservoir_();
  this->publish_level_(true);
  ESP_LOGI(TAG, "reservoir refilled: %u ms of pump runtime", this->reservoir_remaining_);
  if (this->disabled_by_empty_) {
    this->disabled_by_empty_ = false;
    if (this->enabled_ != nullptr) {
      ESP_LOGI(TAG, "re-enabling the rain automation the empty reservoir had switched off");
      *this->enabled_ = true;
    }
  }
}

// Deduct the runtime of the running pump since the last call from the reservoir.
void RainPumpOutput::account_(uint32_t now) {
  if (!this->current_state_ || !this->reservoir_tracked_()) {
    this->last_account_time_ = now;
    return;
  }
  uint32_t ran = now - this->last_account_time_;
  this->last_account_time_ = now;
  this->reservoir_remaining_ = ran >= this->reservoir_remaining_ ? 0 : this->reservoir_remaining_ - ran;
  this->publish_level_(false);
}

void RainPumpOutput::save_reservoir_() {
  if (!this->reservoir_tracked_())
    return;
  ReservoirPref p{this->configured_runtime_, this->reservoir_runtime_, this->reservoir_remaining_};
  this->pref_.save(&p);
}

// Publishes the level in %, throttled to whole-percent changes unless forced.
void RainPumpOutput::publish_level_(bool force) {
  if (this->level_sens_ == nullptr)
    return;
  if (!this->reservoir_tracked_()) {
    if (force)
      this->level_sens_->publish_state(NAN);
    return;
  }
  float pct = 100.0f * (float) this->reservoir_remaining_ / (float) this->reservoir_runtime_;
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
    this->last_account_time_ = now;
    this->pump_->turn_on();
    this->current_state_ = true;
  } else {
    this->account_(now);
    this->pump_->turn_off();
    this->current_state_ = false;
    this->save_reservoir_();
    this->publish_level_(true);
  }
  ESP_LOGD(TAG, "rain pump %s", on ? "ON" : "OFF");
}

// Latest humidify demand from the hygrostat PID (heat_output). Stored, not applied directly;
// loop() arbitrates the actual pump duty against the weather/humidity rules.
void RainPumpOutput::write_state(float state) {
  ESP_LOGV(TAG, "humidify demand %.3f", state);
  this->humidify_demand_ = state;
}

float RainPumpOutput::arbitrate_duty_() {
  // Calibration bypass: drive the slow-PWM straight from the PID relay output.
  if (this->calib_target_ != nullptr && globals::id(this->calib_target_) == CAL_HUMIDIFY) {
    return clamp(this->humidify_demand_, 0.0f, 1.0f);
  }

  float precip = (this->weather_sens_ != nullptr && this->weather_sens_->has_state()) ? this->weather_sens_->state
                                                                                      : 0.0f;
  float target_h = (this->hygrostat_ != nullptr) ? this->hygrostat_->target_temperature : 0.0f;
  float cur_h = (this->humidity_sens_ != nullptr && this->humidity_sens_->has_state()) ? this->humidity_sens_->state
                                                                                       : target_h;

  if (precip > 0.0f) {
    // Scheduled historic rain: rain proportionally, but stop once humidity is 20 pts above target.
    if (cur_h < target_h + 20.0f) {
      return clamp(precip / 5.0f, 0.0f, 1.0f) * WEATHER_MAX_DUTY;
    }
    return 0.0f;
  }

  // No scheduled rain: only humidify once humidity has fallen 10 pts below target, gently.
  if (cur_h <= target_h - 10.0f) {
    return clamp(this->humidify_demand_, 0.0f, 1.0f) * MIST_MAX_DUTY;
  }
  return 0.0f;
}

void RainPumpOutput::loop() {
  uint32_t now = millis();
  this->account_(now);

  // Empty reservoir: switch the rain automation off (it stays off until the refill, which switches
  // it back on), and lock the pump out below whatever asks for it.
  bool empty = this->reservoir_empty_();
  if (empty && this->enabled_ != nullptr && *this->enabled_) {
    ESP_LOGW(TAG, "reservoir empty: rain automation off, refill it and press \"Reservoir refilled\"");
    *this->enabled_ = false;
    this->disabled_by_empty_ = true;
  }

  bool manual = false;
  if (this->manual_duration_ > 0) {
    manual = now - this->manual_start_ < this->manual_duration_;
    if (!manual || empty) {
      ESP_LOGI(TAG, empty ? "Let it rain: stopped, reservoir empty" : "Let it rain: done");
      this->manual_duration_ = 0;
      manual = false;
    }
  }

  bool automatic = this->automation_wants_pump_(now);

  // Reservoir calibration overrides everything, including the empty lockout: the reservoir has just
  // been filled, and the remaining runtime is meaningless until the calibration has measured it.
  if (this->calibrating_) {
    if (now - this->calibration_start_ >= CALIBRATION_MAX) {
      this->finish_reservoir_calibration_(false);
    } else {
      this->set_pump_(true);
    }
    return;
  }
  this->set_pump_((automatic || manual) && !empty);
}

// The slow-PWM over the arbitrated duty: whether the automation wants the pump on right now.
bool RainPumpOutput::automation_wants_pump_(uint32_t now) {
  // Master enable off: no automatic rain/humidification.
  if (this->enabled_ != nullptr && !*this->enabled_) {
    this->was_enabled_ = false;
    return false;
  }

  // The period clock is frozen while disabled (we return above), so on the off->on edge it would
  // otherwise still point at whenever the automation was last on. Start a fresh period instead:
  // switching the automation on then puts the pump at the head of a period, so any rain the
  // weather calls for starts right away rather than up to `period_` later.
  if (!this->was_enabled_) {
    this->was_enabled_ = true;
    this->period_start_time_ = now;
    ESP_LOGD(TAG, "rain enabled: starting a new %u ms period", this->period_);
  }

  this->duty_ = this->arbitrate_duty_();
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
  int h, m, s;
  h = (this->period_ / 3600000);
  m = (this->period_ / 1000 - (3600 * h)) / 60;
  s = (this->period_ / 1000 - (3600 * h) - (m * 60));
  ESP_LOGCONFIG(TAG, "  Period: %02d:%02d:%02d", h, m, s);
  if (this->reservoir_tracked_()) {
    ESP_LOGCONFIG(TAG, "  Reservoir runtime: %u ms (configured %u ms), remaining: %u ms", this->reservoir_runtime_,
                  this->configured_runtime_, this->reservoir_remaining_);
  } else {
    ESP_LOGCONFIG(TAG, "  Reservoir: not tracked");
  }
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace geckorain
}  // namespace esphome
