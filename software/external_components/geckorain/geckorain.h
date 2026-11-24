#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace geckorain {

static const int CAL_HUMIDIFY = 4;  // calibration_target value for humidification

// Float output that drives a binary pump and acts as the hygrostat's humidify actuator. What it
// sprays is arbitrated between historic-weather rain simulation and humidity control (see
// PID_CALIBRATION.md):
//   - weather precipitation present -> rain up to (target humidity + 20 pts)
//   - no precipitation & humidity low -> misting from the humidify demand once humidity falls to
//     (target - 10), at most `max_misting`
// During humidification calibration the arbitration is bypassed and the PID relay drives the pump
// directly, slow-PWM'd over `period`.
//
// Water: everything here is reckoned in ml -- the reservoir, the rain and the misting -- at the
// pump's `flow` through the spray nozzles, the water in proportion to the pump's on-time. No pulse
// is shorter than `min_spray`. A pump idle for `prime_after` (or since boot) first has to spin up and
// pressurize the line to the nozzles: the first automatic pulse after that runs `prime` longer, not
// counted as sprayed water (it is still drawn from the reservoir). After the reservoir was marked empty
// (reservoir_empty(), also the end of a flow calibration) the line has run dry: the next prime is
// `empty_prime_factor` times as long, regardless of the idle time, until a run that long. The flow
// is measured on the device (see the flow calibration below) and kept in flash; the YAML value
// stands in until then.
//
// Rain is real water: the weather's precipitation (mm in the hour) falls on the terrarium floor,
// 1 mm on 1 m² being 1 l. Only `rain_share` of it is sprayed, and rain heavier than `max_rain` counts
// as that -- the reservoir has to last through hours of heavy rain. Heavier rain comes both in
// longer pulses and more often: at `max_rain` a pulse every `rain_interval`, lighter rain
// sqrt(max_rain / mm) times further apart, each pulse the water of its interval -- so pulse length
// and frequency both grow with the square root of the rain. The pulse length stays within
// `min_spray` .. `max_spray` (at the limits the interval gives way). The water builds up and each
// pulse goes out once there is enough for it. Misting builds up `max_misting` x the humidify
// demand and sprays it in pulses of `min_spray`. While the pump runs the loop runs at full speed,
// so short pulses end on time.
//
// Reservoir: the pump is fed from a bottle of which it can draw `reservoir_volume` (calibrated: the
// intake sits above the bottom) and must not run dry. Every pump run (automatic rain, misting and
// the manual "Let it rain") is deducted from it, kept in flash. Once only `reserve` is left the
// pump is locked out and the rain automation switches itself off; refill_reservoir() fills it up
// again and switches the automation back on if it was the empty reservoir that switched it off.
// reservoir_volume 0 disables the tracking.
//
// Flow calibration: with a full reservoir, start_reservoir_calibration() runs the pump -- after
// CALIBRATION_DELAY, time to get the measuring cups in place -- continuously through the spray
// nozzles, everything else held off; reservoir_empty() once the reservoir is empty stops it and
// keeps the runtime. calibrate_flow() with the ml in the cups then sets both the flow (ml over the
// runtime) and the reservoir volume (what the pump draws from one filling).
// stop_reservoir_calibration() aborts. Past `calibration_limit` the run ends by itself and the
// reservoir counts as empty. Not resumed after a reboot.
class RainPumpOutput : public output::FloatOutput, public Component {
 public:
  void set_pump(output::BinaryOutput *pump) { this->pump_ = pump; }
  void set_period(uint32_t period) { this->period_ = period; }
  // ml/s through the spray nozzles until calibrated.
  void set_flow(float ml_per_s) { this->flow_ = ml_per_s; }
  void set_min_spray(uint32_t ms) { this->min_spray_ = ms; }
  void set_max_spray(uint32_t ms) { this->max_spray_ = ms; }
  // After `prime_after` of idle pump (and after a boot) the first automatic pulse runs `prime` longer.
  void set_prime(uint32_t ms) { this->prime_ = ms; }
  void set_prime_after(uint32_t ms) { this->prime_after_ = ms; }
  // After the reservoir was marked empty (the line ran dry), the next prime is this many times `prime`.
  void set_empty_prime_factor(float f) { this->empty_prime_factor_ = f; }
  // The weather's precipitation, mm in the hour.
  void set_weather_sensor(sensor::Sensor *s) { this->weather_sens_ = s; }
  void set_floor_area(float m2) { this->floor_area_ = m2; }
  void set_rain_share(float share) { this->rain_share_ = share; }
  void set_max_rain(float mm) { this->max_rain_ = mm; }
  void set_rain_interval(uint32_t ms) { this->rain_interval_ = ms; }
  void set_max_misting(float ml_per_hour) { this->max_misting_ = ml_per_hour; }
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
  // Litres the pump draws from one filling until calibrated; 0 disables reservoir tracking.
  void set_reservoir_volume(float litres) { this->reservoir_ml_ = litres * 1000.0f; }
  void set_reserve(float share) { this->reserve_ = share; }
  // Changes whenever flow or reservoir_volume are edited in the YAML: drops a stored calibration.
  void set_config_hash(uint32_t h) { this->config_hash_ = h; }
  void set_reservoir_level_sensor(sensor::Sensor *s) { this->level_sens_ = s; }
  // Publish the flow (ml/s) and the reservoir volume (ml) in force, calibrated or from the YAML.
  void set_flow_sensor(sensor::Sensor *s) { this->flow_sens_ = s; }
  void set_volume_sensor(sensor::Sensor *s) { this->volume_sens_ = s; }
  // Publishes the pump on-time of a flow calibration in progress (or awaiting its result), in s.
  void set_calibration_sensor(sensor::Sensor *s) { this->calibration_sens_ = s; }
  void set_calibration_limit(uint32_t ms) { this->calibration_limit_ = ms; }

  // Manual rain (the "Let it rain" button): run the pump for `ms`, regardless of the automation.
  // Refused while the reservoir is empty or a calibration runs.
  void run_manual(uint32_t ms);
  // The reservoir has been refilled: back to full.
  void refill_reservoir();
  // The reservoir is empty: ends a flow calibration run, keeping its runtime for the result;
  // otherwise just locks out the pump as if it had run empty.
  void reservoir_empty();

  void start_reservoir_calibration();
  void stop_reservoir_calibration();
  bool is_calibrating_reservoir() const { return this->calibrating_; }
  // The flow calibration's run sprayed `ml` in all: sets the flow and the reservoir volume.
  void calibrate_flow(float ml);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;  // hygrostat humidify demand (0..1)
  void loop() override;
  bool automation_wants_pump_(uint32_t now);
  // Water per hour (ml) the rain or the misting calls for, and the pulse length to spray it in.
  float spray_rate_(bool &rain, uint32_t &pulse_ms) const;
  // Whether a spray pulse runs now, water building up at `ml_per_hour`.
  bool spray_wants_pump_(uint32_t now, float ml_per_hour, uint32_t pulse_ms, bool rain);
  // Humidify calibration: the slow-PWM over the PID relay output.
  bool calibration_wants_pump_(uint32_t now);
  // Water the precipitation calls for, ml per hour.
  float rain_ml_per_hour_(float mm) const;
  // Water of a pump run of `ms`, and the run for `ml`.
  float water_ml_(uint32_t ms) const { return this->flow_ * (float) ms / 1000.0f; }
  uint32_t run_ms_(float ml) const { return this->flow_ > 0.0f ? (uint32_t) (ml / this->flow_ * 1000.0f) : 0; }
  void reset_spray_();
  void set_pump_(bool on);
  void account_(uint32_t now);
  void save_reservoir_();
  void publish_level_(bool force);
  void publish_model_();
  void publish_calibration_();
  void release_empty_lockout_();
  bool reservoir_tracked_() const { return this->reservoir_ml_ > 0.0f; }
  float reserve_ml_() const { return this->reserve_ * this->reservoir_ml_; }
  // A calibration knows no empty reservoir until told so.
  bool reservoir_empty_() const {
    return this->reservoir_tracked_() && !this->calibrating_ && this->remaining_ml_ <= this->reserve_ml_();
  }

  output::BinaryOutput *pump_{nullptr};
  sensor::Sensor *weather_sens_{nullptr};
  sensor::Sensor *humidity_sens_{nullptr};
  pid::PIDClimate *hygrostat_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};
  bool *enabled_{nullptr};

  float flow_{2.66f};         // ml/s through the nozzles
  uint32_t min_spray_{200};   // ms, shortest pulse
  uint32_t max_spray_{3000};  // ms, longest rain pulse
  uint32_t prime_{0};                    // ms added to the first pulse after an idle spell; 0 = off
  uint32_t prime_after_{60 * 60 * 1000};  // ms of idle pump after which the next pulse primes
  uint32_t last_off_{0};                 // when the pump was last switched off
  bool ran_since_boot_{false};           // last_off_ is valid
  float empty_prime_factor_{6.0f};       // prime x this after the reservoir ran dry
  bool dry_{false};                      // the line ran dry, no run long enough to prime it since
  uint32_t dry_prime_ms_() const { return (uint32_t) (this->prime_ * this->empty_prime_factor_); }

  float humidify_demand_{0};  // latest hygrostat humidify demand from write_state
  float duty_{0};             // the calibration's slow-PWM duty
  bool current_state_{false};
  bool pwm_active_{false};    // the calibration's slow-PWM runs; entering it restarts the period
  uint32_t period_start_time_{0};
  uint32_t period_{180 * 1000};

  float floor_area_{0.24f};        // m²
  float rain_share_{0.15f};        // share of the historic precipitation sprayed
  float max_rain_{7.6f};           // mm in the hour; heavier rain counts as this
  uint32_t rain_interval_{30000};  // ms between rain pulses at max_rain
  float max_misting_{120.0f};      // ml per hour at full humidify demand
  float owed_ml_{0};               // water built up, not sprayed yet
  uint32_t spray_last_{0};         // last time water was added
  bool spraying_{false};           // spray_last_ is valid
  uint32_t pulse_start_{0};
  uint32_t pulse_ms_{0};           // length of the running pulse; 0 = none

  uint32_t manual_start_{0};
  uint32_t manual_duration_{0};  // 0 = no manual run in progress
  bool manual_pending_{false};   // requested, starts on the next loop pass

  sensor::Sensor *level_sens_{nullptr};
  sensor::Sensor *flow_sens_{nullptr};
  sensor::Sensor *volume_sens_{nullptr};
  float reservoir_ml_{0};            // what the pump draws from one filling; 0 = not tracked
  float reserve_{0.10f};             // share of the filling kept back: "empty" from there
  float remaining_ml_{0};
  uint32_t config_hash_{0};
  uint32_t last_account_time_{0};    // last time the running pump was accounted
  uint32_t run_start_{0};            // when the running pump was switched on
  HighFrequencyLoopRequester high_freq_;  // while the pump runs: short pulses end on time
  int published_level_{-1};          // last published whole percent, to throttle updates (-2: unknown)
  bool disabled_by_empty_{false};    // the empty reservoir switched the automation off
  sensor::Sensor *calibration_sens_{nullptr};
  bool calibrating_{false};          // flow calibration: the pump runs continuously
  uint32_t calibration_runtime_{0};  // ms the last flow calibration ran, awaiting its result; 0 = none
  uint32_t calibration_start_{0};    // when the flow calibration was switched on
  uint32_t calibration_limit_{10 * 60 * 1000};  // dry-run guard: the calibration ends here
  uint32_t last_calibration_publish_{0};
  ESPPreferenceObject pref_;

  static constexpr uint32_t CALIBRATION_MIN = 10 * 1000;   // less on-time is no measurement
  static constexpr uint32_t CALIBRATION_DELAY = 5 * 1000;  // from switching on to the pump starting
};

}  // namespace geckorain
}  // namespace esphome
