#include "geckorain.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace geckorain {

static const char *const TAG = "geckorain";

void RainPumpOutput::setup() {
  this->turn_off();
  this->current_state_ = false;
}

void RainPumpOutput::loop() {
  uint32_t now = millis();
  float scaled_state = this->state_ * (float) this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGD(TAG, "now=%u period=%u End of period. State: %.2f, Scaled state: %.0f",
             now, this->period_, this->state_, scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
    if (!this->current_state_) {
      ESP_LOGD(TAG, "Turn on rain pump");
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
  ESP_LOGCONFIG(TAG, "Rain Pump Output:");
  int h, m, s;
  h = (this->period_ / 3600000);
  m = (this->period_ / 1000 - (3600 * h)) / 60;
  s = (this->period_ / 1000 - (3600 * h) - (m * 60));
  ESP_LOGCONFIG(TAG, "  Period: %02d:%02d:%02d", h, m, s);
  LOG_FLOAT_OUTPUT(this);
}

void RainPumpOutput::write_state(float state) {
  ESP_LOGD(TAG, "RainPumpOutput write_state %.2f", state);
  this->state_ = state;
}

}  // namespace geckorain
}  // namespace esphome
