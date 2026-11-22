#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include <cmath>
#include <map>
#include <string>

namespace esphome {
namespace geckoweather {

// What a WMO code says about the weather. The code only decides what the numbers cannot: how much
// broken cloud drifts across the sun (clouds, 0..1) and an optional named light effect.
// sky_brightness (0..1) and precipitation (mm) are the condition's *typical* values, standing in for
// the measured ones only while the test override pins this code or a measured feed is lost.
struct WeatherRule {
  float clouds{0.0f};
  std::string effect;  // empty / "none" -> no effect
  float sky_brightness{1.0f};
  float precipitation{0.0f};
};

// The weather currently in force, handed to every subscriber on each change.
//
// attenuation is the single sky-attenuation figure: how bright the terrarium looks -- the historic
// sky brightness. It says nothing about heating: the daylight lamp is the daytime heater and belongs
// to the thermostat PID. precipitation is what the rain pump follows.
struct WeatherState {
  int code{-1};               // WMO code, -1 before the first reading
  float precipitation{0.0f};  // mm in the hour, measured or the code's typical value
  float attenuation{1.0f};    // 0..1, the sky brightness measured or the code's typical value
  float clouds{0.0f};
  std::string effect;
  bool from_override{false};  // pinned by the HA test override rather than the live feed
};

// One historic feed from Home Assistant (temperature, humidity, WMO code, sky brightness or
// precipitation) and whether the fallback is standing in for it. A feed is lost when HA reports it unavailable (the HA sensor
// publishes NaN), when the API connection drops (the last value would otherwise stay in force
// indefinitely), and from boot until its first value arrives. It is live again on the next valid
// value -- HA resends every subscribed state on reconnect.
struct Feed {
  sensor::Sensor *input{nullptr};
  bool lost{true};
  uint32_t lost_since{0};
  bool fallback{false};  // lost for longer than the timeout: the standard cycle stands in
  float value{NAN};      // last valid value, held while lost until the fallback stands in
};

// The historic feeds, by name for other components (geckoalarm's sensor health)
enum class WeatherFeed : uint8_t { CODE, TEMPERATURE, HUMIDITY, SKY_BRIGHTNESS, PRECIPITATION, COUNT };

// Owns the resolved weather state: the one place that answers "what is the weather doing right
// now". It writes the outputs every consumer shares -- the sky attenuation global, the precipitation
// the rain pump follows, and the temperature/humidity setpoint sensors the climates follow -- and
// hands the full resolved state to subscribers, so a consumer that needs more than a number
// (geckolights, which has to route the light effect) does not have to re-derive it.
//
// The measured figures set the levels: the sky attenuation is the historic sky brightness (%, how
// bright the sky is against a clear one, worked out by the HA component from the radiation by day
// and the cloud cover by night), and the rain pump turns the precipitation (mm) into water. The WMO
// code is only the character of the weather, through the matrix: passing clouds and light effects
// (Lightning). Its typical brightness and precipitation stand in for a measured value that is
// missing.
//
// Fallback: without the historic feed the terrarium would sit on the last value HA sent -- possibly
// a heatwave afternoon or a cold night -- for as long as HA is gone. Once a feed has been lost for
// `timeout`, a standard day/night cycle stands in for it: temperature and humidity follow a cosine
// over the solar day (warmest and driest at `warmest_hour`, coolest and most humid twelve hours
// later), the sky falls back to `code`, sky brightness and precipitation to the typical values of the
// code in force, and with neither precipitation nor weather code left `on_rain` fires once a day at
// `rain_hour` (a morning hour, so the day dries the terrarium) so it still gets its shower.
//
// It knows nothing about lights, pumps or fans by design; that is the point of the split.
class GeckoWeather : public Component {
 public:
  void set_code_sensor(sensor::Sensor *s) { this->code_.input = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_.input = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_.input = s; }
  void set_sky_brightness_sensor(sensor::Sensor *s) { this->sky_brightness_.input = s; }
  void set_precipitation_sensor(sensor::Sensor *s) { this->precipitation_.input = s; }
  void set_temperature_setpoint_sensor(sensor::Sensor *s) { this->temperature_out_ = s; }
  void set_humidity_setpoint_sensor(sensor::Sensor *s) { this->humidity_out_ = s; }
  // Cap of the temperature setpoint: a hotter value is published as this.
  void set_max_temperature(float t) { this->max_temperature_ = t; }
  // Output: the precipitation in force (mm in the hour), which the rain pump follows.
  void set_precipitation_setpoint_sensor(sensor::Sensor *s) { this->precip_sens_ = s; }
  void set_attenuation(globals::GlobalsComponent<float> *g) { this->attenuation_ = g; }
  // Matrix, flattened to one entry per WMO code at codegen time.
  void add_rule(int code, float clouds, const std::string &effect, float sky_brightness, float precipitation) {
    this->rules_[code] = WeatherRule{clouds, effect, sky_brightness, precipitation};
  }
  void set_default_rule(float clouds, const std::string &effect, float sky_brightness, float precipitation) {
    this->default_ = WeatherRule{clouds, effect, sky_brightness, precipitation};
  }

  // Fallback (all optional: without a clock there is no fallback, only the NaN guard).
  void set_fallback_time(time::RealTimeClock *t) { this->time_ = t; }
  void set_fallback_timeout(uint32_t ms) { this->timeout_ = ms; }
  void set_fallback_longitude(float lon) { this->longitude_ = lon; }
  void set_fallback_warmest_hour(float h) { this->warmest_hour_ = h; }
  void set_fallback_temperature(float day, float night) {
    this->temp_day_ = day;
    this->temp_night_ = night;
  }
  void set_fallback_humidity(float day, float night) {
    this->hum_day_ = day;
    this->hum_night_ = night;
  }
  void set_fallback_code(int code) { this->fallback_code_ = code; }
  void set_fallback_rain_hour(float h) { this->rain_hour_ = h; }
  void add_on_fallback_rain_callback(std::function<void()> &&cb) { this->rain_callback_.add(std::move(cb)); }

  // Dispatch a WMO code through the matrix. Used by the live sensor and by the HA test override,
  // which pins the manually-chosen code -- with its typical cover and precipitation -- until cleared.
  void set_weather_code(int code, bool from_override = false);
  void clear_override();
  // Re-announce the code currently in force. Used when a consumer's automation is re-enabled so it
  // resumes at once instead of waiting for the next weather event.
  void reapply();

  // Whether the standard cycle stands in for any of the historic feeds.
  bool in_fallback() const {
    return this->temperature_.fallback || this->humidity_.fallback || this->code_.fallback ||
           this->sky_brightness_.fallback || this->precipitation_.fallback;
  }
  // One feed: its Home Assistant sensor (nullptr if not configured), and whether the standard cycle
  // stands in for it -- lost for longer than the fallback timeout.
  sensor::Sensor *feed_input(WeatherFeed f) const { return this->feed_(f).input; }
  bool feed_in_fallback(WeatherFeed f) const { return this->feed_(f).fallback; }

  void add_on_weather_callback(std::function<void(const WeatherState &)> &&cb) {
    this->callback_.add(std::move(cb));
  }
  const WeatherState &state() const { return this->state_; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void resolve_(int code, bool from_override);
  // Re-resolve the code in force, e.g. after a measured value changed.
  void update_();
  // A measured value, or the stand-in while the override pins a code or the feed has none.
  float measured_or_(const Feed &feed, float typical) const;
  const Feed &feed_(WeatherFeed f) const {
    switch (f) {
      case WeatherFeed::CODE: return this->code_;
      case WeatherFeed::TEMPERATURE: return this->temperature_;
      case WeatherFeed::HUMIDITY: return this->humidity_;
      case WeatherFeed::SKY_BRIGHTNESS: return this->sky_brightness_;
      default: return this->precipitation_;
    }
  }
  // The code the feed says is in force: the live code, or the fallback sky while it is lost.
  int feed_code_() const { return this->code_.fallback ? this->fallback_code_ : this->last_live_code_; }

  bool fallback_configured_() const { return this->time_ != nullptr; }
  void watch_feed_(Feed &feed, const char *name, std::function<void(float)> &&on_live);
  void mark_lost_(Feed &feed, const char *name, uint32_t now);
  void check_fallback_();
  // Hour of the *solar* day (0..24) at the sun's longitude, NAN without a valid clock.
  float solar_hour_();
  float cycle_(float day, float night, float solar_hour) const;
  static void publish_rounded_(sensor::Sensor *s, float value, float step);
  float capped_temperature_(float t) const;

  Feed code_;
  Feed temperature_;
  Feed humidity_;
  Feed sky_brightness_;  // %
  Feed precipitation_;   // mm
  sensor::Sensor *temperature_out_{nullptr};
  sensor::Sensor *humidity_out_{nullptr};
  float max_temperature_{NAN};  // NAN: no cap
  sensor::Sensor *precip_sens_{nullptr};
  globals::GlobalsComponent<float> *attenuation_{nullptr};
  std::map<int, WeatherRule> rules_;  // WMO code -> behaviour
  WeatherRule default_;               // fallback for unmapped codes
  WeatherState state_;
  bool override_active_{false};  // HA test override pins the chosen code
  bool resolved_{false};         // state_ holds a resolved weather
  int last_live_code_{-1};       // last code seen from the live sensor
  CallbackManager<void(const WeatherState &)> callback_;

  time::RealTimeClock *time_{nullptr};
  uint32_t timeout_{10 * 60 * 1000};
  float longitude_{0.0f};
  float warmest_hour_{15.0f};
  float temp_day_{26.0f};
  float temp_night_{21.0f};
  float hum_day_{65.0f};
  float hum_night_{80.0f};
  int fallback_code_{1};
  float rain_hour_{8.0f};
  float last_solar_hour_{NAN};  // for detecting the crossing of rain_hour
  bool api_was_connected_{false};
  CallbackManager<void()> rain_callback_;
};

// Fires once a day at the fallback rain hour while neither precipitation nor weather code is known.
class FallbackRainTrigger : public Trigger<> {
 public:
  explicit FallbackRainTrigger(GeckoWeather *parent) {
    parent->add_on_fallback_rain_callback([this]() { this->trigger(); });
  }
};

}  // namespace geckoweather
}  // namespace esphome
