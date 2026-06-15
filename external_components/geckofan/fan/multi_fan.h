#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace geckofan {

struct PidConf {
  pid::PIDClimate *component;
  float weight;
};

// Concrete fan entity that the user toggles in Home Assistant. MultiFan reads its
// state/speed/oscillating and drives the actual PWM output accordingly.
class StatusFan : public fan::Fan {
 public:
  fan::FanTraits get_traits() override { return fan::FanTraits(true, true, false, 100); }

 protected:
  void control(const fan::FanCall &call) override {
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_oscillating().has_value())
      this->oscillating = *call.get_oscillating();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();
    this->publish_state();
  }
};


class MultiFan : public Component, public sensor::Sensor, public output::FloatOutput {
 public:
  MultiFan(fan::Fan *fan, output::FloatOutput *output)
     : fan_(fan), output_(output) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_hygro(const PidConf &hygro);
  void set_thermo(const PidConf &thermo);
  void set_vicinity(sensor::Sensor *vicinity) { vicinity_sens_ = vicinity; }
  void set_windspeed(sensor::Sensor *windspeed) { windspeed_sens_ = windspeed; }

 protected:
  void set_weighted_fan_value();
  void update_from_pid_(bool hygro_action);
  bool get_vicinity_temperature(float *vicinity);
  float get_windspeed();
  void write_state(float state) override;

  fan::Fan *fan_;
  output::FloatOutput *output_;
  PidConf hygro_{NULL};
  PidConf thermo_{NULL};
  sensor::Sensor *vicinity_sens_{NULL};
  sensor::Sensor *windspeed_sens_{NULL};
  bool next_update_{true};
  bool enable_auto_mode_{true};
};

}  // namespace geckofan
}  // namespace esphome
