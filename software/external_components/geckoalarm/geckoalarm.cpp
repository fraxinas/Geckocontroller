#include "geckoalarm.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace geckoalarm {

static const char *const TAG = "geckoalarm";

// For the log only; Home Assistant and the console tell the alarms apart by their entities.
static const char *const LOG_NAMES[] = {"overtemperature", "undertemperature", "low humidity"};
static_assert(sizeof(LOG_NAMES) / sizeof(LOG_NAMES[0]) == static_cast<size_t>(Alarm::COUNT), "one name per alarm");

static constexpr uint32_t CHECK_MS = 1000;

void GeckoAlarm::setup() {
  // A known state in Home Assistant from the start, not "unknown" until the first alarm.
  for (auto &s : this->state_)
    if (s.sensor != nullptr)
      s.sensor->publish_initial_state(false);
  this->set_interval("check", CHECK_MS, [this]() { this->check_(); });
}

void GeckoAlarm::check_() {
  const float t = this->thermostat_->current_temperature, ts = this->thermostat_->target_temperature;
  const float h = this->hygrostat_->current_temperature, hs = this->hygrostat_->target_temperature;
  this->update_(Alarm::OVERTEMPERATURE, t - ts, this->temp_deviation_, this->temp_hysteresis_, t, ts);
  this->update_(Alarm::UNDERTEMPERATURE, ts - t, this->temp_deviation_, this->temp_hysteresis_, t, ts);
  this->update_(Alarm::LOW_HUMIDITY, hs - h, this->hum_deficit_, this->hum_hysteresis_, h, hs);
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
}

}  // namespace geckoalarm
}  // namespace esphome
