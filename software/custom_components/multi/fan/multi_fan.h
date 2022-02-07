#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace multi {

class MultiFan : public Component {
 public:
  MultiFan(fan::FanState *fan, output::FloatOutput *output)
     : fan_(fan), output_(output) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  fan::FanState *fan_;
  output::FloatOutput *output_;
  bool next_update_{true};
};

}  // namespace multi
}  // namespace esphome
