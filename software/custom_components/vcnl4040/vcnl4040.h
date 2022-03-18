#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "Adafruit_VCNL4040.h"

namespace esphome {
namespace vcnl4040 {


using namespace esphome;

class VCNL4040Component : public PollingComponent, public i2c::I2CDevice {
  public:
    void set_lux_sensor(sensor::Sensor *lux_sensor) { lux_sensor_ = lux_sensor; }

    float get_setup_priority() const override;
    void setup() override;
    void dump_config() override;
    void update() override;

  protected:
    sensor::Sensor *lux_sensor_;
    Adafruit_VCNL4040 vcnl4040_;

};

}  // namespace vcnl4040
}  // namespace esphome
