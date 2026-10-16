#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace geckopid {

// One tuned parameter set per direction, with the vicinity temperature recorded at tune time
// (cooling/heating effectiveness depends on how far the vicinity temperature was from target).
struct PidSet {
  float kp;
  float ki;
  float kd;
  float vicinity;
  bool valid;
};

struct CalibData {
  // Heating has two physically distinct actuators sharing one PID controller: the daylight lamp
  // (daytime, primary) and the heat lamp (night, plus daytime supplement). Each is tuned and
  // stored separately; geckopid swaps the thermostat's params on the day<->night transition.
  PidSet heat_day;
  PidSet heat_night;
  PidSet cool;
  PidSet humid;
  PidSet dehumid;
  uint32_t magic;
};

// Calibration targets (also written to the shared calibration_target global so actuators
// switch to pass-through). 1 heat_day · 2 heat_night · 3 cool · 4 humidify · 5 dehumidify.
class GeckoPid : public Component {
 public:
  void set_thermostat(pid::PIDClimate *c) { this->thermostat_ = c; }
  void set_hygrostat(pid::PIDClimate *c) { this->hygrostat_ = c; }
  void set_vicinity_sensor(sensor::Sensor *s) { this->vicinity_sens_ = s; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { this->calib_target_ = g; }
  void set_is_daytime(globals::RestoringGlobalsComponent<bool> *g) { this->is_daytime_ = g; }
  void set_noiseband(float n) { this->noiseband_ = n; }

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_selected_target(int t) { this->selected_ = t; }
  void start_calibration();
  void abort_calibration();
  bool is_calibrating() { return this->active_ != 0; }

 protected:
  PidSet *set_for_(int target);
  pid::PIDClimate *climate_for_(int target);
  void apply_set_(pid::PIDClimate *c, const PidSet &s);
  void runtime_swap_();
  void finish_(bool ok);
  void save_();

  pid::PIDClimate *thermostat_{nullptr};
  pid::PIDClimate *hygrostat_{nullptr};
  sensor::Sensor *vicinity_sens_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};
  globals::RestoringGlobalsComponent<bool> *is_daytime_{nullptr};
  float noiseband_{0.5f};

  CalibData data_{};
  ESPPreferenceObject pref_;

  int selected_{1};
  int active_{0};  // target currently being calibrated, 0 == idle
  pid::PIDClimate *active_climate_{nullptr};
  float baseline_kp_{0};
  uint32_t start_ms_{0};
  uint32_t timeout_ms_{0};
  climate::ClimateMode saved_mode_{climate::CLIMATE_MODE_OFF};

  climate::ClimateAction last_therm_{climate::CLIMATE_ACTION_OFF};
  climate::ClimateAction last_hygro_{climate::CLIMATE_ACTION_OFF};
  bool last_daytime_{true};  // tracks is_daytime so the heating set re-swaps on day<->night
};

}  // namespace geckopid
}  // namespace esphome
