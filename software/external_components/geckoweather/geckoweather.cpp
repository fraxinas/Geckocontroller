#include "geckoweather.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace geckoweather {

static const char *const TAG = "geckoweather";

static const uint32_t CHECK_INTERVAL_MS = 10 * 1000;

static bool api_connected() {
#ifdef USE_API
  return api::global_api_server != nullptr && api::global_api_server->is_connected();
#else
  return true;
#endif
}

void GeckoWeather::setup() {
  // A valid value makes its feed live; NaN (HA: unavailable / unknown -- e.g. no historic record for
  // this hour) is never passed on, it marks the feed lost and leaves the last good value in force
  // until the fallback takes over.
  this->watch_feed_(this->code_, "weather code", [this](float x) {
    this->last_live_code_ = (int) x;
    this->set_weather_code(this->last_live_code_, false);
  });
  this->watch_feed_(this->temperature_, "temperature", [this](float x) {
    if (this->temperature_out_ != nullptr)
      this->temperature_out_->publish_state(this->capped_temperature_(x));
  });
  this->watch_feed_(this->humidity_, "humidity", [this](float x) {
    if (this->humidity_out_ != nullptr)
      this->humidity_out_->publish_state(x);
  });
  this->watch_feed_(this->sky_brightness_, "sky brightness", [this](float) { this->update_(); });
  this->watch_feed_(this->precipitation_, "precipitation", [this](float) { this->update_(); });

  if (this->fallback_configured_())
    this->set_interval("fallback", CHECK_INTERVAL_MS, [this]() { this->check_fallback_(); });
}

void GeckoWeather::watch_feed_(Feed &feed, const char *name, std::function<void(float)> &&on_live) {
  if (feed.input == nullptr)
    return;
  feed.input->add_on_state_callback([this, &feed, name, on_live](float x) {
    if (std::isnan(x)) {
      ESP_LOGW(TAG, "%s: no historic value from HA", name);
      this->mark_lost_(feed, name, millis());
      return;
    }
    feed.lost = false;
    feed.value = x;
    if (feed.fallback) {
      feed.fallback = false;
      ESP_LOGI(TAG, "%s: historic feed back (%.1f), fallback ends", name, x);
    }
    on_live(x);
  });
}

void GeckoWeather::mark_lost_(Feed &feed, const char *name, uint32_t now) {
  if (feed.input == nullptr || feed.lost)
    return;
  feed.lost = true;
  feed.lost_since = now;
  if (this->fallback_configured_())
    ESP_LOGW(TAG, "%s: historic feed lost, fallback in %u s unless it returns", name, this->timeout_ / 1000);
}

void GeckoWeather::check_fallback_() {
  uint32_t now = millis();

  // Connection gone: the last values would stay in force indefinitely. Stays lost after a reconnect
  // until HA actually sends a value -- an entity HA does not have never comes back by itself.
  bool api = api_connected();
  if (this->api_was_connected_ && !api) {
    ESP_LOGW(TAG, "Home Assistant connection lost");
    this->mark_lost_(this->code_, "weather code", now);
    this->mark_lost_(this->temperature_, "temperature", now);
    this->mark_lost_(this->humidity_, "humidity", now);
    this->mark_lost_(this->sky_brightness_, "sky brightness", now);
    this->mark_lost_(this->precipitation_, "precipitation", now);
  }
  this->api_was_connected_ = api;

  auto due = [this, now](Feed &feed, const char *name) {
    if (feed.input == nullptr || !feed.lost || feed.fallback || now - feed.lost_since < this->timeout_)
      return false;
    feed.fallback = true;
    ESP_LOGW(TAG, "%s: no historic value for %u s, standard day/night cycle takes over", name,
             this->timeout_ / 1000);
    return true;
  };
  bool temp_due = due(this->temperature_, "temperature");
  bool hum_due = due(this->humidity_, "humidity");
  bool code_due = due(this->code_, "weather code");
  bool sky_due = due(this->sky_brightness_, "sky brightness");
  bool precip_due = due(this->precipitation_, "precipitation");
  if (code_due)
    this->set_weather_code(this->fallback_code_, false);  // ignored while the test override pins a code
  else if (sky_due || precip_due)
    this->update_();  // the code's typical value stands in

  float h = this->solar_hour_();

  // The cycle needs the clock. Without one (a boot that never reached HA), hold the middle of it
  // rather than a day or night extreme.
  if (this->temperature_.fallback) {
    float t = std::isnan(h) ? (this->temp_day_ + this->temp_night_) / 2.0f : this->cycle_(this->temp_day_, this->temp_night_, h);
    if (temp_due)
      ESP_LOGI(TAG, "fallback temperature %.1f °C", t);
    publish_rounded_(this->temperature_out_, this->capped_temperature_(t), 0.1f);
  }
  if (this->humidity_.fallback) {
    float rh = std::isnan(h) ? (this->hum_day_ + this->hum_night_) / 2.0f : this->cycle_(this->hum_day_, this->hum_night_, h);
    if (hum_due)
      ESP_LOGI(TAG, "fallback humidity %.0f %%", rh);
    publish_rounded_(this->humidity_out_, rh, 1.0f);
  }

  // Daily shower, in the morning so the simulated sun dries the excess water off by day instead of
  // leaving it damp overnight. Fires on crossing rain_hour, so a fallback that starts later in the
  // day does not rain at once -- HA's weather may well have rained that day already. Only while
  // nothing tells the rain any more: a live weather code still brings its typical precipitation.
  if (!std::isnan(h)) {
    bool crossed = !std::isnan(this->last_solar_hour_) && this->last_solar_hour_ < this->rain_hour_ &&
                   h >= this->rain_hour_ && h > this->last_solar_hour_;
    bool rain_unknown = this->code_.fallback && (this->precipitation_.input == nullptr || this->precipitation_.fallback);
    if (crossed && rain_unknown) {
      ESP_LOGI(TAG, "fallback: daily rain");
      this->rain_callback_.call();
    }
    this->last_solar_hour_ = h;
  }
}

float GeckoWeather::solar_hour_() {
  if (this->time_ == nullptr)
    return NAN;
  ESPTime utc = this->time_->utcnow();
  if (!utc.is_valid())
    return NAN;
  float h = utc.hour + utc.minute / 60.0f + utc.second / 3600.0f + this->longitude_ / 15.0f;
  h = std::fmod(h, 24.0f);
  return h < 0.0f ? h + 24.0f : h;
}

// Cosine over the solar day: `day` at warmest_hour, `night` twelve hours later.
float GeckoWeather::cycle_(float day, float night, float solar_hour) const {
  float phase = (solar_hour - this->warmest_hour_) * (2.0f * (float) M_PI / 24.0f);
  return (day + night) / 2.0f + (day - night) / 2.0f * std::cos(phase);
}

// The temperature setpoint never goes above the configured maximum: a hotter value is replaced by it.
float GeckoWeather::capped_temperature_(float t) const {
  if (std::isnan(this->max_temperature_) || t <= this->max_temperature_)
    return t;
  ESP_LOGW(TAG, "temperature %.1f °C above the maximum, setpoint capped at %.1f °C", t, this->max_temperature_);
  return this->max_temperature_;
}

// Publish only when the value moves by a visible step: the climates take every publish as a new
// target, and the cycle is re-evaluated every few seconds.
void GeckoWeather::publish_rounded_(sensor::Sensor *s, float value, float step) {
  if (s == nullptr)
    return;
  float v = std::round(value / step) * step;
  if (s->has_state() && std::fabs(s->state - v) < step / 2.0f)
    return;
  s->publish_state(v);
}

void GeckoWeather::set_weather_code(int code, bool from_override) {
  if (this->override_active_ && !from_override)
    return;  // a manual test override is active; ignore the live sensor
  if (from_override)
    this->override_active_ = true;
  this->resolve_(code, from_override);
}

void GeckoWeather::reapply() {
  if (this->resolved_)
    this->resolve_(this->state_.code, this->override_active_);
}

// A measured value changed or was lost: resolve the code in force again.
void GeckoWeather::update_() {
  int code = this->override_active_ ? this->state_.code : this->feed_code_();
  this->resolve_(code, this->override_active_);
}

// The measured value while the feed gives one -- held over a short loss, until the fallback takes
// over -- else the typical value of the code in force. The test override always takes the typical
// value: picking "Heavy rain" has to rain, whatever the sky is doing.
float GeckoWeather::measured_or_(const Feed &feed, float typical) const {
  if (this->override_active_ || feed.input == nullptr || feed.fallback || std::isnan(feed.value))
    return typical;
  return feed.value;
}

void GeckoWeather::clear_override() {
  this->override_active_ = false;
  int code = this->feed_code_();
  ESP_LOGD(TAG, "override cleared -> %s code %d", this->code_.fallback ? "fallback" : "live", code);
  this->update_();  // back to the measured cover and precipitation too
}

// Resolve the weather and publish the result. The code gives passing clouds and the effect; sky
// brightness and precipitation are measured (or the code's typical values) and are the sky
// attenuation and the precipitation the rain pump follows. Both outputs are written
// unconditionally: this component reports what the weather is doing, and whether to act on it is
// each consumer's own business (they have their own master enables). Subscribers get the full resolved state last, so they see the attenuation global
// already updated. Before the first code (-1) the default rule stands in.
void GeckoWeather::resolve_(int code, bool from_override) {
  auto it = this->rules_.find(code);
  const WeatherRule &r = (it != this->rules_.end()) ? it->second : this->default_;

  float attenuation = this->measured_or_(this->sky_brightness_, r.sky_brightness * 100.0f);
  attenuation = clamp(attenuation / 100.0f, 0.0f, 1.0f);
  float mm = std::max(this->measured_or_(this->precipitation_, r.precipitation), 0.0f);

  WeatherState s{code, mm, attenuation, r.clouds, r.effect, from_override};
  bool changed = !this->resolved_ || s.code != this->state_.code || s.from_override != this->state_.from_override ||
                 s.attenuation != this->state_.attenuation || s.precipitation != this->state_.precipitation;
  this->state_ = s;
  this->resolved_ = true;
  if (changed)
    ESP_LOGD(TAG, "code %d%s: sky brightness %.0f %%, %.1f mm, clouds %.2f, effect '%s'", code,
             from_override ? " (override)" : "", attenuation * 100.0f, mm, r.clouds, r.effect.c_str());

  if (this->attenuation_ != nullptr)
    globals::id(this->attenuation_) = attenuation;
  if (this->precip_sens_ != nullptr)
    this->precip_sens_->publish_state(mm);
  this->callback_.call(this->state_);
}

void GeckoWeather::dump_config() {
  ESP_LOGCONFIG(TAG, "GeckoWeather:");
  ESP_LOGCONFIG(TAG, "  mapped WMO codes: %u", (unsigned) this->rules_.size());
  if (!std::isnan(this->max_temperature_))
    ESP_LOGCONFIG(TAG, "  temperature setpoint capped at %.1f °C", this->max_temperature_);
  ESP_LOGCONFIG(TAG, "  sky brightness: %s, precipitation: %s",
                this->sky_brightness_.input != nullptr ? "measured" : "typical of the code",
                this->precipitation_.input != nullptr ? "measured" : "typical of the code");
  if (this->fallback_configured_()) {
    ESP_LOGCONFIG(TAG, "  fallback after %u s: temperature %.1f/%.1f °C, humidity %.0f/%.0f %% (day/night), "
                       "warmest at %.1f h solar, code %d, daily rain at %.1f h solar",
                  this->timeout_ / 1000, this->temp_day_, this->temp_night_, this->hum_day_, this->hum_night_,
                  this->warmest_hour_, this->fallback_code_, this->rain_hour_);
  } else {
    ESP_LOGCONFIG(TAG, "  fallback: none");
  }
}

}  // namespace geckoweather
}  // namespace esphome
