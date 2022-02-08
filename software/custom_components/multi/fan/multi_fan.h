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


class MultiFan : public Component {
 public:
  MultiFan(fan::FanState *fan, output::FloatOutput *windspeed)
     : fan_(fan), windspeed_(windspeed) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_hygro(const PidConf &hygro) { hygro_ = hygro; }
  void set_thermo(const PidConf &thermo) { thermo_ = thermo; }

 protected:
  fan::FanState *fan_;
  output::FloatOutput *windspeed_;
  bool next_update_{true};

  PidConf hygro_{NULL};
  PidConf thermo_{NULL};
  sensor::Sensor *vicinity_{NULL};
  sensor::Sensor *target_thermo_{NULL};
};

}  // namespace multi
}  // namespace esphome
