#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace geckorain {

// Slow-PWM style float output that drives a binary pump output over a fixed period.
// A float level of e.g. 0.02 with a 30 min period turns the pump on for ~36 s per period.
class RainPumpOutput : public output::FloatOutput, public Component {
 public:
  void set_pump(output::BinaryOutput *pump) { this->pump_ = pump; }
  void set_period(uint32_t period) { this->period_ = period; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  void loop() override;

  output::BinaryOutput *pump_{nullptr};
  float state_{0};
  bool current_state_{false};
  uint32_t period_start_time_{0};
  uint32_t period_{180 * 1000};
};

}  // namespace geckorain
}  // namespace esphome
