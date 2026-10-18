#include "geckolights.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include <cmath>

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
  // Master enable off: freeze the effect so the RGB light holds its current state.
  if (this->enabled_ != nullptr && !*this->enabled_) {
    return;
  }
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
  // Master enable off: freeze the effect so the daylight lamp holds its current state.
  if (this->enabled_ != nullptr && !*this->enabled_) {
    return;
  }
  const uint32_t now = millis();
  if (now < this->last_brightness_change_ + this->transition_length_) {
    return;
  }
  // The ramp no longer *commands* the daylight lamp -- it only advances `twilight_attenuation`, the
  // envelope GeckoLights::write_state() clamps the thermostat's heat demand into. That lamp is the
  // daytime heater, so an open-loop brightness trajectory locked the PID out of its own actuator
  // for the three hours a ramp runs: the thermostat sat in "cooling" from mid-morning on while the
  // lamp kept climbing, and the terrarium ended up +8 K over setpoint -- an excess that collapsed
  // within twenty minutes of the effect releasing the lamp. The *visible* sunrise and sunset are
  // carried by the scenic colour sweep and the growlight illumination controller, not by this lamp.
  globals::id(this->twilight_attenuation_) += this->incr_;
  this->last_brightness_change_ = now;
  if (!this->inverse_ && globals::id(this->twilight_attenuation_) >= 1.0f) {
    globals::id(this->twilight_attenuation_) = 1.0f;  // the envelope is a clamp bound: keep it exact
    ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_);
    auto call = this->day_light_->make_call();
    call.set_effect((uint32_t) 0);
    call.perform();
  } else if (this->inverse_ && globals::id(this->twilight_attenuation_) <= 0.0f) {
    globals::id(this->twilight_attenuation_) = 0.0f;
    ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_);
    auto call = this->day_light_->make_call();
    call.set_state(false);
    call.perform();
    globals::id(this->is_daytime_) = false;
  } else {
    // Deliberately no light call: the PID writes the lamp on its next cycle (one per ceiling
    // temperature reading, 15 s) and a brightness written from here would only fight it.
    ESP_LOGD(TAG, "SunDayLightEffect '%s' envelope: twilight_attenuation=%.3f", this->name_,
             globals::id(this->twilight_attenuation_));
  }
}

// ------------------- LightningLightEffect -------------------

void LightningLightEffect::start() {
  ESP_LOGD(TAG, "Starting LightningLightEffect '%s'", this->name_);
  this->flashing_ = true;
  this->first_ = true;
  this->last_apply_ = 0;
}

void LightningLightEffect::apply() {
  if (this->enabled_ != nullptr && !*this->enabled_) {
    return;  // light automation disabled -> freeze
  }
  const uint32_t now = millis();
  if (now - this->last_apply_ < 50) {  // throttle to the original 50 ms update interval
    return;
  }
  this->last_apply_ = now;

  auto call = this->scenic_light_->make_call();
  if (this->first_) {
    call.set_rgb(1.f, 1.f, 1.f);
    call.perform();
    this->first_ = false;
  }
  uint8_t r = 1 + (random_uint32() % 99);  // 1..99 (matches the old random(1,100))
  if (r > 95) {
    call.set_transition_length(5);
    call.set_brightness(1.0f);
    call.perform();
    this->flashing_ = true;
  } else if (this->flashing_) {
    call.set_transition_length(300);
    call.set_brightness(0.01f);
    call.perform();
    this->flashing_ = false;
  }
}

// Uniform random float in [a, b) (random_uint32 from helpers.h).
static float frand(float a, float b) { return a + (b - a) * ((float) random_uint32() / (float) 0xFFFFFFFFu); }

// ------------------- GeckoLights -------------------

void GeckoLights::setup() {
  auto *sunrise_color = new SunColorLightEffect("Sunrise Color");
  auto *sunset_color = new SunColorLightEffect("Sunset Color");
  for (auto *e : {sunrise_color, sunset_color}) {
    e->set_scenic_light(this->scenic_light_);
    e->set_sun_effect_delay(this->sun_effect_delay_);
    e->set_enabled(this->enabled_);
  }
  sunset_color->set_inverse(true);

  auto *lightning = new LightningLightEffect("Lightning");
  lightning->set_scenic_light(this->scenic_light_);
  lightning->set_enabled(this->enabled_);
  this->scenic_light_->add_effects({sunrise_color, sunset_color, lightning});

  auto *sunrise_day = new SunDayLightEffect("Sunrise Daylight");
  auto *sunset_day = new SunDayLightEffect("Sunset Daylight");
  for (auto *e : {sunrise_day, sunset_day}) {
    e->set_day_light(this->day_light_);
    e->set_twilight_attenuation(this->twilight_attenuation_);
    e->set_is_daytime(this->is_daytime_);
    e->set_sun_effect_delay(this->sun_effect_delay_);
    e->set_sun_rise_max(this->sun_rise_max_);
    e->set_enabled(this->enabled_);
  }
  sunset_day->set_inverse(true);

  this->day_light_->add_effects({sunrise_day, sunset_day});
  ESP_LOGD(TAG, "Added Sun light effects");

  // Registry so the weather matrix can route an effect name to the light that owns it.
  // "Daylight Swell" is deliberately absent: passing clouds are no longer a light effect on the
  // daylight lamp (which the PID must keep control of) but a mode of the illumination controller.
  this->effect_lights_["Lightning"] = this->scenic_light_;

  // geckoweather resolves the WMO code; this component reacts to the resolved state.
  if (this->weather_ != nullptr)
    this->weather_->add_on_weather_callback([this](const geckoweather::WeatherState &w) { this->on_weather_(w); });
  if (this->moon_sens_ != nullptr)
    this->moon_sens_->add_on_state_callback([this](float x) { this->on_moon_(x); });

  this->start_illumination_cycle_(millis());
}

// The illumination controller only has to resolve a five-minute cycle, so a 1 s tick is ample and
// keeps the per-loop cost to one comparison.
void GeckoLights::loop() {
  const uint32_t now = millis();
  if (now - this->illum_last_tick_ < 1000)
    return;
  this->illum_last_tick_ = now;
  this->update_quiet_hours_();
  this->update_illumination_();
}

// The weather changed. This component only owns the lamp-facing half of the response: routing the
// named light effect, and telling the illumination controller whether clouds are drifting through.
// The sky attenuation and the rain intensity are already published by geckoweather -- every other
// consumer reads them straight from there.
void GeckoLights::on_weather_(const geckoweather::WeatherState &w) {
  if (this->enabled_ != nullptr && !*this->enabled_)
    return;  // light automation disabled -> lamps stay as they are
  // "Daylight Swell" is not a light effect: it tells the illumination controller that this weather
  // has clouds drifting through it, so the growlight's cycle gusts instead of holding a steady
  // duty. Keep it out of apply_weather_effect_, which only knows about real light effects.
  this->illum_clouds_ = w.effect == "Daylight Swell";
  this->apply_weather_effect_(this->illum_clouds_ ? "" : w.effect);
  // By day the illumination controller picks the new attenuation up on its own tick. By night
  // nothing re-reads it until the moon sensor next changes, which can be hours, so re-render now.
  if (!globals::id(this->is_daytime_) && this->moon_sens_ != nullptr && this->moon_sens_->has_state())
    this->on_moon_(this->moon_sens_->state);
}

// Switch the running weather effect. Stops the previous one (if any) and starts the new one on the
// light that owns it (Lightning -> scenic RGB, Daylight Swell -> day lamp). Empty / "none" = off.
void GeckoLights::apply_weather_effect_(const std::string &effect) {
  std::string want = (effect == "none") ? "" : effect;
  if (want == this->active_weather_effect_)
    return;

  if (!this->active_weather_effect_.empty()) {
    auto prev = this->effect_lights_.find(this->active_weather_effect_);
    if (prev != this->effect_lights_.end()) {
      auto call = prev->second->make_call();
      call.set_effect("None");
      call.perform();
    }
  }
  this->active_weather_effect_ = want;
  if (!want.empty()) {
    auto next = this->effect_lights_.find(want);
    if (next != this->effect_lights_.end()) {
      auto call = next->second->turn_on();
      call.set_effect(want);
      call.perform();
    } else {
      ESP_LOGW(TAG, "weather effect '%s' is not registered", want.c_str());
    }
  }
}

// Is the local wall clock inside the sleeping-hours window -- from quiet_start_min_ until sunrise?
// The far end is simply "the night ended", so daytime closes the window rather than any end time;
// that also keeps the boundary watcher below reporting the end at sunrise instead of at whatever
// hour an end time happened to name. What is left is telling the two sides of midnight apart for a
// window that wraps it, and noon splits those cleanly since no part of the night falls near it.
bool GeckoLights::in_quiet_hours_() {
  if (this->quiet_time_ == nullptr || globals::id(this->is_daytime_))
    return false;
  auto now = this->quiet_time_->now();
  if (!now.is_valid())
    return false;  // no clock yet (boot, before the first sync) -> normal moonlight
  uint16_t minutes = now.hour * 60 + now.minute;
  return minutes >= this->quiet_start_min_ || minutes < 12 * 60;
}

// on_moon_ only runs when the moon or the weather changes, which can be hours apart -- so nothing
// would happen at 23:30 itself. Watch the boundary on the 1 s tick and re-render when it moves.
void GeckoLights::update_quiet_hours_() {
  bool quiet = this->in_quiet_hours_();
  if (quiet == this->quiet_now_)
    return;
  this->quiet_now_ = quiet;
  ESP_LOGI(TAG, "moonlight quiet hours %s (factor %.2f)", quiet ? "begin" : "end", this->quiet_factor_);
  if (this->moon_sens_ != nullptr && this->moon_sens_->has_state())
    this->on_moon_(this->moon_sens_->state);
}

// Historic moon illumination (0-100). At night, with no other scenic effect running, render the
// (sky-attenuated) moonlight on the scenic RGB light.
void GeckoLights::on_moon_(float x) {
  if (this->enabled_ != nullptr && !*this->enabled_)
    return;  // light automation disabled -> freeze
  std::string effect = this->scenic_light_->get_effect_name();
  if (globals::id(this->is_daytime_) == false && effect == "None") {
    auto call = this->scenic_light_->make_call();
    float attenuated_moonlight = (x / 2.0 + x / 2.0 * (globals::id(this->skycond_attenuation_))) / 100.0 + 0.01;
    // Sleeping hours: scale the whole figure, floor included, so the terrarium goes to almost
    // nothing rather than merely dimmer.
    if (this->quiet_now_)
      attenuated_moonlight *= this->quiet_factor_;
    call.set_brightness(attenuated_moonlight);
    call.set_state(x > 0.0 ? true : false);
    call.perform();
  }
}

void GeckoLights::dump_config() {
  ESP_LOGCONFIG(TAG, "GeckoLights:");
  ESP_LOGCONFIG(TAG, "  sun_effect_delay: %u s", this->sun_effect_delay_);
  ESP_LOGCONFIG(TAG, "  sun_rise_max: %.2f", this->sun_rise_max_);
  if (this->quiet_time_ != nullptr)
    ESP_LOGCONFIG(TAG, "  moonlight quiet hours: from %02u:%02u local until sunrise, factor %.2f",
                  this->quiet_start_min_ / 60, this->quiet_start_min_ % 60, this->quiet_factor_);
  if (this->growlight_binary_ != nullptr || this->growlight_light_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  illumination: growlight %s, split %.0f%%, full elevation %.0f deg",
                  this->growlight_light_ != nullptr ? "dimmable" : "switched", this->illum_split_ * 100.0f,
                  this->illum_full_elevation_);
    if (this->growlight_light_ == nullptr)
      ESP_LOGCONFIG(TAG, "  illumination slow-PWM period: %u s", this->illum_period_ms_ / 1000);
  }
}

// Tolerance for "the lamp already sits at this brightness". Brightness reaches the frontend as an
// 8-bit value, so a difference below this is invisible everywhere.
static constexpr float LAMP_EPSILON = 1e-4f;

// The PID re-runs on every ceiling-temperature reading (15 s) and re-writes *both* of its outputs
// each time -- including a plain 0.0 to the heater while it is cooling. The resulting lamp command
// is then usually identical to the one sent one cycle earlier, and re-performing it costs a debug
// line plus a full entity state re-publish over the API without moving the lamp at all.
//
// This predicate lets write_state() issue only real changes.
//
// It compares against remote_values, the *target* of a running transition (constant while the
// transition interpolates current_values), so the comparison is against what the lamp was last
// told to reach. Anything else that moves the lamp -- an effect, HA, the sunrise/sunset handlers --
// updates remote_values too, so the PID still corrects it on its next cycle.
bool GeckoLights::lamp_needs_update_(light::LightState *lamp, bool on, float brightness) {
  const auto &rv = lamp->remote_values;
  if (rv.is_on() != on)
    return true;
  if (!on)
    return false;  // brightness of a lamp that is off and stays off changes nothing
  return std::fabs(rv.get_brightness() - brightness) > LAMP_EPSILON;
}

// Off is a plain set_state(false): the stored brightness is left alone, which is harmless because
// lamp_needs_update_ ignores it while the lamp is off and every turn-on here names its brightness.
bool GeckoLights::drive_lamp_(light::LightState *lamp, float level, optional<uint32_t> transition_ms) {
  level = clamp(level, 0.0f, 1.0f);
  bool on = level > 0.0f;
  if (!lamp_needs_update_(lamp, on, level))
    return false;
  auto call = lamp->make_call();
  if (transition_ms.has_value())
    call.set_transition_length(*transition_ms);
  call.set_state(on);
  if (on)
    call.set_brightness(level);
  call.perform();
  return true;
}

// Thermostat heat_output target: drives the daylight lamp (daytime) or heat lamp (nighttime),
// ported from the old gecko.yaml `heater` write_action lambda.
void GeckoLights::write_state(float state) {
  // During heating calibration the PID relay output is routed here; drive only the targeted lamp
  // directly (no attenuation math) so the autotune relay is clean. The target selects the lamp
  // regardless of actual day/night: 1 == heat_day (daylight lamp), 2 == heat_night (heat lamp).
  // The other heater is forced off so it does not contaminate the autotune.
  int ct = (this->calib_target_ != nullptr) ? globals::id(this->calib_target_) : 0;
  if (ct == 1 || ct == 2) {
    this->logged_thermostat_off_ = false;
    float b = clamp(state, 0.0f, 1.0f);
    auto *active = (ct == 1) ? this->day_light_ : this->heat_lamp_;
    auto *idle = (ct == 1) ? this->heat_lamp_ : this->day_light_;
    bool acted = false;
    if (this->lamp_needs_update_(idle, false, 0.001f)) {
      auto off = idle->make_call();
      off.set_state(false);
      off.set_brightness(0.001f);
      off.perform();
      acted = true;
    }
    bool on = b > 0.0f;
    float level = on ? b : 0.001f;
    if (this->lamp_needs_update_(active, on, level)) {
      auto call = active->make_call();
      call.set_state(on);
      call.set_brightness(level);
      call.perform();
      acted = true;
    }
    // Between relay flips the command is unchanged, so this traces the flips instead of the poll.
    if (acted)
      ESP_LOGD(TAG, "calibration pass-through (target %d): heat output %.2f", ct, b);
    return;
  }
  climate::ClimateMode mode = this->thermostat_->mode;
  if (mode == climate::CLIMATE_MODE_OFF) {
    // Once per transition into "off" -- the PID keeps writing 0.0 here every poll.
    if (!this->logged_thermostat_off_) {
      ESP_LOGD(TAG, "ignored heater write_action because thermostat is off");
      this->logged_thermostat_off_ = true;
    }
  } else if (globals::id(this->is_daytime_) == true) {
    this->logged_thermostat_off_ = false;
    // The twilight ramp is the only thing still capping the lamp. Cloud cover is not: it dims what
    // the terrarium *looks* like, which the growlight now carries, while this lamp is here to make
    // heat. So the lamp simply follows the PID demand -- all the way down, so a satisfied (or
    // cooling) thermostat lets it go dark instead of sitting at a token brightness, and up to
    // whatever the ramp currently allows, with the heat lamp below covering any shortfall.
    float att_max = globals::id(this->twilight_attenuation_);
    // While a Sunrise/Sunset Daylight ramp is running, the ramp is the lamp's *ceiling*, not its
    // command: `sun_rise_max` shapes that ceiling exactly as it used to shape the ramp's own
    // brightness, but the PID is free to sit anywhere below it. So a cold morning still looks like
    // a sunrise -- the demand is saturated and the lamp tracks the envelope -- while a terrarium
    // that is already at temperature simply stays dim instead of being cooked by a cosmetic ramp.
    if (this->day_light_->get_effect_name() != "None")
      att_max *= this->sun_rise_max_;
    float brightness = clamp(state, 0.0f, att_max);
    bool acted = false;
    bool day_on = brightness > 0.0f;
    // Carry the state explicitly: the lamp may now be off, and a brightness-only call cannot switch
    // it back on. 0.001f instead of a literal zero for the same reason as the heat lamp below --
    // set_brightness(0) resets the light's stored brightness to 1.0 and would desync the predicate.
    if (this->lamp_needs_update_(this->day_light_, day_on, brightness)) {
      auto daylight = this->day_light_->make_call();
      daylight.set_transition_length(30000);
      daylight.set_state(day_on);
      daylight.set_brightness(day_on ? brightness : 0.001f);
      daylight.perform();
      acted = true;
    }

    // The daylight lamp is the sole daytime heater. The heat lamp only supplements when the
    // daylight is already saturated at its natural envelope (att_max) and the PID still demands
    // more heat than the daylight can provide; otherwise it must be off (it may carry over a
    // value from the night branch). The supplement covers the demand above att_max, normalized
    // over the remaining headroom.
    float heat_brightness = 0.0f;
    if (state > att_max && att_max < 1.0f) {
      heat_brightness = clamp((state - att_max) / (1.0f - att_max), 0.0f, 1.0f);
    }
    bool heat_on = heat_brightness > 0.0f;
    float heat_level = heat_on ? heat_brightness : 0.001f;
    if (this->lamp_needs_update_(this->heat_lamp_, heat_on, heat_level)) {
      auto heatlamp = this->heat_lamp_->make_call();
      heatlamp.set_state(heat_on);
      heatlamp.set_brightness(heat_level);
      heatlamp.perform();
      acted = true;
    }

    // Logged after the fact, and only when a lamp actually moved: an unchanged poll says nothing.
    if (acted) {
      ESP_LOGD(TAG, "Thermostat PID Controller daylight write_action: %.3f, att_max: %.3f => brightness: %.3f",
               state, att_max, brightness);
      ESP_LOGD(TAG, "daytime heat lamp supplement: %.3f (state %.3f, att_max %.3f)", heat_brightness, state, att_max);
    }
  } else {
    this->logged_thermostat_off_ = false;
    bool on = state != 0.0f;
    float level = on ? state : 0.001f;
    if (this->lamp_needs_update_(this->heat_lamp_, on, level)) {
      ESP_LOGD(TAG, "Thermostat PID Controller nighttime heatlamp write_action: %.3f", state);
      auto heatlamp = this->heat_lamp_->make_call();
      heatlamp.set_state(on);
      heatlamp.set_brightness(level);
      heatlamp.perform();
    }
  }
}

// ------------------- Illumination controller -------------------
//
// How bright the terrarium *looks*. This is a separate job from the daylight lamp, which is the
// daytime heater and now follows the PID demand all the way to zero (a mild, sunny day can leave it
// dark). The light the animals actually get comes from the growlight LED strip, backed by the
// scenic RGB run as a plain white fill.
//
// setpoint = weather illumination factor x sun elevation factor, both 0..1:
//
//   0 .......... split (50%) .......... 1
//   |<- scenic white 0..100% ->|<- growlight duty 0..100%, scenic in antiphase ->|
//
// Below the split the growlight stays off and the (comparatively weak) scenic white carries the
// light alone, ramped so it reaches full exactly at the split. Above it the growlight takes over:
// a switched strip is chopped over illum_period_ms_ (5 min by default) with the scenic white
// filling the off phases, so the terrarium is never unlit and the perceived average lands between
// "scenic only" and "growlight only". A dimmable strip skips the chopping and holds the duty as a
// level, with the scenic white at the complement -- the same time-average, minus the flicker.
//
// The chopping is not a metronome. A fixed 5-minute square wave reads as machinery, and real sky
// does not work that way, so every cycle draws its own length around the nominal period. Under a
// weather code that carries clouds ("Daylight Swell", which used to be a light effect on the
// daylight lamp) the duty gusts from cycle to cycle as well: a thick cloud, then a sunny break.
// Both draws are symmetric around the target, so the mean duty -- and with it the illumination the
// setpoint asked for -- is preserved; only the rhythm is irregular.

// Cross-fade for the scenic white. Short next to a 5 min cycle, long enough not to snap.
static constexpr uint32_t ILLUM_FADE_MS = 1000;
static constexpr float ILLUM_CYCLE_VARIANCE = 0.5f;  // cycle length: nominal period +/- 50%
static constexpr float ILLUM_CLOUD_GUST = 0.25f;     // duty swing per cycle while clouds pass
// Spelled out rather than reusing the Arduino core's DEG_TO_RAD: that one is a double macro,
// which would silently widen the whole sun_factor_ computation.
static constexpr float ILLUM_DEG_PER_RAD = 3.14159265f / 180.0f;

// Sun elevation -> 0..1. sin(elevation) is the geometric daylight strength (cosine of the zenith
// angle); normalising it against illum_full_elevation_ lets midday reach a full 100% at this site
// instead of only in high summer. Below the horizon there is no daylight to render.
float GeckoLights::sun_factor_() {
  if (this->sun_ == nullptr)
    return 1.0f;
  float elevation = (float) this->sun_->elevation();
  if (elevation <= 0.0f)
    return 0.0f;
  float full = std::sin(this->illum_full_elevation_ * ILLUM_DEG_PER_RAD);
  if (full <= 0.0f)
    return 1.0f;
  return clamp(std::sin(elevation * ILLUM_DEG_PER_RAD) / full, 0.0f, 1.0f);
}

// Draw the next growlight cycle: its length, and the duty it runs at. Called at each cycle
// boundary, so a cloud holds for the whole cycle instead of flickering with every 1 s tick.
void GeckoLights::start_illumination_cycle_(uint32_t now) {
  this->illum_cycle_start_ = now;
  this->illum_cycle_ms_ =
      (uint32_t) ((float) this->illum_period_ms_ * frand(1.0f - ILLUM_CYCLE_VARIANCE, 1.0f + ILLUM_CYCLE_VARIANCE));
  this->illum_cycle_duty_ = this->illum_clouds_
                                ? clamp(this->illum_duty_ + frand(-ILLUM_CLOUD_GUST, ILLUM_CLOUD_GUST), 0.0f, 1.0f)
                                : this->illum_duty_;
  this->illum_on_ms_ = (uint32_t) (this->illum_cycle_duty_ * (float) this->illum_cycle_ms_);
  ESP_LOGD(TAG, "illumination: cycle %us at duty %.0f%%%s (target %.0f%%)", this->illum_cycle_ms_ / 1000,
           this->illum_cycle_duty_ * 100.0f, this->illum_clouds_ ? " (clouds)" : "", this->illum_duty_ * 100.0f);
}

void GeckoLights::set_growlight_(bool on) {
  if (this->growlight_light_ != nullptr) {
    this->set_growlight_level_(on ? 1.0f : 0.0f);
    return;
  }
  if (this->growlight_binary_ == nullptr || on == this->illum_growlight_on_)
    return;  // a switched output only needs writing on the flip
  this->illum_growlight_on_ = on;
  if (on) {
    this->growlight_binary_->turn_on();
  } else {
    this->growlight_binary_->turn_off();
  }
  ESP_LOGD(TAG, "illumination: growlight %s (duty %.0f%%, setpoint %.0f%%)", on ? "ON" : "off",
           this->illum_duty_ * 100.0f, this->illum_setpoint_ * 100.0f);
}

// Goes through the light entity, so HA shows the real state and the light's own automations (relay
// power) run. Skipped when the level did not change, e.g. a weather update that only moved the sky.
void GeckoLights::set_growlight_level_(float level) {
  if (this->growlight_light_ == nullptr)
    return;
  level = clamp(level, 0.0f, 1.0f);
  this->illum_growlight_on_ = level > 0.0f;
  drive_lamp_(this->growlight_light_, level, ILLUM_FADE_MS);
}

// The scenic RGB as a white fill. Skipped when the command would not move it, so the 1 s tick does
// not re-publish the entity while a phase holds.
void GeckoLights::drive_scenic_white_(float brightness, bool force) {
  brightness = clamp(brightness, 0.0f, 1.0f);
  bool on = brightness > 0.0f;
  const auto &rv = this->scenic_light_->remote_values;
  // `force` covers taking the light back from an effect: it may have left the brightness where the
  // controller wants it but the colour somewhere else entirely, and brightness alone wouldn't say so.
  if (!force && rv.is_on() == on && (!on || std::fabs(rv.get_brightness() - brightness) <= LAMP_EPSILON))
    return;
  auto call = this->scenic_light_->make_call();
  call.set_transition_length(ILLUM_FADE_MS);
  call.set_state(on);
  if (on) {
    call.set_rgb(1.0f, 1.0f, 1.0f);
    call.set_brightness(brightness);
  }
  call.perform();
}

void GeckoLights::publish_illumination_() {
  if (this->illum_sens_ == nullptr)
    return;
  float pct = this->illum_setpoint_ * 100.0f;
  if (!std::isnan(this->illum_published_) && std::fabs(pct - this->illum_published_) < 1.0f)
    return;  // sub-percent drift is not worth an API round trip
  this->illum_published_ = pct;
  this->illum_sens_->publish_state(pct);
}

// Night, or the controller was switched off: park the growlight and hand the scenic RGB back to the
// moon handler, which owns it after dark.
void GeckoLights::release_illumination_() {
  ESP_LOGD(TAG, "illumination: releasing (night) -> growlight off");
  this->illum_setpoint_ = 0.0f;
  this->illum_duty_ = 0.0f;
  this->set_growlight_(false);
  this->publish_illumination_();
  bool had_scenic = this->illum_scenic_owned_;
  this->illum_scenic_owned_ = false;
  if (had_scenic && this->moon_sens_ != nullptr && this->moon_sens_->has_state())
    this->moon_sens_->publish_state(this->moon_sens_->state);  // re-runs on_moon_ -> scenic moonlight
}

void GeckoLights::update_illumination_() {
  if (this->growlight_binary_ == nullptr && this->growlight_light_ == nullptr)
    return;  // no growlight configured -> feature not in use
  // Same freeze semantics as the lamps and effects: the master enable holds everything where it is
  // rather than switching it off, and the controller simply resumes on a later tick.
  if (this->enabled_ != nullptr && !*this->enabled_)
    return;

  if (!globals::id(this->is_daytime_)) {
    if (this->illum_active_) {
      this->release_illumination_();
      this->illum_active_ = false;
    }
    return;
  }
  this->illum_active_ = true;

  float sun = this->sun_factor_();
  float sky = globals::id(this->skycond_attenuation_);
  float setpoint = clamp(sky * sun, 0.0f, 1.0f);
  bool moved = std::fabs(setpoint - this->illum_setpoint_) > 0.01f;
  this->illum_setpoint_ = setpoint;

  float scenic;
  if (setpoint < this->illum_split_) {
    this->illum_duty_ = 0.0f;
    this->set_growlight_(false);
    scenic = (this->illum_split_ > 0.0f) ? setpoint / this->illum_split_ : 1.0f;
  } else {
    this->illum_duty_ = (setpoint - this->illum_split_) / (1.0f - this->illum_split_);
    const uint32_t now = millis();
    if (now - this->illum_cycle_start_ >= this->illum_cycle_ms_)
      this->start_illumination_cycle_(now);
    if (this->growlight_light_ != nullptr) {
      // Dimmable strip: hold the level. Clouds still gust it, so it swells over the cycle rather
      // than sitting flat -- the same shape the chopping gives, without the switching.
      float level = this->illum_clouds_ ? this->illum_cycle_duty_ : this->illum_duty_;
      this->set_growlight_level_(level);
      scenic = 1.0f - level;
    } else {
      bool on = (now - this->illum_cycle_start_) < this->illum_on_ms_;
      this->set_growlight_(on);
      scenic = on ? 0.0f : 1.0f;
    }
  }

  // The scenic RGB is shared: the sunrise/sunset colour sweeps and Lightning own it while they run,
  // and they are the more interesting thing to look at. Give way, and pick it back up when done.
  bool owned = this->scenic_light_->get_effect_name() == "None";
  bool taken_over = owned && !this->illum_scenic_owned_;
  if (owned != this->illum_scenic_owned_)
    ESP_LOGD(TAG, "illumination: scenic light %s", owned ? "taken over" : "yielded to an effect");
  this->illum_scenic_owned_ = owned;
  if (owned)
    this->drive_scenic_white_(scenic, taken_over);

  if (moved)
    ESP_LOGD(TAG, "illumination: setpoint %.0f%% (sky %.0f%% x sun %.0f%%) -> growlight duty %.0f%%",
             setpoint * 100.0f, sky * 100.0f, sun * 100.0f, this->illum_duty_ * 100.0f);
  this->publish_illumination_();
}

}  // namespace geckolights
}  // namespace esphome
