#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace multi {

class MultiFan : public Component, public fan::Fan {
 public:
  MultiFan(output::FloatOutput *output) : output_(output) {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  output::FloatOutput *output_;
};

}  // namespace multi
}  // namespace esphome
