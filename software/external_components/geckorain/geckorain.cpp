#include "geckorain.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace geckorain {

static const char *const TAG = "geckorain";

void RainPumpOutput::setup() {
  this->turn_off();
  this->current_state_ = false;
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
  this->duty_ = this->arbitrate_duty_();
  float scaled_state = this->duty_ * (float) this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGD(TAG, "now=%u period=%u End of period. duty: %.3f, on-time: %.0f ms", now, this->period_, this->duty_,
             scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
    if (!this->current_state_) {
      ESP_LOGD(TAG, "Turn on rain pump (duty %.3f)", this->duty_);
      this->pump_->turn_on();
      this->current_state_ = true;
    }
    return;
  } else if (this->current_state_) {
    ESP_LOGD(TAG, "Turn off rain pump");
    this->pump_->turn_off();
    this->current_state_ = false;
  }
}

void RainPumpOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Rain Pump Output (humidify + rain):");
  int h, m, s;
  h = (this->period_ / 3600000);
  m = (this->period_ / 1000 - (3600 * h)) / 60;
  s = (this->period_ / 1000 - (3600 * h) - (m * 60));
  ESP_LOGCONFIG(TAG, "  Period: %02d:%02d:%02d", h, m, s);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace geckorain
}  // namespace esphome
