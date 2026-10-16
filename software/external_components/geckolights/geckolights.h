#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"

namespace esphome {
namespace geckolights {

// --- Minimal light outputs (equivalent to the built-in monochromatic / rgb platforms),
//     kept self-contained so geckolights can own its light entities. ---

class GeckoBrightnessOutput : public light::LightOutput {
 public:
  void set_output(output::FloatOutput *output) { this->output_ = output; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float bright;
    state->current_values_as_brightness(&bright);
    this->output_->set_level(bright);
  }

 protected:
  output::FloatOutput *output_{nullptr};
};

class GeckoRGBOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { this->red_ = red; }
  void set_green(output::FloatOutput *green) { this->green_ = green; }
  void set_blue(output::FloatOutput *blue) { this->blue_ = blue; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue);
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
  }

 protected:
  output::FloatOutput *red_{nullptr};
  output::FloatOutput *green_{nullptr};
  output::FloatOutput *blue_{nullptr};
};

// --- Sunrise / sunset light effects, ported from the old gecko_custom.h. ---

class SunColorLightEffect : public light::LightEffect {
 public:
  explicit SunColorLightEffect(const char *name) : light::LightEffect(name) {}

  void set_scenic_light(light::LightState *scenic_light) { this->scenic_light_ = scenic_light; }
  void set_sun_effect_delay(uint16_t delay) { this->sun_effect_delay_ = delay; }
  void set_inverse(bool direction) { this->inverse_ = direction; }

  void start() override;
  void apply() override;

 protected:
  light::LightState *scenic_light_{nullptr};
  uint16_t sun_effect_delay_{180};
  bool inverse_{false};
  uint32_t last_color_change_{0};
  uint32_t transition_length_{};
  size_t index_{};
  size_t final_index_{};
  ssize_t incr_{};
};

class SunDayLightEffect : public light::LightEffect {
 public:
  explicit SunDayLightEffect(const char *name) : light::LightEffect(name) {}

  void set_day_light(light::LightState *day_light) { this->day_light_ = day_light; }
  void set_twilight_attenuation(globals::GlobalsComponent<float> *g) { this->twilight_attenuation_ = g; }
  void set_skycond_attenuation(globals::GlobalsComponent<float> *g) { this->skycond_attenuation_ = g; }
  void set_is_daytime(globals::RestoringGlobalsComponent<bool> *g) { this->is_daytime_ = g; }
  void set_sun_effect_delay(uint16_t delay) { this->sun_effect_delay_ = delay; }
  void set_sun_rise_max(float v) { this->sun_rise_max_ = v; }
  void set_inverse(bool direction) { this->inverse_ = direction; }

  void start() override;
  void apply() override;

 protected:
  light::LightState *day_light_{nullptr};
  globals::GlobalsComponent<float> *twilight_attenuation_{nullptr};
  globals::GlobalsComponent<float> *skycond_attenuation_{nullptr};
  globals::RestoringGlobalsComponent<bool> *is_daytime_{nullptr};
  uint16_t sun_effect_delay_{180};
  float sun_rise_max_{0.75f};
  bool inverse_{false};
  uint32_t last_brightness_change_{0};
  uint32_t transition_length_{};
  float brightness_{};
  float incr_{};
};

// --- Owner component: holds the three lights, registers the sun effects, and serves as the
//     thermostat heat_output that drives the daylight / heat lamp (ported heater write_action). ---

class GeckoLights : public Component, public output::FloatOutput {
 public:
  void set_heat_lamp(light::LightState *l) { this->heat_lamp_ = l; }
  void set_day_light(light::LightState *l) { this->day_light_ = l; }
  void set_scenic_light(light::LightState *l) { this->scenic_light_ = l; }
  void set_thermostat(pid::PIDClimate *t) { this->thermostat_ = t; }
  void set_twilight_attenuation(globals::GlobalsComponent<float> *g) { this->twilight_attenuation_ = g; }
  void set_skycond_attenuation(globals::GlobalsComponent<float> *g) { this->skycond_attenuation_ = g; }
  void set_is_daytime(globals::RestoringGlobalsComponent<bool> *g) { this->is_daytime_ = g; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { this->calib_target_ = g; }
  void set_sun_effect_delay(uint16_t delay) { this->sun_effect_delay_ = delay; }
  void set_sun_rise_max(float v) { this->sun_rise_max_ = v; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  void write_state(float state) override;  // thermostat heat_output target

  light::LightState *heat_lamp_{nullptr};
  light::LightState *day_light_{nullptr};
  light::LightState *scenic_light_{nullptr};
  pid::PIDClimate *thermostat_{nullptr};
  globals::GlobalsComponent<float> *twilight_attenuation_{nullptr};
  globals::GlobalsComponent<float> *skycond_attenuation_{nullptr};
  globals::RestoringGlobalsComponent<bool> *is_daytime_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};  // 1 == heating calibration
  uint16_t sun_effect_delay_{180};
  float sun_rise_max_{0.75f};
};

}  // namespace geckolights
}  // namespace esphome
