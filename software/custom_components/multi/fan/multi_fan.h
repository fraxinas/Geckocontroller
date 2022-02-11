#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/fan/fan_state.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace multi {

struct PidConf {
  pid::PIDClimate *component;
  float weight;
};


class MultiFan : public Component, public sensor::Sensor, public output::FloatOutput {
 public:
  MultiFan(fan::FanState *fan, output::FloatOutput *output)
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

  fan::FanState *fan_;
  output::FloatOutput *output_;
  PidConf hygro_{NULL};
  PidConf thermo_{NULL};
  sensor::Sensor *vicinity_sens_{NULL};
  sensor::Sensor *windspeed_sens_{NULL};
  bool next_update_{true};
  bool enable_auto_mode_{true};
};

}  // namespace multi
}  // namespace esphome
