#include "vcnl4040.h"

namespace esphome {
namespace vcnl4040 {

static const char *const TAG = "vcnl4040.sensor";

float VCNL4040Component::get_setup_priority() const {
    return setup_priority::DATA;
}

void VCNL4040Component::setup() {
    this->vcnl4040_ = Adafruit_VCNL4040();
    if (!this->vcnl4040_.begin()) {
      while (1);
    }
  }

void VCNL4040Component::update() {
    uint16_t lux = this->vcnl4040_.getLux();

    ESP_LOGD(TAG, "Update illuminance=%u lux", lux);

    if (this->lux_sensor_ != nullptr)
      this->lux_sensor_->publish_state(lux);

    // int prox = this->vcnl4040_.getProximity();
    // prox_sensor->publish_state(prox);

    // int lux = this->vcnl4040_.getLux();
    // lux_sensor->publish_state(lux);

    // int raw = this->vcnl4040_.getWhiteLight();
    // rawLight_sensor->publish_state(raw);
}

void VCNL4040Component::dump_config() {
  ESP_LOGCONFIG(TAG, "VCNL4040:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Illuminance", this->lux_sensor_);
}

}  // namespace vcnl4040
}  // namespace esphome
