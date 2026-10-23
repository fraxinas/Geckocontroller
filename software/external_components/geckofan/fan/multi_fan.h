#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/pid/pid_climate.h"
#include "esphome/components/globals/globals_component.h"

namespace esphome {
namespace geckofan {

// calibration_target global values (see PID_CALIBRATION.md). The fan is the actuator for
// cooling (3) and dehumidification (5); during those calibrations it passes the raw PID
// relay output straight through to the PWM instead of using the combined formula.
static const int CAL_COOL = 3;
static const int CAL_DEHUMID = 5;

// Concrete fan entity that the user toggles in Home Assistant. MultiFan reads its
// state/speed and drives the actual PWM output accordingly. Auto/manual is a separate
// switch (the multifan_auto global), not the fan's oscillation flag.
class StatusFan : public fan::Fan {
 public:
  fan::FanTraits get_traits() override { return fan::FanTraits(false, true, false, 100); }

 protected:
  void control(const fan::FanCall &call) override {
    if (call.get_state().has_value())
      this->state = *call.get_state();
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

  void set_hygro(pid::PIDClimate *hygro);
  void set_thermo(pid::PIDClimate *thermo);
  void set_vicinity(sensor::Sensor *vicinity) { vicinity_sens_ = vicinity; }
  void set_windspeed(sensor::Sensor *windspeed) { windspeed_sens_ = windspeed; }
  // Tacho (pulse_counter) to re-sample promptly when the fan speed changes, instead of waiting
  // out its slow update_interval. Held as PollingComponent so we can call update() on demand.
  void set_tacho(PollingComponent *tacho) { tacho_ = tacho; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { calib_target_ = g; }
  // Bound to a `globals:` bool of either flavour: the codegen emits GlobalsComponent<bool> for
  // restore_value: no and the unrelated RestoringGlobalsComponent<bool> for yes. Both own their
  // bool for the life of the app, so store its address and stay agnostic to the choice.
  void set_auto(globals::GlobalsComponent<bool> *g) { auto_global_ = &globals::id(g); }
  void set_auto(globals::RestoringGlobalsComponent<bool> *g) { auto_global_ = &globals::id(g); }

 protected:
  void set_combined_fan_value();
  void update_from_pid_(bool hygro_action);
  bool get_vicinity_temperature(float *vicinity);
  float get_windspeed();
  void write_state(float state) override;

  fan::Fan *fan_;
  output::FloatOutput *output_;
  pid::PIDClimate *hygro_{nullptr};
  pid::PIDClimate *thermo_{nullptr};
  sensor::Sensor *vicinity_sens_{NULL};
  sensor::Sensor *windspeed_sens_{NULL};
  PollingComponent *tacho_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};
  bool *auto_global_{nullptr};
  bool next_update_{true};
  bool enable_auto_mode_{true};

  // Tacho re-sampling: when the applied PWM level changes, kick the tacho to publish a fresh
  // reading. TACHO_SAMPLE_MS is the window after the change over which the new speed is counted
  // (long enough for the fan to settle and accumulate countable pulses).
  static constexpr uint32_t TACHO_SAMPLE_MS = 5000;
  float last_applied_level_{0.0f};
  bool have_applied_{false};

  int calibration_target_() { return this->calib_target_ ? globals::id(this->calib_target_) : 0; }
  bool calibrating_fan_() {
    int t = this->calibration_target_();
    return t == CAL_COOL || t == CAL_DEHUMID;
  }
  // Auto mode (PID drives the fan) vs manual (HA slider drives the fan); defaults to auto.
  bool auto_mode_() { return this->auto_global_ ? *this->auto_global_ : true; }
};

}  // namespace geckofan
}  // namespace esphome
