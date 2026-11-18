#include "geckoalarm.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace geckoalarm {

static const char *const TAG = "geckoalarm";

// For the log only; Home Assistant and the console tell the alarms apart by their entities.
static const char *const LOG_NAMES[] = {"overtemperature", "undertemperature", "low humidity"};
static_assert(sizeof(LOG_NAMES) / sizeof(LOG_NAMES[0]) == static_cast<size_t>(Alarm::COUNT), "one name per alarm");

static constexpr uint32_t CHECK_MS = 1000;
// Home Assistant re-sends every subscribed state on connecting; its feeds are judged from then on.
static constexpr uint32_t API_SETTLE_MS = 60000;

static bool api_connected() {
#ifdef USE_API
  return api::global_api_server != nullptr && api::global_api_server->is_connected();
#else
  return true;
#endif
}

void GeckoAlarm::setup() {
  // A known state in Home Assistant from the start, not "unknown" until the first alarm.
  for (auto &s : this->state_)
    if (s.sensor != nullptr)
      s.sensor->publish_initial_state(false);
  if (this->sensor_problem_.sensor != nullptr)
    this->sensor_problem_.sensor->publish_initial_state(false);
  if (this->details_ != nullptr)
    this->details_->publish_state("");
#ifdef USE_GECKOALARM_WEATHER
  // the historic feeds, after the configured sensors; only the temperature has a stale timeout
  if (this->weather_ != nullptr) {
    for (uint8_t f = 0; f < static_cast<uint8_t>(geckoweather::WeatherFeed::COUNT); f++) {
      const auto feed = static_cast<geckoweather::WeatherFeed>(f);
      sensor::Sensor *input = this->weather_->feed_input(feed);
      if (input == nullptr)
        continue;
      Watched w{input, feed == geckoweather::WeatherFeed::TEMPERATURE ? this->stale_timeout_ms_ : 0, false};
      w.feed = (int8_t) f;
      this->watched_.push_back(w);
    }
  }
#endif
  // Sensor health: the timeouts count from here; every valid reading restarts its sensor's.
  const uint32_t now = millis();
  for (size_t i = 0; i < this->watched_.size(); i++) {
    this->watched_[i].last_ok = now;
    auto ok = [this, i](float x) {
      if (!std::isnan(x))
        this->watched_[i].last_ok = millis();
    };
    if (this->watched_[i].raw)
      this->watched_[i].sensor->add_on_raw_state_callback(ok);
    else
      this->watched_[i].sensor->add_on_state_callback(ok);
  }
  this->set_interval("check", CHECK_MS, [this]() { this->check_(); });
}

void GeckoAlarm::check_() {
  const float t = this->thermostat_->current_temperature, ts = this->thermostat_->target_temperature;
  const float h = this->hygrostat_->current_temperature, hs = this->hygrostat_->target_temperature;
  this->update_(Alarm::OVERTEMPERATURE, t - ts, this->temp_deviation_, this->temp_hysteresis_, t, ts);
  this->update_(Alarm::UNDERTEMPERATURE, ts - t, this->temp_deviation_, this->temp_hysteresis_, t, ts);
  this->update_(Alarm::LOW_HUMIDITY, hs - h, this->hum_deficit_, this->hum_hysteresis_, h, hs);
  this->check_sensors_();
}

void GeckoAlarm::check_sensors_() {
  const uint32_t now = millis();
  const bool api = api_connected();
  if (api && !this->api_up_)
    this->api_since_ = now;
  this->api_up_ = api;
  bool changed = false, any = false;
  for (auto &w : this->watched_) {
    const bool bad = w.feed >= 0 ? this->weather_bad_(w, now) : now - w.last_ok > w.timeout_ms;
    if (bad != w.bad) {
      w.bad = bad;
      changed = true;
      if (bad && w.feed >= 0)
        ESP_LOGW(TAG, "historic feed '%s' missing or stuck", w.sensor->get_name().c_str());
      else if (bad)
        ESP_LOGW(TAG, "sensor '%s': no valid reading for %u s", w.sensor->get_name().c_str(),
                 (unsigned) (w.timeout_ms / 1000));
      else
        ESP_LOGI(TAG, "sensor '%s': valid readings again", w.sensor->get_name().c_str());
    }
    any |= bad;
  }
  // the details first, so an automation triggered by the binary sensor reads the current list
  if (changed && this->details_ != nullptr) {
    std::string names;
    for (auto &w : this->watched_) {
      if (!w.bad)
        continue;
      if (!names.empty())
        names += ", ";
      names += w.sensor->get_name().c_str();
    }
    this->details_->publish_state(names);
  }
  // on at once (the timeout was the delay), off once all have been fine for clear_delay
  State &s = this->sensor_problem_;
  bool publish = false;
  if (any) {
    s.timing = false;
    publish = !s.on;
    s.on = true;
  } else if (s.on) {
    if (!s.timing) {
      s.timing = true;
      s.since = now;
    }
    if (now - s.since >= this->clear_delay_ms_) {
      s.on = false;
      s.timing = false;
      publish = true;
    }
  }
  if (publish && s.sensor != nullptr)
    s.sensor->publish_state(s.on);
}

void GeckoAlarm::update_(Alarm a, float excess, float limit, float hysteresis, float value, float setpoint) {
  if (std::isnan(excess))
    return;  // no reading: leave the alarm as it is
  State &s = this->state_[idx(a)];
  // Off: waiting for the value to stay out of range. On: waiting for it to stay back inside.
  const bool changing = s.on ? excess < limit - hysteresis : excess > limit;
  if (!changing) {
    s.timing = false;
    return;
  }
  const uint32_t now = millis();
  if (!s.timing) {
    s.timing = true;
    s.since = now;
  }
  if (now - s.since < (s.on ? this->clear_delay_ms_ : this->delay_ms_))
    return;
  s.on = !s.on;
  s.timing = false;
  if (s.on)
    ESP_LOGW(TAG, "%s alarm: %.1f, setpoint %.1f", LOG_NAMES[idx(a)], value, setpoint);
  else
    ESP_LOGI(TAG, "%s alarm cleared: %.1f, setpoint %.1f", LOG_NAMES[idx(a)], value, setpoint);
  if (s.sensor != nullptr)
    s.sensor->publish_state(s.on);
}

bool GeckoAlarm::weather_bad_(Watched &w, uint32_t now) {
#ifdef USE_GECKOALARM_WEATHER
  // Home Assistant away or just back: nothing to judge -- keep the state, restart the stale timer
  if (!this->api_up_ || now - this->api_since_ < API_SETTLE_MS) {
    w.last_ok = now;
    return w.bad;
  }
  if (this->weather_->feed_in_fallback(static_cast<geckoweather::WeatherFeed>(w.feed)))
    return true;  // missing
  return w.timeout_ms > 0 && now - w.last_ok > w.timeout_ms;  // stuck
#else
  return false;
#endif
}

void GeckoAlarm::dump_config() {
  ESP_LOGCONFIG(TAG, "Gecko climate alarms:");
  ESP_LOGCONFIG(TAG, "  temperature: setpoint +/- %.1f K (clears %.1f K inside)", this->temp_deviation_,
                this->temp_hysteresis_);
  ESP_LOGCONFIG(TAG, "  humidity: setpoint - %.0f %%RH (clears %.0f %%RH inside)", this->hum_deficit_,
                this->hum_hysteresis_);
  ESP_LOGCONFIG(TAG, "  raised after %u s, cleared after %u s", (unsigned) (this->delay_ms_ / 1000),
                (unsigned) (this->clear_delay_ms_ / 1000));
  for (size_t i = 0; i < N; i++)
    if (this->state_[i].sensor != nullptr)
      LOG_BINARY_SENSOR("  ", LOG_NAMES[i], this->state_[i].sensor);
  for (auto &w : this->watched_) {
    if (w.feed >= 0)
      ESP_LOGCONFIG(TAG, "  watching historic feed '%s': missing (fallback)%s", w.sensor->get_name().c_str(),
                    w.timeout_ms > 0 ? ", or unchanged for the stale timeout" : "");
    else
      ESP_LOGCONFIG(TAG, "  watching '%s': valid reading%s at least every %u s", w.sensor->get_name().c_str(),
                    w.raw ? " (raw)" : "", (unsigned) (w.timeout_ms / 1000));
  }
  LOG_BINARY_SENSOR("  ", "sensor problem", this->sensor_problem_.sensor);
  LOG_TEXT_SENSOR("  ", "sensor problem details", this->details_);
}

}  // namespace geckoalarm
}  // namespace esphome
