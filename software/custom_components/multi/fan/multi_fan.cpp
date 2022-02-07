#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"

namespace esphome {
namespace multi {

static const char *const TAG = "multi.fan";

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MultiFan...");
    this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}

void MultiFan::dump_config() {
  ESP_LOGCONFIG(TAG, "MultiFan '%s':", this->fan_->get_name().c_str());
}

void MultiFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace multi
}  // namespace esphome
