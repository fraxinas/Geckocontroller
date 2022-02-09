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


class MultiFan : public sensor::Sensor, public PollingComponent {
 public:
  MultiFan(fan::FanState *fan)
     : fan_(fan) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_hygro(const PidConf &hygro) { hygro_ = hygro; }
  void set_vicinity(sensor::Sensor *vicinity) { vicinity_sens_ = vicinity; }
  void set_windspeed(sensor::Sensor *windspeed) { windspeed_sens_ = windspeed; }
  void update() override;

 protected:
  void set_weighted_fan_value();
  fan::FanState *fan_;
  bool next_update_{true};
  float vicinity_temp_{0};
  float windspeed_{0};

  PidConf hygro_{NULL};
  PidConf thermo_{NULL};
  sensor::Sensor *vicinity_sens_{NULL};
  sensor::Sensor *windspeed_sens_{NULL};
  sensor::Sensor *target_thermo_{NULL};
};

}  // namespace multi
}  // namespace esphome
