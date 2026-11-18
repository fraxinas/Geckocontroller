#include "geckolights.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include <cmath>

namespace esphome {
namespace geckolights {

static const char *const TAG = "geckolights";

#define SUN_LUT_LEN 6
static uint8_t sun_lut[SUN_LUT_LEN][4] = {
  {128, 20, 250, 20},
  {153, 5, 250, 150},
  {179, 5, 5, 250},
  {204, 200, 5, 200},
  {230, 250, 130, 5},
  {255, 250, 200, 20}};

// ------------------- SunColorLightEffect -------------------

void SunColorLightEffect::start() {
  ESP_LOGD(TAG, "Starting SunColorLightEffect '%s' %s", this->name_, this->inverse_ ? "inversed" : "");
  this->finished_ = false;
  this->transition_length_ = this->next_step_ms_ != 0 ? this->next_step_ms_ : this->sun_effect_delay_ * 1000;
  this->next_step_ms_ = 0;
  // Due right away: the first step shows on the first apply().
  this->last_color_change_ = millis() - this->transition_length_;
  if (this->inverse_) {
    this->index_ = SUN_LUT_LEN - 1;
    this->final_index_ = -1;
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
  if (now - this->last_color_change_ < this->transition_length_) {
    return;  // difference, not sum: stays correct across the millis() wrap after ~49 days
  }
  auto call = this->scenic_light_->make_call();
  int i = this->index_;
  if (i == this->final_index_) {
    ESP_LOGD(TAG, "SunColorLightEffect '%s' finished", this->name_);
    this->finished_ = true;
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
    this->index_ += this->incr_;
  }
  call.perform();
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

// How often the clock-driven inputs (sun elevation, quiet-hours start) are re-read. The sun climbs
// at most ~0.25 deg a minute, so the illumination setpoint moves by well under a percent between
// samples.
static constexpr uint32_t ILLUM_SAMPLE_MS = 60000;

void GeckoLights::setup() {
  auto *sunrise_color = this->sunrise_color_ = new SunColorLightEffect("Sunrise Color");
  auto *sunset_color = this->sunset_color_ = new SunColorLightEffect("Sunset Color");
  for (auto *e : {sunrise_color, sunset_color}) {
    e->set_scenic_light(this->scenic_light_);
    e->set_sun_effect_delay(this->sun_effect_delay_);
    e->set_enabled(this->enabled_);
  }
  sunset_color->set_inverse(true);
  // Only a sunrise sweep that played to its end starts the day early (see follow_sun_).
  sunrise_color->set_on_stop([this](bool finished) { this->on_scenic_effect_stopped_(finished, true); });
  // The sunset ends the day as it starts (see follow_sun_), so its end hands over to the moonlight.
  sunset_color->set_on_stop([this](bool) { this->on_scenic_effect_stopped_(false, true); });

  auto *lightning = new LightningLightEffect("Lightning");
  lightning->set_scenic_light(this->scenic_light_);
  lightning->set_enabled(this->enabled_);
  lightning->set_on_stop([this](bool) { this->on_scenic_effect_stopped_(false, false); });
  this->scenic_light_->add_effects({sunrise_color, sunset_color, lightning});

  ESP_LOGD(TAG, "Added Sun light effects");

  // geckoweather resolves the WMO code; this component reacts to the resolved state.
  if (this->weather_ != nullptr)
    this->weather_->add_on_weather_callback([this](const geckoweather::WeatherState &w) { this->on_weather_(w); });
  if (this->moon_sens_ != nullptr)
    this->moon_sens_->add_on_state_callback([this](float x) { this->on_moon_(x); });

  // No loop(): everything here is event driven. Weather and moon arrive as callbacks, growlight
  // cycle boundaries as timeouts (start_illumination_cycle_). What is left are the inputs that
  // drift with the clock -- the sun's elevation (day/night, illumination), and the quiet-hours
  // start (minute resolution) -- and those only need sampling at their own pace. A manual
  // day/night or lights_enabled flip is picked up on the same sample.
  this->set_interval("illum_sample", ILLUM_SAMPLE_MS, [this]() {
    this->follow_sun_();
    this->update_quiet_hours_();
    this->update_illumination_();
  });
  if (this->growlight_binary_ != nullptr)
    this->start_illumination_cycle_(millis());
}

// The weather changed. This component only owns the lamp-facing half of the response: the scenic
// light effect, and how much broken cloud the illumination controller lets drift across the sun.
// The sky attenuation and the rain intensity are already published by geckoweather -- every other
// consumer reads them straight from there.
void GeckoLights::on_weather_(const geckoweather::WeatherState &w) {
  if (this->enabled_ != nullptr && !*this->enabled_)
    return;  // light automation disabled -> lamps stay as they are
  this->set_clouds_(w.clouds);
  const bool was_dark = this->dark_scene_();
  this->apply_weather_effect_(w.effect);
  if (this->dark_scene_() != was_dark)
    this->reroute_heat_();  // storm moving in / out: daylight lamp off / back now, not on the next PID cycle
  // Re-render with the new sky attenuation right away: by day through the illumination controller,
  // by night through the moonlight (the moon sensor itself can be hours from its next change).
  if (globals::id(this->is_daytime_)) {
    this->update_illumination_();
  } else if (this->moon_sens_ != nullptr && this->moon_sens_->has_state()) {
    this->on_moon_(this->moon_sens_->state);
  }
}

// Switch the running weather effect on the scenic RGB (Lightning). Empty / "none" = off.
void GeckoLights::apply_weather_effect_(const std::string &effect) {
  std::string want = (effect == "none") ? "" : effect;
  if (want == this->active_weather_effect_) {
    this->resume_weather_effect_();  // e.g. the same storm picked again after a sweep ended it
    return;
  }
  // Stop the previous one only if it still runs: a sunrise/sunset sweep may have replaced it since.
  if (!this->active_weather_effect_.empty() &&
      this->scenic_light_->get_effect_name() == StringRef(this->active_weather_effect_)) {
    auto call = this->scenic_light_->make_call();
    call.set_effect("None");
    call.perform();
  }
  this->active_weather_effect_ = want;
  if (!want.empty()) {
    auto call = this->scenic_light_->turn_on();
    call.set_effect(want);
    call.perform();
  }
}

// A sunrise/sunset sweep replaces a running weather effect; once the scenic RGB is free again and the
// weather still calls for it (a storm outlasting the sunset), it comes back.
void GeckoLights::resume_weather_effect_() {
  if (this->active_weather_effect_.empty() || (this->enabled_ != nullptr && !*this->enabled_))
    return;
  if (this->scenic_light_->get_effect_name() == StringRef("None"))
    this->start_scenic_effect_(this->active_weather_effect_.c_str());
}

// The sun-driven day. The "Sunrise Color" / "Sunset Color" sweeps on the scenic RGB start as the sun
// passes sunrise_elevation_ / sunset_elevation_ -- on a crossing between two passes only, like any
// sun trigger, so a reboot mid-morning does not replay the sunrise; and only with light automation
// on, which is the one place that gates them.
//
// Day begins once the "Sunrise Color" sweep has played: before that
// the daylight lamp must not heat, since on a warm morning it would snap on bright in the middle of
// the sunrise. Normally the sweep itself reports its end (on_scenic_effect_stopped_), so the growlight
// takes over from it seamlessly. Where it doesn't -- after a reboot, or when the sweep
// never ran or was cut short (light automation off, a Lightning weather effect at dawn) -- its end
// is computed: the sun's crossing of the sunrise elevation plus the sweep's fixed length.
//
// Night begins as the "Sunset Color" sweep starts, at sunset_elevation_: from then on the scene is
// the sunset's, and the bright daylight lamp would wash it out, so the invisible heat lamp takes
// over the heating right there. The growlight is still lit at that point and cross-fades into the
// sweep's first colour (release_illumination_), so the terrarium never goes dark in between.
//
// Only a change in that verdict writes is_daytime, so the "Daytime" switch overrides
// until the next hand-over; the first valid sample after boot always counts as a change, which
// aligns the flag restored from flash with the sun. Deliberately not gated by the light-automation
// enable: which lamp heats is the thermostat's business, and write_state() hands over between the
// two on its next cycle.
void GeckoLights::follow_sun_() {
  if (this->sun_ == nullptr)
    return;  // no sun configured: day/night is only set from outside
  auto *clock = this->sun_->get_time();
  if (clock == nullptr || !clock->now().is_valid())
    return;  // no sun position before the first time sync
  // Sun::sunrise/sunset(date) return the next event at or after `date`, so asking from UTC midnight
  // yields today's. At this longitude the local day's sunrise and sunset share its UTC date.
  time_t now = clock->utcnow().timestamp;
  ESPTime midnight = ESPTime::from_epoch_utc(now - now % 86400);
  auto rise = this->sun_->sunrise(midnight, this->sunrise_elevation_);
  auto dusk = this->sun_->sunset(midnight, this->sunset_elevation_);
  if (!rise.has_value() || !dusk.has_value())
    return;

  time_t last = this->last_sun_sample_;
  this->last_sun_sample_ = now;
  bool lights_on = this->enabled_ == nullptr || *this->enabled_;
  if (last != 0 && lights_on) {
    if (last < rise->timestamp && rise->timestamp <= now) {
      ESP_LOGI(TAG, "Good morning, Gecko.");
      this->start_scenic_effect_("Sunrise Color");
    }
    if (last < dusk->timestamp && dusk->timestamp <= now) {
      ESP_LOGI(TAG, "Good night, Gecko.");
      this->start_scenic_effect_("Sunset Color");
    }
  }

  // The sweep shows each of the SUN_LUT_LEN steps for one sun_effect_delay_, the first right at the
  // crossing, which this pass sees up to a minute late.
  time_t sweep_s = SUN_LUT_LEN * this->sun_effect_delay_ + 60;
  bool swept = this->sunrise_swept_ >= rise->timestamp;
  bool day = (swept || now >= rise->timestamp + sweep_s) && now < dusk->timestamp;
  if (this->sun_day_.has_value() && *this->sun_day_ == day)
    return;
  this->sun_day_ = day;
  if (globals::id(this->is_daytime_) == day)
    return;
  ESP_LOGI(TAG, "%s -> %s", day ? "sunrise sweep done" : "sunset begins", day ? "DAY" : "NIGHT");
  globals::id(this->is_daytime_) = day;
}

void GeckoLights::start_scenic_effect_(const char *name) {
  auto call = this->scenic_light_->turn_on();
  call.set_effect(name);
  call.perform();
}

// An effect let go of the scenic RGB, finished or cut short: hand it back to its standing owner.
// Deferred, because stop() runs inside the LightCall that ended the effect, which would overwrite a
// call issued from here -- and if that call started another effect, the owners see it and keep out.
void GeckoLights::on_scenic_effect_stopped_(bool sunrise_done, bool sweep) {
  // A test sunrise does not start the day.
  if (sunrise_done && this->test_effect_ == nullptr && this->sun_ != nullptr && this->sun_->get_time() != nullptr)
    this->sunrise_swept_ = this->sun_->get_time()->utcnow().timestamp;
  this->defer("scenic_release", [this, sweep]() {
    // The test sweep is over once its effect no longer runs (this stop may also be the effect it
    // replaced).
    if (this->test_effect_ != nullptr && !(this->scenic_light_->get_effect_name() == StringRef(this->test_effect_))) {
      this->end_test_sweep_();
      return;
    }
    this->follow_sun_();
    if (sweep)
      this->resume_weather_effect_();
    if (globals::id(this->is_daytime_)) {
      this->update_illumination_();
    } else if (this->moon_sens_ != nullptr && this->moon_sens_->has_state()) {
      this->on_moon_(this->moon_sens_->state);
    }
  });
}

void GeckoLights::test_sweep(bool sunrise) {
  const char *name = sunrise ? "Sunrise Color" : "Sunset Color";
  if (this->enabled_ != nullptr && !*this->enabled_) {
    ESP_LOGW(TAG, "test %s: light automation is off", name);
    return;
  }
  const int ct = this->calib_target_ != nullptr ? globals::id(this->calib_target_) : 0;
  if (ct == 1 || ct == 2) {
    ESP_LOGW(TAG, "test %s: heating calibration in progress", name);
    return;
  }
  ESP_LOGI(TAG, "test %s: %u s steps, daylight lamp held off", name, (unsigned) (TEST_SWEEP_STEP_MS / 1000));
  this->test_effect_ = name;
  this->test_pace_ = true;
  const uint32_t step = this->sweep_step_ms_();
  this->reroute_heat_();
  // By day the growlight fades out with the sweep's first colour, as at a real sunset; it comes
  // back with the cross-fade after the sweep (update_illumination_ sees a takeover).
  if (this->illum_active_) {
    this->illum_setpoint_ = 0.0f;
    if (this->growlight_light_ != nullptr)
      this->set_growlight_level_(0.0f, step);
    else
      this->set_growlight_(false);
    this->illum_active_ = false;
    this->publish_illumination_();
  }
  (sunrise ? this->sunrise_color_ : this->sunset_color_)->set_next_step_ms(TEST_SWEEP_STEP_MS);
  this->start_scenic_effect_(name);
}

// Back to the real time of day: growlight (cross-fading in at the test pace) or moonlight, and the
// daylight lamp to the thermostat's current demand.
void GeckoLights::end_test_sweep_() {
  ESP_LOGI(TAG, "test %s over", this->test_effect_);
  this->test_effect_ = nullptr;
  this->follow_sun_();
  this->resume_weather_effect_();
  if (globals::id(this->is_daytime_)) {
    this->update_illumination_();
  } else if (this->moon_sens_ != nullptr && this->moon_sens_->has_state()) {
    this->on_moon_(this->moon_sens_->state);
  }
  this->test_pace_ = false;
  this->reroute_heat_();
}

bool GeckoLights::dark_scene_() const {
  if (this->enabled_ != nullptr && !*this->enabled_)
    return false;  // light automation off: no scene to protect
  return this->test_effect_ != nullptr || !this->active_weather_effect_.empty();
}

void GeckoLights::reroute_heat_() {
  if (this->last_heat_output_.has_value())  // nothing to route before the thermostat's first write
    this->write_state(*this->last_heat_output_);
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
// would happen at 23:30 itself. Watch the boundary on the illumination sample and re-render when
// it moves.
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
  if (this->sun_ != nullptr)
    ESP_LOGCONFIG(TAG, "  sun: sunrise sweep at %.0f deg, day from its end; sunset sweep and night at %.0f deg",
                  this->sunrise_elevation_, this->sunset_elevation_);
  if (this->quiet_time_ != nullptr)
    ESP_LOGCONFIG(TAG, "  moonlight quiet hours: from %02u:%02u local until sunrise, factor %.2f",
                  this->quiet_start_min_ / 60, this->quiet_start_min_ % 60, this->quiet_factor_);
  if (this->growlight_binary_ != nullptr || this->growlight_light_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  illumination: growlight %s, full elevation %.0f deg",
                  this->growlight_light_ != nullptr ? "dimmable" : "switched", this->illum_full_elevation_);
    if (this->growlight_light_ == nullptr)
      ESP_LOGCONFIG(TAG, "  illumination slow-PWM period: %u s", this->illum_period_ms_ / 1000);
  }
}

// Tolerance for "the lamp already sits at this brightness". Brightness reaches the frontend as an
// 8-bit value, so a difference below this is invisible everywhere.
static constexpr float LAMP_EPSILON = 1e-4f;

// The daylight lamp is a large incandescent heater: fade it rather than snapping it.
static constexpr uint32_t DAYLIGHT_FADE_MS = 30000;
// Growlight and daylight lamp out as the scene goes dark (a storm moving in), and the growlight back.
static constexpr uint32_t DARK_FADE_MS = 5000;
// The daylight lamp set by hand follows its slider promptly.
static constexpr uint32_t MANUAL_FADE_MS = 1000;

// The PID re-runs on every ceiling-temperature reading (15 s) and re-writes *both* of its outputs
// each time -- including a plain 0.0 to the heater while it is cooling. The resulting lamp command
// is then usually identical to the one sent one cycle earlier, and re-performing it costs a debug
// line plus a full entity state re-publish over the API without moving the lamp at all.
//
// This predicate lets write_state() issue only real changes.
//
// It compares against remote_values, the *target* of a running transition (constant while the
// transition interpolates current_values), so the comparison is against what the lamp was last
// told to reach. Anything else that moves the lamp -- an effect, HA -- updates remote_values too, so
// the PID still corrects it on its next cycle.
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
  this->last_heat_output_ = state;  // also on the calibration / OFF paths: re-routing replays it
  // During heating calibration the PID relay output is routed here; drive only the targeted lamp
  // directly (no attenuation math) so the autotune relay is clean. The target selects the lamp
  // regardless of actual day/night: 1 == heat_day (daylight lamp), 2 == heat_night (heat lamp).
  // The other heater is forced off so it does not contaminate the autotune.
  int ct = (this->calib_target_ != nullptr) ? globals::id(this->calib_target_) : 0;
  bool calibrating = ct == 1 || ct == 2;
  // Thermostat OFF means no heating, as with any thermostat: both heaters go off -- once, on
  // entering OFF. After that the PID's writes (a plain 0.0 every poll) are ignored, so the lamps are
  // free for manual control from HA until the thermostat is switched back on.
  // A daylight lamp set by hand ("Daylight Manual") keeps its level through this too.
  const optional<float> manual = this->daylight_manual_;
  if (!calibrating && this->thermostat_->mode == climate::CLIMATE_MODE_OFF) {
    if (!this->thermostat_off_) {
      this->thermostat_off_ = true;
      if (!manual.has_value())
        drive_lamp_(this->day_light_, 0.0f, DAYLIGHT_FADE_MS);
      drive_lamp_(this->heat_lamp_, 0.0f);
      ESP_LOGI(TAG, "thermostat off -> heaters off, lamps left to manual control");
    }
    if (manual.has_value())
      drive_lamp_(this->day_light_, *manual, MANUAL_FADE_MS);
    return;
  }
  this->thermostat_off_ = false;

  if (calibrating) {
    float b = clamp(state, 0.0f, 1.0f);
    auto *active = (ct == 1) ? this->day_light_ : this->heat_lamp_;
    auto *idle = (ct == 1) ? this->heat_lamp_ : this->day_light_;
    bool acted = drive_lamp_(idle, 0.0f);
    acted |= drive_lamp_(active, b);
    // Between relay flips the command is unchanged, so this traces the flips instead of the poll.
    if (acted)
      ESP_LOGD(TAG, "calibration pass-through (target %d): heat output %.2f", ct, b);
    return;
  }
  // One heater at a time. By day the daylight lamp follows the PID demand 0..100%, all the way
  // down, so a satisfied (or cooling) thermostat lets it go dark; it carries no sunrise/sunset ramp
  // and no cloud dimming -- what the terrarium *looks* like is the growlight's job (illumination
  // controller below). By night -- which starts with the sunset sweep -- the heat lamp does the same. Each side also switches the other
  // heater off, so a day/night flip from any source hands over on the next PID cycle.
  // A dark scene (thunderstorm, test sweep) keeps the bright daylight lamp off by day too -- it would
  // wash out the lightning -- and the heat lamp carries the demand as at night; storms last hours.
  // A daylight lamp set by hand holds its level through all of this; the heat lamp is unaffected.
  float level = clamp(state, 0.0f, 1.0f);
  const bool dark = this->dark_scene_();
  const bool daylight = globals::id(this->is_daytime_) && !dark;
  const float day_level = manual.has_value() ? *manual : (daylight ? level : 0.0f);
  const uint32_t day_fade = manual.has_value() ? MANUAL_FADE_MS : (daylight || !dark ? DAYLIGHT_FADE_MS : DARK_FADE_MS);
  bool acted = drive_lamp_(this->day_light_, day_level, day_fade);
  acted |= drive_lamp_(this->heat_lamp_, daylight ? 0.0f : level);
  // Logged after the fact, and only when a lamp actually moved: an unchanged poll says nothing.
  if (acted)
    ESP_LOGD(TAG, "Thermostat PID Controller %s write_action: %.3f%s", daylight ? "daylight" : "heat lamp", state,
             manual.has_value() ? " (daylight lamp manual)" : "");
}

void GeckoLights::set_daylight_manual(optional<float> level) {
  if (level.has_value())
    level = clamp(*level, 0.0f, 1.0f);
  const bool was = this->daylight_manual_.has_value();
  this->daylight_manual_ = level;
  if (level.has_value()) {
    if (!was)
      ESP_LOGI(TAG, "daylight lamp manual: %.0f%%", *level * 100.0f);
  } else if (was) {
    ESP_LOGI(TAG, "daylight lamp back to the thermostat");
    // With the thermostat off, let its OFF path switch the released lamp off like on entering OFF.
    this->thermostat_off_ = false;
  }
  if (this->last_heat_output_.has_value())
    this->reroute_heat_();  // now, not on the next PID cycle
  else if (level.has_value())
    drive_lamp_(this->day_light_, *level, MANUAL_FADE_MS);  // before the thermostat's first write
}

// ------------------- Illumination controller -------------------
//
// How bright the terrarium *looks*. This is a separate job from the daylight lamp, which is the
// daytime heater and now follows the PID demand all the way to zero (a mild, sunny day can leave it
// dark). The light the animals actually get is the growlight LED strip's, and the growlight's alone:
//
//   setpoint = weather illumination factor x sun elevation factor x passing cloud, all 0..1
//
// A dimmable strip simply holds the setpoint as its level. A switched one is chopped over
// illum_period_ms_ (5 min by default) at the setpoint as its duty. That chopping is not a metronome:
// a fixed 5-minute square wave reads as machinery, and real sky does not work that way, so every
// cycle draws its own length around the nominal period. The draw is symmetric, so the mean duty --
// and with it the illumination the setpoint asked for -- is preserved; only the rhythm is irregular.
//
// The scenic RGB is not daylight. Run as a white fill it lit the low-sun hours as a dim, flat glow
// that went nearly black before the sunset even began -- and then the sunset's colours flared up out
// of it. By day it stays dark; it is there for the sunrise/sunset sweeps, Lightning and the moon.

// Growlight fade between samples. The sun moves it by well under a percent a minute, so short.
static constexpr uint32_t ILLUM_FADE_MS = 1000;
static constexpr float ILLUM_CYCLE_VARIANCE = 0.5f;  // cycle length: nominal period +/- 50%
// Spelled out rather than reusing the Arduino core's DEG_TO_RAD: that one is a double macro,
// which would silently widen the whole sun_factor_ computation.
static constexpr float ILLUM_DEG_PER_RAD = 3.14159265f / 180.0f;

// Sun elevation -> 0..1. sin(elevation) is the geometric daylight strength (cosine of the zenith
// angle); normalising it against illum_full_elevation_ lets midday reach a full 100% at this site
// instead of only in high summer. Below the horizon there is no daylight to render. The day itself
// ends well above it, at the sunset elevation, with the growlight still lit for the sweep to take
// over from.
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

// Switched strip only: draw the next slow-PWM cycle -- its length, and the on-time at the current
// duty. Called at each cycle boundary; the cycle's own edges are scheduled here, so they land on
// time rather than on the next sample.
void GeckoLights::start_illumination_cycle_(uint32_t now) {
  this->illum_cycle_start_ = now;
  this->illum_cycle_ms_ =
      (uint32_t) ((float) this->illum_period_ms_ * frand(1.0f - ILLUM_CYCLE_VARIANCE, 1.0f + ILLUM_CYCLE_VARIANCE));
  this->illum_on_ms_ = (uint32_t) (this->illum_setpoint_ * (float) this->illum_cycle_ms_);
  this->set_timeout("illum_cycle", this->illum_cycle_ms_, [this]() { this->update_illumination_(); });
  this->set_timeout("illum_phase", this->illum_on_ms_, [this]() { this->update_illumination_(); });
  ESP_LOGD(TAG, "illumination: cycle %us at duty %.0f%%", this->illum_cycle_ms_ / 1000,
           this->illum_setpoint_ * 100.0f);
}

void GeckoLights::set_growlight_(bool on) {
  if (this->growlight_light_ != nullptr) {
    this->set_growlight_level_(on ? 1.0f : 0.0f, ILLUM_FADE_MS);
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
  ESP_LOGD(TAG, "illumination: growlight %s (setpoint %.0f%%)", on ? "ON" : "off", this->illum_setpoint_ * 100.0f);
}

// Goes through the light entity, so HA shows the real state and the light's own automations (relay
// power) run. Skipped when the level did not change, e.g. a weather update that only moved the sky.
void GeckoLights::set_growlight_level_(float level, uint32_t fade_ms) {
  if (this->growlight_light_ == nullptr)
    return;
  level = clamp(level, 0.0f, 1.0f);
  this->illum_growlight_on_ = level > 0.0f;
  drive_lamp_(this->growlight_light_, level, fade_ms);
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
// moon handler, which owns it after dark. Night normally starts together with the "Sunset Color"
// sweep, so a dimmable growlight fades out at the sweep's pace while its first colour fades in.
void GeckoLights::release_illumination_() {
  ESP_LOGD(TAG, "illumination: releasing (night) -> growlight off");
  this->illum_setpoint_ = 0.0f;
  if (this->growlight_light_ != nullptr) {
    this->set_growlight_level_(0.0f, this->sweep_step_ms_());
  } else {
    this->set_growlight_(false);
  }
  this->publish_illumination_();
  // Scenic moonlight. on_moon_ keeps out of a running effect, which renders it when it stops.
  if (this->moon_sens_ != nullptr && this->moon_sens_->has_state())
    this->on_moon_(this->moon_sens_->state);
}

void GeckoLights::update_illumination_() {
  if (this->growlight_binary_ == nullptr && this->growlight_light_ == nullptr)
    return;  // no growlight configured -> feature not in use
  // Same freeze semantics as the lamps and effects: the master enable holds everything where it is
  // rather than switching it off, and the controller simply resumes on a later update.
  if (this->enabled_ != nullptr && !*this->enabled_)
    return;
  if (this->test_effect_ != nullptr)
    return;  // a test sweep owns the scene until it ends

  if (!globals::id(this->is_daytime_)) {
    if (this->illum_active_) {
      this->release_illumination_();
      this->illum_active_ = false;
    }
    return;
  }

  float sun = this->sun_factor_();
  float sky = globals::id(this->skycond_attenuation_);
  float setpoint = clamp(sky * sun * this->cloud_factor_, 0.0f, 1.0f);
  // A weather effect (Lightning) plays on the scenic RGB and needs a dark scene: the growlight is
  // off for as long as the weather calls for one, even while a sweep has the scenic RGB.
  const bool dark = this->dark_scene_();
  if (dark)
    setpoint = 0.0f;
  // While a cloud fades, every render -- the cloud's own and any sample in between -- fades over
  // what is left of it; a fixed short fade would snap the growlight to the cloud's end state.
  const uint32_t now = millis();
  int32_t fade_left = (int32_t) (this->cloud_fade_end_ - now);
  uint32_t fade = fade_left > (int32_t) ILLUM_FADE_MS ? (uint32_t) fade_left : ILLUM_FADE_MS;
  // Taking the day over -- typically from the sunrise sweep, which ends on a bright warm amber --
  // cross-fades at the sweep's own pace: the growlight comes up as the scenic RGB goes down.
  uint32_t handover = this->sweep_step_ms_();
  if (!this->illum_active_ && handover > fade)
    fade = handover;
  if (dark != this->illum_dark_ && DARK_FADE_MS > fade)
    fade = DARK_FADE_MS;
  this->illum_dark_ = dark;
  this->illum_active_ = true;
  bool moved = std::fabs(setpoint - this->illum_setpoint_) > 0.01f;
  this->illum_setpoint_ = setpoint;

  if (this->growlight_light_ != nullptr) {
    this->set_growlight_level_(setpoint, fade);
  } else {
    if (now - this->illum_cycle_start_ >= this->illum_cycle_ms_)
      this->start_illumination_cycle_(now);
    this->set_growlight_((now - this->illum_cycle_start_) < this->illum_on_ms_);
  }

  // The scenic RGB stays dark by day. The sunrise/sunset sweeps and Lightning own it while they run;
  // keep out of those, and put it out once they hand it back, at the sweep's pace.
  if (this->scenic_light_->get_effect_name() == "None")
    drive_lamp_(this->scenic_light_, 0.0f, handover);

  if (moved && dark)
    ESP_LOGD(TAG, "illumination: growlight off for the '%s' weather effect", this->active_weather_effect_.c_str());
  else if (moved)
    ESP_LOGD(TAG, "illumination: growlight %.0f%% (sky %.0f%% x sun %.0f%% x cloud %.0f%%)", setpoint * 100.0f,
             sky * 100.0f, sun * 100.0f, this->cloud_factor_ * 100.0f);
  this->publish_illumination_();
}

// ------------------- Passing clouds -------------------
//
// Broken cloud drifting across the sun, on top of the steady sky the weather's oktas already set.
// The weather's `clouds` (0..1) says how much of it there is: the more, the shorter the clear gaps
// and the deeper the clouds. Each cloud is one event -- it slides in, sits, and slides off again --
// rendered by fading the illumination setpoint, and with it the growlight.
// The daylight lamp is not involved: it is the heater and the PID's alone.

static constexpr float CLOUD_MAX_DEPTH = 0.7f;              // deepest cloud at clouds = 100%
static constexpr float CLOUD_MIN_DEPTH_SHARE = 0.4f;        // a cloud's depth: 40..100% of the max
static constexpr uint32_t CLOUD_GAP_MIN_MS = 30000;         // clear sky between clouds, at least
static constexpr uint32_t CLOUD_GAP_SPREAD_MS = 8 * 60000;  // ... plus up to this at clouds -> 0
static constexpr uint32_t CLOUD_FADE_MIN_MS = 30000, CLOUD_FADE_MAX_MS = 90000;  // sliding in / off
static constexpr uint32_t CLOUD_HOLD_MIN_MS = 20000, CLOUD_HOLD_MAX_MS = 120000;  // fully across

static uint32_t frand_ms(uint32_t a, uint32_t b) { return (uint32_t) frand((float) a, (float) b); }

void GeckoLights::set_clouds_(float clouds) {
  this->clouds_ = clamp(clouds, 0.0f, 1.0f);
  // A cloud already under way finishes on its own; the chain stops at the next gap once clear.
  if (this->clouds_ > 0.0f && !this->cloud_chain_) {
    this->cloud_chain_ = true;
    this->schedule_cloud_();
  }
}

void GeckoLights::schedule_cloud_() {
  uint32_t gap = CLOUD_GAP_MIN_MS + (uint32_t) (frand(0.0f, 1.0f) * (1.0f - this->clouds_) * CLOUD_GAP_SPREAD_MS);
  this->set_timeout("cloud", gap, [this]() { this->cloud_arrives_(); });
}

void GeckoLights::cloud_arrives_() {
  if (this->clouds_ <= 0.0f) {
    this->cloud_chain_ = false;  // the sky cleared during the gap
    return;
  }
  float depth = CLOUD_MAX_DEPTH * this->clouds_ * frand(CLOUD_MIN_DEPTH_SHARE, 1.0f);
  uint32_t fade_in = frand_ms(CLOUD_FADE_MIN_MS, CLOUD_FADE_MAX_MS);
  uint32_t hold = frand_ms(CLOUD_HOLD_MIN_MS, CLOUD_HOLD_MAX_MS);
  uint32_t fade_out = frand_ms(CLOUD_FADE_MIN_MS, CLOUD_FADE_MAX_MS);
  ESP_LOGD(TAG, "cloud: %.0f%% deep, %us in, %us across, %us off", depth * 100.0f, fade_in / 1000, hold / 1000,
           fade_out / 1000);
  this->render_cloud_(1.0f - depth, fade_in);
  this->set_timeout("cloud", fade_in + hold, [this, fade_out]() {
    this->render_cloud_(1.0f, fade_out);
    this->set_timeout("cloud", fade_out, [this]() { this->schedule_cloud_(); });
  });
}

void GeckoLights::render_cloud_(float factor, uint32_t fade_ms) {
  this->cloud_factor_ = factor;
  this->cloud_fade_end_ = millis() + fade_ms;
  this->update_illumination_();  // by night / frozen: a no-op, the factor is picked up later
}

}  // namespace geckolights
}  // namespace esphome
