#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/speed/fan/speed_fan.h"

namespace esphome {
namespace multi {

static const char *const TAG = "multifan";

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up multifan...");
}

void MultiFan::dump_config() {
  LOG_DISPLAY("", "Multifan", this);
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace multi
}  // namespace esphome
