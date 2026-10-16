#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace geckorain {

static const int CAL_HUMIDIFY = 3;  // calibration_target value for humidification

// Slow-PWM style float output that drives a binary pump over a fixed period, and acts as the
// hygrostat's humidify actuator. The effective duty cycle is arbitrated between historic-weather
// rain simulation and humidity control (see PID_CALIBRATION.md):
//   - weather precipitation present -> rain up to (target humidity + 20 pts)
//   - no precipitation & humidity low -> short misting pulses once humidity falls to (target - 10)
// During humidification calibration the arbitration is bypassed and the PID relay drives directly.
class RainPumpOutput : public output::FloatOutput, public Component {
 public:
  void set_pump(output::BinaryOutput *pump) { this->pump_ = pump; }
  void set_period(uint32_t period) { this->period_ = period; }
  void set_weather_sensor(sensor::Sensor *s) { this->weather_sens_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sens_ = s; }
  void set_hygrostat(pid::PIDClimate *c) { this->hygrostat_ = c; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { this->calib_target_ = g; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;  // hygrostat humidify demand (0..1)
  void loop() override;
  float arbitrate_duty_();

  output::BinaryOutput *pump_{nullptr};
  sensor::Sensor *weather_sens_{nullptr};
  sensor::Sensor *humidity_sens_{nullptr};
  pid::PIDClimate *hygrostat_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};

  float humidify_demand_{0};  // latest hygrostat humidify demand from write_state
  float duty_{0};             // current effective duty cycle (0..1) for the slow-PWM
  bool current_state_{false};
  uint32_t period_start_time_{0};
  uint32_t period_{180 * 1000};

  // Internal duty caps preserving the original "max ~18 s rain per period" realism.
  static constexpr float WEATHER_MAX_DUTY = 0.02f;  // scheduled rain
  static constexpr float MIST_MAX_DUTY = 0.01f;     // gentle humidification pulses
};

}  // namespace geckorain
}  // namespace esphome
