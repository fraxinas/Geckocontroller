#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/pid/pid_climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sun/sun.h"
#include "esphome/components/time/real_time_clock.h"
#include "../geckoweather/geckoweather.h"
#include <functional>
#include <string>

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

// Base for the effects on the scenic RGB. Between effects the light has a standing owner -- the
// illumination controller by day, which keeps it dark, the moonlight by night -- so every effect reports its end, and
// whether it played to completion or was cut short, and the owner takes the light back.
class ScenicLightEffect : public light::LightEffect {
 public:
  explicit ScenicLightEffect(const char *name) : light::LightEffect(name) {}

  void set_on_stop(std::function<void(bool finished)> &&f) { this->on_stop_ = std::move(f); }
  void stop() override {
    if (this->on_stop_)
      this->on_stop_(this->finished_);
  }

 protected:
  std::function<void(bool)> on_stop_;
  bool finished_{false};
};

// --- Sunrise / sunset light effects, ported from the old gecko_custom.h. ---

class SunColorLightEffect : public ScenicLightEffect {
 public:
  explicit SunColorLightEffect(const char *name) : ScenicLightEffect(name) {}

  void set_scenic_light(light::LightState *scenic_light) { this->scenic_light_ = scenic_light; }
  void set_sun_effect_delay(uint16_t delay) { this->sun_effect_delay_ = delay; }
  void set_enabled(bool *g) { this->enabled_ = g; }
  void set_inverse(bool direction) { this->inverse_ = direction; }

  void start() override;
  void apply() override;

 protected:
  light::LightState *scenic_light_{nullptr};
  bool *enabled_{nullptr};
  uint16_t sun_effect_delay_{180};
  bool inverse_{false};
  uint32_t last_color_change_{0};
  uint32_t transition_length_{};
  int index_{};        // LUT step shown next
  int final_index_{};  // one past the last step, in the direction of travel
  int incr_{};
};

// Thunderstorm flicker on the scenic RGB light (ported from the gecko.yaml "Lightning" lambda
// effect). Triggered by the sky handler when the historic sky value hits the storm marker.
class LightningLightEffect : public ScenicLightEffect {
 public:
  explicit LightningLightEffect(const char *name) : ScenicLightEffect(name) {}

  void set_scenic_light(light::LightState *scenic_light) { this->scenic_light_ = scenic_light; }
  void set_enabled(bool *g) { this->enabled_ = g; }

  void start() override;
  void apply() override;

 protected:
  light::LightState *scenic_light_{nullptr};
  bool *enabled_{nullptr};
  bool flashing_{true};
  bool first_{true};
  uint32_t last_apply_{0};
};

// --- Owner component: holds the three lights, registers the sun effects, and serves as the
//     thermostat heat_output that drives the daylight / heat lamp (ported heater write_action). ---

class GeckoLights : public Component, public output::FloatOutput {
 public:
  void set_heat_lamp(light::LightState *l) { this->heat_lamp_ = l; }
  void set_day_light(light::LightState *l) { this->day_light_ = l; }
  void set_scenic_light(light::LightState *l) { this->scenic_light_ = l; }
  void set_thermostat(pid::PIDClimate *t) { this->thermostat_ = t; }
  void set_skycond_attenuation(globals::GlobalsComponent<float> *g) { this->skycond_attenuation_ = g; }
  void set_is_daytime(globals::RestoringGlobalsComponent<bool> *g) { this->is_daytime_ = g; }
  void set_calibration_target(globals::GlobalsComponent<int> *g) { this->calib_target_ = g; }
  // Master enable: when false, the sun-driven day/night cycles and daylight/RGB effects freeze
  // (the lamps hold their current state). Heating is independent (gated by the thermostat mode).
  // Bound to a `globals:` bool of either flavour: the codegen emits GlobalsComponent<bool> for
  // restore_value: no and the unrelated RestoringGlobalsComponent<bool> for yes. Both own their
  // bool for the life of the app, so store its address and stay agnostic to the choice.
  void set_enabled(globals::GlobalsComponent<bool> *g) { this->enabled_ = &globals::id(g); }
  void set_enabled(globals::RestoringGlobalsComponent<bool> *g) { this->enabled_ = &globals::id(g); }
  // The weather component, subscribed to in setup(). It owns the WMO matrix and the sky attenuation;
  // this component only consumes the half that needs lights -- routing the named effect, and
  // knowing whether the weather has clouds drifting through it.
  void set_weather(geckoweather::GeckoWeather *w) { this->weather_ = w; }
  // Historic moon illumination (0-100) -> scenic moonlight at night.
  void set_moon_sensor(sensor::Sensor *s) { this->moon_sens_ = s; }
  // Quiet hours: from `start` (local wall clock) until the night ends, the moonlight is scaled by
  // `factor` so the terrarium does not glow into the room while people are asleep.
  void set_quiet_time(time::RealTimeClock *t) { this->quiet_time_ = t; }
  void set_quiet_start(uint16_t minutes) { this->quiet_start_min_ = minutes; }
  void set_quiet_factor(float f) { this->quiet_factor_ = f; }
  // Sun elevations (deg): the rising sun's that starts the "Sunrise Color" sweep (day begins once
  // it has played), and the setting sun's that starts "Sunset Color" and with it the night.
  void set_day_elevations(float sunrise, float sunset) {
    this->sunrise_elevation_ = sunrise;
    this->sunset_elevation_ = sunset;
  }
  void set_sun_effect_delay(uint16_t delay) { this->sun_effect_delay_ = delay; }
  // One step of the sunrise/sunset sweeps; also the pace of the growlight <-> sweep cross-fades.
  uint32_t sweep_step_ms_() const { return (uint32_t) this->sun_effect_delay_ * 1000; }

  // --- Illumination controller (see update_illumination_ in the .cpp for the whole scheme) ---
  // How bright the terrarium *looks*, as opposed to how much heat the daylight lamp is making.
  // Driven by the sky attenuation scaled by the sun's elevation, and rendered on the growlight
  // strip alone -- the scenic RGB stays dark by day.
  void set_sun(sun::Sun *s) { this->sun_ = s; }
  // Exactly one of these is wired up at codegen (`growlight:` / `growlight_light:`). A switched
  // strip is slow-PWM'd over illum_period_ms_; a dimmable light is driven to the duty as brightness.
  void set_growlight_binary(output::BinaryOutput *o) { this->growlight_binary_ = o; }
  void set_growlight_light(light::LightState *l) { this->growlight_light_ = l; }
  void set_illumination_period(uint32_t ms) { this->illum_period_ms_ = ms; }
  void set_illumination_full_elevation(float deg) { this->illum_full_elevation_ = deg; }
  void set_illumination_sensor(sensor::Sensor *s) { this->illum_sens_ = s; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  void write_state(float state) override;       // thermostat heat_output target
  void apply_weather_effect_(const std::string &effect);  // switch the weather effect on the scenic RGB
  void on_weather_(const geckoweather::WeatherState &w);   // weather changed -> effect + moonlight
  void on_moon_(float x);                  // moon illumination changed -> scenic moonlight at night
  bool in_quiet_hours_();                  // local clock is inside the sleeping-hours window
  void update_quiet_hours_();              // re-render the moonlight when the window opens/closes
  // "Would this command actually move the lamp?" -- see the definition in geckolights.cpp for why
  // write_state() needs it.
  static bool lamp_needs_update_(light::LightState *lamp, bool on, float brightness);
  // Drive a lamp to `level` (0 = off) if that actually moves it; true when a call was issued.
  static bool drive_lamp_(light::LightState *lamp, float level, optional<uint32_t> transition_ms = {});
  void follow_sun_();                      // sunrise/sunset sweeps and the day/night flip
  void start_scenic_effect_(const char *name);
  void on_scenic_effect_stopped_(bool sunrise_done);  // an effect let go of the scenic RGB

  // --- Illumination controller ---
  void update_illumination_();             // recompute the setpoint and drive the growlight
  void release_illumination_();            // night / disabled: growlight off, scenic back to the moon
  float sun_factor_();                     // sun elevation -> 0..1 daylight strength
  void start_illumination_cycle_(uint32_t now);  // draw the next (randomised) growlight cycle
  void set_growlight_(bool on);            // switched strip (slow-PWM phase)
  void set_growlight_level_(float level, uint32_t fade_ms);  // dimmable light (entity -> relay power)
  // --- Passing clouds ---
  void set_clouds_(float clouds);          // weather changed how much broken cloud drifts across
  void schedule_cloud_();                  // clear gap, then the next cloud
  void cloud_arrives_();                   // one cloud: fade in, hold, fade out
  void render_cloud_(float factor, uint32_t fade_ms);  // move the cloud factor, fading over fade_ms
  void publish_illumination_();

  light::LightState *heat_lamp_{nullptr};
  light::LightState *day_light_{nullptr};
  light::LightState *scenic_light_{nullptr};
  pid::PIDClimate *thermostat_{nullptr};
  globals::GlobalsComponent<float> *skycond_attenuation_{nullptr};
  globals::RestoringGlobalsComponent<bool> *is_daytime_{nullptr};
  globals::GlobalsComponent<int> *calib_target_{nullptr};  // 1/2 == heating calibration (day/night)
  bool *enabled_{nullptr};
  geckoweather::GeckoWeather *weather_{nullptr};
  sensor::Sensor *moon_sens_{nullptr};
  time::RealTimeClock *quiet_time_{nullptr};  // LOCAL clock: this is about the people in the room
  uint16_t quiet_start_min_{23 * 60 + 30};    // minutes past local midnight
  float quiet_factor_{0.5f};                  // moonlight scale inside the window
  bool quiet_now_{false};                     // last evaluated state, to catch the boundary
  std::string active_weather_effect_;                 // currently-running weather effect ("" = none)
  bool thermostat_off_{false};                        // heaters already released for thermostat OFF
  optional<bool> sun_day_{};                          // the sun said "day" at the last sample
  float sunrise_elevation_{5.0f};
  time_t sunrise_swept_{0};                           // when the Sunrise Color sweep last completed
  float sunset_elevation_{5.0f};
  time_t last_sun_sample_{0};                         // UTC time of the previous follow_sun_ pass
  uint16_t sun_effect_delay_{180};

  // --- Illumination controller state ---
  sun::Sun *sun_{nullptr};
  output::BinaryOutput *growlight_binary_{nullptr};   // switched strip (slow-PWM)
  light::LightState *growlight_light_{nullptr};       // dimmable strip (light entity)
  sensor::Sensor *illum_sens_{nullptr};               // optional 0-100% readout of the setpoint
  float illum_setpoint_{0.0f};         // sky x sun x cloud, 0..1 -- the growlight level / duty
  float illum_full_elevation_{45.0f};  // sun elevation (deg) at which daylight counts as full
  uint32_t illum_period_ms_{300000};   // nominal growlight cycle; each cycle varies around it
  uint32_t illum_cycle_start_{0};      // millis() at the start of the current growlight cycle
  uint32_t illum_cycle_ms_{300000};    // length drawn for the current cycle
  uint32_t illum_on_ms_{0};            // growlight on-time within the current cycle
  bool illum_growlight_on_{false};     // last state written to a switched growlight
  bool illum_active_{false};           // controller drove the lights on its last update
  float illum_published_{NAN};         // last value pushed to illum_sens_

  // --- Passing clouds ---
  float clouds_{0.0f};                 // weather: how much broken cloud drifts across, 0..1
  float cloud_factor_{1.0f};           // current cloud dimming on the setpoint, 1 = clear sun
  uint32_t cloud_fade_end_{0};         // millis() at which the running cloud fade completes
  bool cloud_chain_{false};            // a gap -> cloud -> gap timeout chain is running
};

}  // namespace geckolights
}  // namespace esphome
