#pragma once

// Climate alarms: the terrarium too hot, too cold or too dry for a while, whatever the cause -- a
// blown heat lamp or daylight lamp, a clogged rain pipe, an empty reservoir, a lamp left on by hand
// -- and whether the automations are on or not. It only looks at the outcome: the thermostat's and
// the hygrostat's current value against their setpoint.
//
// Each alarm is a binary sensor of device class "problem" (Home Assistant: "Problem" / "OK"), for
// the Geckoconsole and for automations such as an instant message. An alarm is raised once its value
// has been out of range for `delay` without a break, and cleared once it has been back inside the
// limit by the hysteresis for `clear_delay`, so a value hovering at the limit does not keep raising
// it. A missing reading (sensor failure) neither raises nor clears an alarm.
//
// Absolute maximum temperatures (`max_temperature`): whatever the setpoint, a sensor above its
// maximum (e.g. 33 °C at the ceiling, 29 °C at the ground) raises the overtemperature alarm too,
// after `max_temperature_delay`. It clears once the setpoint deviation and every maximum are back
// inside by the temperature hysteresis for `clear_delay`.
//
// Sensor health: a watched sensor that has not delivered a valid reading (not NaN, through its
// filters -- or before them with `raw`) for its timeout is a problem -- missing (never found at boot), broken, or failing its
// reads, whether the driver then skips publishing or publishes NaN. "Sensor Problem" (device
// class "problem") goes on as soon as one is, and off once none has been for `clear_delay`;
// "Sensor Problem Details" names the sensors concerned, by their entity names, and is published
// before the binary sensor so an automation triggered by it reads the current list.
//
// The historic weather feeds from Home Assistant count as sensors too (`weather:`): missing while
// geckoweather's fallback stands in for one, stuck while the historic temperature has not changed
// for its stale timeout. While Home Assistant is away (and for API_SETTLE_MS after it is back and
// has re-sent its states) they cannot be judged and keep their state -- otherwise every longer
// Home Assistant restart would end in a sensor problem.

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>
#ifdef USE_GECKOALARM_WEATHER
#include "esphome/components/geckoweather/geckoweather.h"
#endif

namespace esphome {
namespace geckoalarm {

enum class Alarm : uint8_t {
  OVERTEMPERATURE,   // temperature above the setpoint by more than temperature_deviation, or a
                     // sensor above its absolute maximum
  UNDERTEMPERATURE,  // temperature below the setpoint by more than temperature_deviation
  LOW_HUMIDITY,      // humidity below the setpoint by more than humidity_deficit
  COUNT,
};

class GeckoAlarm : public Component {
 public:
  void set_thermostat(climate::Climate *c) { this->thermostat_ = c; }
  void set_hygrostat(climate::Climate *c) { this->hygrostat_ = c; }
  void set_temperature_deviation(float k) { this->temp_deviation_ = k; }
  void set_humidity_deficit(float rh) { this->hum_deficit_ = rh; }
  void set_temperature_hysteresis(float k) { this->temp_hysteresis_ = k; }
  void set_humidity_hysteresis(float rh) { this->hum_hysteresis_ = rh; }
  void set_delay(uint32_t ms) { this->delay_ms_ = ms; }
  void set_clear_delay(uint32_t ms) { this->clear_delay_ms_ = ms; }
  void add_max_temperature(sensor::Sensor *s, float max) { this->max_temps_.push_back({s, max}); }
  void set_max_temperature_delay(uint32_t ms) { this->max_temp_delay_ms_ = ms; }
  void set_sensor(Alarm a, binary_sensor::BinarySensor *s) { this->state_[idx(a)].sensor = s; }
  void add_watched_sensor(sensor::Sensor *s, uint32_t timeout_ms, bool raw) {
    this->watched_.push_back({s, timeout_ms, raw});
  }
  void set_sensor_problem(binary_sensor::BinarySensor *s) { this->sensor_problem_.sensor = s; }
  void set_sensor_problem_details(text_sensor::TextSensor *s) { this->details_ = s; }
#ifdef USE_GECKOALARM_WEATHER
  void set_weather(geckoweather::GeckoWeather *w, uint32_t stale_timeout_ms) {
    this->weather_ = w;
    this->stale_timeout_ms_ = stale_timeout_ms;
  }
#endif

  bool active(Alarm a) const { return this->state_[idx(a)].on; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  static constexpr size_t N = static_cast<size_t>(Alarm::COUNT);
  static constexpr size_t idx(Alarm a) { return static_cast<size_t>(a); }

  // Whether a condition has held for a while without a break
  struct Timer {
    bool running{false};
    uint32_t since{0};
    bool held(bool cond, uint32_t now, uint32_t ms) {
      if (!cond) {
        this->running = false;
        return false;
      }
      if (!this->running) {
        this->running = true;
        this->since = now;
      }
      return now - this->since >= ms;
    }
  };

  struct State {
    binary_sensor::BinarySensor *sensor{nullptr};
    bool on{false};
    Timer clear;  // on: back inside since
    Timer raise;  // off: out of range since (the climate against its setpoint)
  };

  // An absolute maximum temperature at a sensor, raising the overtemperature alarm
  struct MaxTemp {
    sensor::Sensor *sensor;
    float max;
    Timer raise;  // above the maximum since
  };

  // One condition of an alarm: how far its value is beyond the limit in the alarm's direction
  // (> 0: out of range, NAN: no reading), and what it is for the log.
  struct Cond {
    float excess;
    Timer *raise;
    uint32_t delay_ms;
    const char *name;  // nullptr: the climate, `value` against its setpoint `ref`; else a maximum
    float value;
    float ref;
  };

  struct Watched {
    sensor::Sensor *sensor;
    uint32_t timeout_ms;
    bool raw;             // readings before the filters count
    int8_t feed{-1};      // a geckoweather feed (WeatherFeed), judged by its fallback; 0 timeout = none
    uint32_t last_ok{0};  // millis() of the last valid reading (or of setup)
    bool bad{false};
  };

  void check_();
  void check_sensors_();
  bool weather_bad_(Watched &w, uint32_t now);
  void update_(Alarm a, const Cond *conds, size_t n, float hysteresis);

  climate::Climate *thermostat_{nullptr};
  climate::Climate *hygrostat_{nullptr};
  float temp_deviation_{4.0f};
  float hum_deficit_{10.0f};
  float temp_hysteresis_{0.5f};
  float hum_hysteresis_{2.0f};
  uint32_t delay_ms_{600000};
  uint32_t clear_delay_ms_{120000};
  std::vector<MaxTemp> max_temps_;
  uint32_t max_temp_delay_ms_{120000};
  std::vector<Cond> hot_;  // the overtemperature conditions, reused every check
  State state_[N];
  std::vector<Watched> watched_;
  State sensor_problem_;
  text_sensor::TextSensor *details_{nullptr};
#ifdef USE_GECKOALARM_WEATHER
  geckoweather::GeckoWeather *weather_{nullptr};
  uint32_t stale_timeout_ms_{0};
#endif
  bool api_up_{false};
  uint32_t api_since_{0};  // millis() the API connection came up
};

}  // namespace geckoalarm
}  // namespace esphome
