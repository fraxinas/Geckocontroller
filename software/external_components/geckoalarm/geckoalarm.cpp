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
      Watched &w = this->watched_[i];
      if (!std::isnan(x) && x >= w.min && x <= w.max)
        w.last_ok = millis();
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
  const float dev = this->temp_deviation_;
  State &over = this->state_[idx(Alarm::OVERTEMPERATURE)], &under = this->state_[idx(Alarm::UNDERTEMPERATURE)];
  State &dry = this->state_[idx(Alarm::LOW_HUMIDITY)];

  // too hot: above the setpoint by the deviation, or any sensor above its absolute maximum
  std::vector<Cond> &hot = this->hot_;
  hot.clear();
  hot.push_back({t - ts - dev, &over.raise, this->delay_ms_, nullptr, t, ts});
  for (auto &m : this->max_temps_) {
    const float v = m.sensor->has_state() ? m.sensor->state : NAN;
    hot.push_back({v - m.max, &m.raise, this->max_temp_delay_ms_, m.sensor->get_name().c_str(), v, m.max});
  }
  this->update_(Alarm::OVERTEMPERATURE, hot.data(), hot.size(), this->temp_hysteresis_);

  const Cond cold{ts - t - dev, &under.raise, this->delay_ms_, nullptr, t, ts};
  this->update_(Alarm::UNDERTEMPERATURE, &cold, 1, this->temp_hysteresis_);
  const Cond low{hs - h - this->hum_deficit_, &dry.raise, this->delay_ms_, nullptr, h, hs};
  this->update_(Alarm::LOW_HUMIDITY, &low, 1, this->hum_hysteresis_);
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
    s.clear.running = false;
    publish = !s.on;
    s.on = true;
  } else if (s.on && s.clear.held(true, now, this->clear_delay_ms_)) {
    s.on = false;
    s.clear.running = false;
    publish = true;
  }
  if (publish && s.sensor != nullptr)
    s.sensor->publish_state(s.on);
}

// Off: raised once one condition has been out of range for its delay. On: cleared once every
// condition with a reading has been back inside by the hysteresis for clear_delay. A condition
// without a reading counts for neither; with none at all the alarm stays as it is.
void GeckoAlarm::update_(Alarm a, const Cond *conds, size_t n, float hysteresis) {
  State &s = this->state_[idx(a)];
  const uint32_t now = millis();
  bool known = false, back = true;
  const Cond *raised = nullptr;
  for (size_t i = 0; i < n; i++) {
    const Cond &c = conds[i];
    if (std::isnan(c.excess))
      continue;  // no reading: leave its timer as it is
    known = true;
    back &= c.excess < -hysteresis;
    if (c.raise->held(!s.on && c.excess > 0.0f, now, c.delay_ms) && raised == nullptr)
      raised = &c;
  }
  if (!known)
    return;
  if (!s.on) {
    if (raised == nullptr)
      return;
    s.on = true;
    s.clear.running = false;
    if (raised->name == nullptr)
      ESP_LOGW(TAG, "%s alarm: %.1f, setpoint %.1f", LOG_NAMES[idx(a)], raised->value, raised->ref);
    else
      ESP_LOGW(TAG, "%s alarm: %s %.1f, maximum %.1f", LOG_NAMES[idx(a)], raised->name, raised->value, raised->ref);
  } else {
    if (!s.clear.held(back, now, this->clear_delay_ms_))
      return;
    s.on = false;
    s.clear.running = false;
    ESP_LOGI(TAG, "%s alarm cleared: %.1f, setpoint %.1f", LOG_NAMES[idx(a)], conds[0].value, conds[0].ref);
  }
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
  for (auto &m : this->max_temps_)
    ESP_LOGCONFIG(TAG, "  '%s': at most %.1f °C (raised after %u s)", m.sensor->get_name().c_str(), m.max,
                  (unsigned) (this->max_temp_delay_ms_ / 1000));
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
      ESP_LOGCONFIG(TAG, "  watching '%s': valid reading%s in %g..%g at least every %u s",
                    w.sensor->get_name().c_str(), w.raw ? " (raw)" : "", w.min, w.max,
                    (unsigned) (w.timeout_ms / 1000));
  }
  LOG_BINARY_SENSOR("  ", "sensor problem", this->sensor_problem_.sensor);
  LOG_TEXT_SENSOR("  ", "sensor problem details", this->details_);
}

}  // namespace geckoalarm
}  // namespace esphome
