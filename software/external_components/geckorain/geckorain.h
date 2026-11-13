#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace geckorain {

static const int CAL_HUMIDIFY = 4;  // calibration_target value for humidification

// Slow-PWM style float output that drives a binary pump over a fixed period, and acts as the
// hygrostat's humidify actuator. The effective duty cycle is arbitrated between historic-weather
// rain simulation and humidity control (see PID_CALIBRATION.md):
//   - weather precipitation present -> rain up to (target humidity + 20 pts)
//   - no precipitation & humidity low -> short misting pulses once humidity falls to (target - 10)
// During humidification calibration the arbitration is bypassed and the PID relay drives directly.
//
// Reservoir: the pump is fed from a bottle and must not run dry. With `reservoir_runtime` set, every
// millisecond the pump runs (automatic rain, misting, calibration and the manual "Let it rain") is
// deducted from that budget, which is kept in flash. At zero the pump is locked out and the rain
// automation switches itself off; refill_reservoir() restores the full budget and switches the
// automation back on if it was the empty reservoir that switched it off.
//
// Reservoir calibration: with a full reservoir, start_reservoir_calibration() runs the pump
// continuously; stop it once the pump draws air. The measured time less a safety margin becomes the
// reservoir runtime (kept in flash; editing `reservoir_runtime` in the YAML overrides it again).
class RainPumpOutput : public output::FloatOutput, public Component {
 public:
  void set_pump(output::BinaryOutput *pump) { this->pump_ = pump; }
  void set_period(uint32_t period) { this->period_ = period; }
  void set_weather_sensor(sensor::Sensor *s) { this->weather_sens_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sens_ = s; }
  void set_hygrostat(pid::PIDClimate *c) { this->hygrostat_ = c; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { this->calib_target_ = g; }
  // Master enable: when false, no automatic rain/humidification (the manual "Let it rain" button
  // still drives the pump directly). Defaults to enabled when not configured.
  // Bound to a `globals:` bool of either flavour: the codegen emits GlobalsComponent<bool> for
  // restore_value: no and the unrelated RestoringGlobalsComponent<bool> for yes. Both own their
  // bool for the life of the app, so store its address and stay agnostic to the choice.
  void set_enabled(globals::GlobalsComponent<bool> *g) { this->enabled_ = &globals::id(g); }
  void set_enabled(globals::RestoringGlobalsComponent<bool> *g) { this->enabled_ = &globals::id(g); }
  // Pump runtime one reservoir filling lasts until calibrated; 0 disables reservoir tracking.
  void set_reservoir_runtime(uint32_t ms) { this->configured_runtime_ = ms; }
  void set_reservoir_level_sensor(sensor::Sensor *s) { this->level_sens_ = s; }
  // Publishes the effective reservoir runtime (configured or calibrated), in seconds.
  void set_reservoir_runtime_sensor(sensor::Sensor *s) { this->runtime_sens_ = s; }

  // Manual rain (the "Let it rain" button): run the pump for `ms`, regardless of the automation.
  // Refused while the reservoir is empty.
  void run_manual(uint32_t ms);
  // The reservoir has been refilled: restore the full pump runtime.
  void refill_reservoir();

  void start_reservoir_calibration();
  void stop_reservoir_calibration();
  bool is_calibrating_reservoir() const { return this->calibrating_; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;  // hygrostat humidify demand (0..1)
  void loop() override;
  float arbitrate_duty_();
  bool automation_wants_pump_(uint32_t now);
  void set_pump_(bool on);
  void account_(uint32_t now);
  void save_reservoir_();
  void publish_level_(bool force);
  void finish_reservoir_calibration_(bool adopt);
  bool reservoir_tracked_() const { return this->configured_runtime_ > 0; }
  bool reservoir_empty_() const { return this->reservoir_tracked_() && this->reservoir_remaining_ == 0; }

  output::BinaryOutput *pump_{nullptr};
  sensor::Sensor *weather_sens_{nullptr};
  sensor::Sensor *humidity_sens_{nullptr};
  pid::PIDClimate *hygrostat_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};
  bool *enabled_{nullptr};

  float humidify_demand_{0};  // latest hygrostat humidify demand from write_state
  float duty_{0};             // current effective duty cycle (0..1) for the slow-PWM
  bool current_state_{false};
  bool was_enabled_{false};   // tracks the master-enable edge so a re-enable restarts the period
  uint32_t period_start_time_{0};
  uint32_t period_{180 * 1000};

  uint32_t manual_start_{0};
  uint32_t manual_duration_{0};  // 0 = no manual run in progress

  sensor::Sensor *level_sens_{nullptr};
  sensor::Sensor *runtime_sens_{nullptr};
  uint32_t configured_runtime_{0};   // `reservoir_runtime` from the YAML; 0 = not tracked
  uint32_t reservoir_runtime_{0};    // effective ms of pump runtime per filling (configured or calibrated)
  uint32_t reservoir_remaining_{0};  // ms of pump runtime left
  uint32_t last_account_time_{0};    // last time the running pump's runtime was deducted
  int published_level_{-1};          // last published whole percent, to throttle updates
  bool disabled_by_empty_{false};    // the empty reservoir switched the automation off
  bool calibrating_{false};
  uint32_t calibration_start_{0};
  ESPPreferenceObject pref_;

  static constexpr float CALIBRATION_MARGIN = 0.95f;            // adopted share of the measured runtime
  static constexpr uint32_t CALIBRATION_MIN = 10 * 1000;        // shorter runs count as aborted
  static constexpr uint32_t CALIBRATION_MAX = 5 * 60 * 1000;    // dry-run guard: give up after this

  // Internal duty caps preserving the original "max ~18 s rain per period" realism.
  static constexpr float WEATHER_MAX_DUTY = 0.02f;  // scheduled rain
  static constexpr float MIST_MAX_DUTY = 0.01f;     // gentle humidification pulses
};

}  // namespace geckorain
}  // namespace esphome
