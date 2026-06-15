#include "geckolights.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace geckolights {

static const char *const TAG = "geckolights";

#define SUN_LUT_LEN 7
static uint8_t sun_lut[SUN_LUT_LEN][4] = {
  {100, 20, 250, 20},
  {120, 5, 250, 150},
  {140, 5, 5, 250},
  {160, 200, 5, 200},
  {180, 250, 130, 5},
  {200, 250, 200, 20},
  {220, 250, 250, 250}};

// ------------------- SunColorLightEffect -------------------

void SunColorLightEffect::start() {
  ESP_LOGD(TAG, "Starting SunColorLightEffect '%s' %s", this->name_, this->inverse_ ? "inversed" : "");
  this->transition_length_ = this->sun_effect_delay_ * 1000;
  if (this->inverse_) {
    this->index_ = SUN_LUT_LEN;
    this->final_index_ = 0;
    this->incr_ = -1;
  } else {
    this->index_ = 0;
    this->final_index_ = SUN_LUT_LEN;
    this->incr_ = +1;
  }
}

void SunColorLightEffect::apply() {
  const uint32_t now = millis();
  if (now < this->last_color_change_ + this->transition_length_) {
    return;
  }
  auto call = this->scenic_light_->make_call();
  size_t i = this->index_ += this->incr_;
  if (i == this->final_index_) {
    ESP_LOGD(TAG, "SunColorLightEffect '%s' finished", this->name_);
    call.set_effect((uint32_t) 0);
  } else {
    float a, r, g, b;
    a = sun_lut[i][0] / 255.f;
    r = sun_lut[i][1] / 255.f;
    g = sun_lut[i][2] / 255.f;
    b = sun_lut[i][3] / 255.f;
    call.set_brightness(a);
    call.set_rgb(r, g, b);
    call.set_publish(true);
    call.set_transition_length(this->transition_length_);
    call.set_state(true);
    this->last_color_change_ = now;
  }
  call.perform();
}

// ------------------- SunDayLightEffect -------------------

void SunDayLightEffect::start() {
  ESP_LOGD(TAG, "Starting SunDayLightEffect '%s' %s", this->name_, this->inverse_ ? "inversed" : "");
  this->transition_length_ = this->sun_effect_delay_ * 235;
  // with delay of 180 the entire transition will take 3 hours
  if (this->inverse_) {
    globals::id(this->twilight_attenuation_) = 1.0f;
    this->incr_ = (-1.f) / 255.f;
  } else {
    globals::id(this->is_daytime_) = true;
    globals::id(this->twilight_attenuation_) = 0.0f;
    this->incr_ = (+1.f) / 255.f;
  }
}

void SunDayLightEffect::apply() {
  const uint32_t now = millis();
  if (now < this->last_brightness_change_ + this->transition_length_) {
    return;
  }
  auto call = this->day_light_->make_call();
  globals::id(this->twilight_attenuation_) += this->incr_;
  if (!this->inverse_ && globals::id(this->twilight_attenuation_) >= 1.0f) {
    ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_);
    call.set_effect((uint32_t) 0);
  } else if (this->inverse_ && globals::id(this->twilight_attenuation_) <= 0.0f) {
    ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_);
    call.set_state(false);
    globals::id(this->is_daytime_) = false;
  } else {
    this->day_light_->current_values_as_brightness(&this->brightness_);
    ESP_LOGD(TAG, "SunDayLightEffect '%s' apply. current_values_as_brightness=%.3f", this->name_, this->brightness_);
    if (this->inverse_) {
      this->day_light_->current_values_as_brightness(&this->brightness_);
      this->brightness_ /= (globals::id(this->twilight_attenuation_) * globals::id(this->skycond_attenuation_));
    }
    if (!this->inverse_) {
      this->brightness_ = this->sun_rise_max_;
    }
    if (this->brightness_ > 1.0f) {
      this->brightness_ = 1.0f;
    }
    float new_brightness = this->brightness_ * globals::id(this->twilight_attenuation_) * globals::id(this->skycond_attenuation_);
    ESP_LOGD(TAG, "SunDayLightEffect normalized brightness=%.3f, twilight_attenuation=%.3f, skycond_attenuation=%.3f, new_brightness=%.3f",
             this->brightness_, globals::id(this->twilight_attenuation_), globals::id(this->skycond_attenuation_), new_brightness);
    this->brightness_ = new_brightness;
    call.set_brightness(this->brightness_);
    call.set_publish(true);
    call.set_transition_length(this->transition_length_);
    call.set_state(true);
    this->last_brightness_change_ = now;
  }
  call.perform();
}

// ------------------- GeckoLights -------------------

void GeckoLights::setup() {
  auto *sunrise_color = new SunColorLightEffect("Sunrise Color");
  auto *sunset_color = new SunColorLightEffect("Sunset Color");
  for (auto *e : {sunrise_color, sunset_color}) {
    e->set_scenic_light(this->scenic_light_);
    e->set_sun_effect_delay(this->sun_effect_delay_);
  }
  sunset_color->set_inverse(true);
  this->scenic_light_->add_effects({sunrise_color, sunset_color});

  auto *sunrise_day = new SunDayLightEffect("Sunrise Daylight");
  auto *sunset_day = new SunDayLightEffect("Sunset Daylight");
  for (auto *e : {sunrise_day, sunset_day}) {
    e->set_day_light(this->day_light_);
    e->set_twilight_attenuation(this->twilight_attenuation_);
    e->set_skycond_attenuation(this->skycond_attenuation_);
    e->set_is_daytime(this->is_daytime_);
    e->set_sun_effect_delay(this->sun_effect_delay_);
    e->set_sun_rise_max(this->sun_rise_max_);
  }
  sunset_day->set_inverse(true);
  this->day_light_->add_effects({sunrise_day, sunset_day});
  ESP_LOGD(TAG, "Added Sun light effects");
}

void GeckoLights::dump_config() {
  ESP_LOGCONFIG(TAG, "GeckoLights:");
  ESP_LOGCONFIG(TAG, "  sun_effect_delay: %u s", this->sun_effect_delay_);
  ESP_LOGCONFIG(TAG, "  sun_rise_max: %.2f", this->sun_rise_max_);
}

// Thermostat heat_output target: drives the daylight lamp (daytime) or heat lamp (nighttime),
// ported from the old gecko.yaml `heater` write_action lambda.
void GeckoLights::write_state(float state) {
  climate::ClimateMode mode = this->thermostat_->mode;
  if (mode == climate::CLIMATE_MODE_OFF) {
    ESP_LOGD(TAG, "ignored heater write_action because thermostat is off");
  } else if (globals::id(this->is_daytime_) == true) {
    float set_val = this->thermostat_->get_output_value();
    float att_max = globals::id(this->twilight_attenuation_) * globals::id(this->skycond_attenuation_);
    float att_min = att_max / 10.0f;
    ESP_LOGD(TAG, "Thermostat PID Controller daylight write_action: %.3f, set_val: %.3f, att_max: %.3f", state, set_val, att_max);
    float brightness = state;
    if (state > att_max) {
      float max_ratio = (set_val - 1.0f) * 0.1f;
      float att_ratio = (1.0f - max_ratio) * att_max;
      brightness = (att_ratio + max_ratio) * globals::id(this->twilight_attenuation_);
      ESP_LOGD(TAG, "state>att_max. (%.2f%+.2f)*%.2f => brightness: %.2f", att_ratio, max_ratio,
               globals::id(this->twilight_attenuation_), brightness);
    } else if (state < att_min) {
      brightness = att_min;
      ESP_LOGD(TAG, "state<att_min (%.2f) => brightness: %.2f", att_min, brightness);
    }
    auto daylight = this->day_light_->make_call();
    daylight.set_transition_length(30000);
    daylight.set_brightness(brightness);
    daylight.perform();
  } else {
    ESP_LOGD(TAG, "Thermostat PID Controller nighttime heatlamp write_action: %.3f", state);
    auto heatlamp = this->heat_lamp_->make_call();
    heatlamp.set_state(!!state);
    heatlamp.set_brightness(state ? state : 0.001f);
    heatlamp.perform();
  }
}

}  // namespace geckolights
}  // namespace esphome
