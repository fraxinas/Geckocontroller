#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/sensor/sensor.h"
#include <map>
#include <string>

namespace esphome {
namespace geckoweather {

// One resolved weather behaviour: how bright the terrarium looks under this condition
// (attenuation, 0..1), the rain intensity (0..5) and an optional named light effect. The matrix
// maps each WMO code to one of these.
//
// attenuation is the single sky-attenuation figure -- a plain per-condition percentage, derived
// from cloud cover in oktas at codegen. It says what the weather *looks* like, and nothing about
// heating: the daylight lamp is the daytime heater and belongs to the thermostat PID.
struct WeatherRule {
  float attenuation{1.0f};
  float rain{0.0f};
  std::string effect;  // empty / "none" -> no effect
};

// The weather currently in force, handed to every subscriber on each change.
struct WeatherState {
  int code{-1};             // WMO code, -1 before the first reading
  float attenuation{1.0f};
  float rain{0.0f};
  std::string effect;
  bool from_override{false};  // pinned by the HA test override rather than the live feed
};

// Owns the WMO weather matrix and the resolved weather state: the one place that answers "what is
// the weather doing right now". It writes the two outputs every consumer shares -- the sky
// attenuation global and the rain-intensity sensor the pump reads -- and hands the full resolved
// state to subscribers, so a consumer that needs more than a number (geckolights, which has to
// route the light effect) does not have to re-derive it from the code.
//
// It knows nothing about lights, pumps or fans by design; that is the point of the split.
class GeckoWeather : public Component {
 public:
  void set_code_sensor(sensor::Sensor *s) { this->code_sens_ = s; }
  void set_precipitation_sensor(sensor::Sensor *s) { this->precip_sens_ = s; }
  void set_attenuation(globals::GlobalsComponent<float> *g) { this->attenuation_ = g; }
  // Matrix, flattened to one entry per WMO code at codegen time.
  void add_rule(int code, float attenuation, float rain, const std::string &effect) {
    this->rules_[code] = WeatherRule{attenuation, rain, effect};
  }
  void set_default_rule(float attenuation, float rain, const std::string &effect) {
    this->default_ = WeatherRule{attenuation, rain, effect};
  }

  // Dispatch a WMO code through the matrix. Used by the live sensor and by the HA test override,
  // which pins the manually-chosen code until cleared.
  void set_weather_code(int code, bool from_override = false);
  void clear_override();
  // Re-announce the code currently in force. Used when a consumer's automation is re-enabled so it
  // resumes at once instead of waiting for the next weather event.
  void reapply();

  void add_on_weather_callback(std::function<void(const WeatherState &)> &&cb) {
    this->callback_.add(std::move(cb));
  }
  const WeatherState &state() const { return this->state_; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void resolve_(int code, bool from_override);

  sensor::Sensor *code_sens_{nullptr};
  sensor::Sensor *precip_sens_{nullptr};
  globals::GlobalsComponent<float> *attenuation_{nullptr};
  std::map<int, WeatherRule> rules_;  // WMO code -> behaviour
  WeatherRule default_;               // fallback for unmapped codes
  WeatherState state_;
  bool override_active_{false};  // HA test override pins the chosen code
  int last_live_code_{-1};       // last code seen from the live sensor
  CallbackManager<void(const WeatherState &)> callback_;
};

}  // namespace geckoweather
}  // namespace esphome
