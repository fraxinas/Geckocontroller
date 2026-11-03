#include "geckoweather.h"
#include "esphome/core/log.h"

namespace esphome {
namespace geckoweather {

static const char *const TAG = "geckoweather";

void GeckoWeather::setup() {
  if (this->code_sens_ != nullptr)
    this->code_sens_->add_on_state_callback([this](float x) {
      this->last_live_code_ = (int) x;
      this->set_weather_code((int) x, false);
    });
}

void GeckoWeather::set_weather_code(int code, bool from_override) {
  if (this->override_active_ && !from_override)
    return;  // a manual test override is active; ignore the live sensor
  if (from_override)
    this->override_active_ = true;
  this->resolve_(code, from_override);
}

void GeckoWeather::reapply() {
  if (this->state_.code >= 0)
    this->resolve_(this->state_.code, this->override_active_);
}

void GeckoWeather::clear_override() {
  this->override_active_ = false;
  ESP_LOGD(TAG, "override cleared -> live code %d", this->last_live_code_);
  if (this->last_live_code_ >= 0)
    this->set_weather_code(this->last_live_code_, false);
}

// Look the code up and publish the result. Both outputs are written unconditionally: this component
// reports what the weather is doing, and whether to act on it is each consumer's own business (they
// have their own master enables). Subscribers get the full resolved state last, so they see the
// attenuation global already updated.
void GeckoWeather::resolve_(int code, bool from_override) {
  auto it = this->rules_.find(code);
  const WeatherRule &r = (it != this->rules_.end()) ? it->second : this->default_;

  this->state_ = WeatherState{code, r.attenuation, r.clouds, r.rain, r.effect, from_override};
  ESP_LOGD(TAG, "code %d%s -> attenuation %.2f, clouds %.2f, rain %.1f, effect '%s'", code,
           from_override ? " (override)" : "", r.attenuation, r.clouds, r.rain, r.effect.c_str());

  if (this->attenuation_ != nullptr)
    globals::id(this->attenuation_) = r.attenuation;
  if (this->precip_sens_ != nullptr)
    this->precip_sens_->publish_state(r.rain);
  this->callback_.call(this->state_);
}

void GeckoWeather::dump_config() {
  ESP_LOGCONFIG(TAG, "GeckoWeather:");
  ESP_LOGCONFIG(TAG, "  mapped WMO codes: %u", (unsigned) this->rules_.size());
  ESP_LOGCONFIG(TAG, "  default: attenuation %.2f, rain %.1f", this->default_.attenuation, this->default_.rain);
}

}  // namespace geckoweather
}  // namespace esphome
