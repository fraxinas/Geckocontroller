// Geckoconsole UI: navigation, input handling and region-based rendering for the 320x240
// ILI9341 (mipi_spi, 2-stripe buffer) of gecko-console.yaml.
//
// Included by gecko-console.yaml via `esphome: includes:`. ESPHome adds includes at the end of
// the generated main.cpp, after all component and global declarations, so this file can use id()
// (which returns a reference here; inside YAML lambdas id() yields the pointer).
//
// Rendering model
// ---------------
// Nothing draws directly. State changes mark areas dirty (invalidate()) or the whole screen
// (invalidate_all()); flush() -- called from a short interval -- turns that into display updates:
//  * full redraw: one update(); the display lambda runs once per buffer stripe and draws the page,
//    clipped by hand to the stripe (shapes are drawn pixel by pixel, so off-stripe drawing costs
//    as much as real drawing).
//  * region redraw: one update() per dirty box; the lambda draws the page clipped to the box.
//    mipi_spi only sends the bounding box of the pixels actually drawn. Requires
//    auto_clear_enabled: false.
//    mipi_spi quirk: update() stops at the first stripe in which nothing was drawn. A box lying
//    entirely in a later stripe would never be sent, so in such an earlier stripe one pixel of the
//    page (redrawn with its correct content) keeps the update going.
//    mipi_spi stride quirk (ESPHome 2026.5): for a dirty box not starting at x = 0 it advances
//    the buffer pointer by (width - x_low) instead of the full width per row, shearing the rows.
//    Every region is therefore widened to start at x = 0 (its right edge stays), which makes the
//    step equal the buffer width. Fast-changing elements (the colour wheel) sit at the left edge.
//
// Interaction model
// -----------------
// Start page (live sky, gecko_scene.h) -> Menu (4x2 tiles) -> subpage. The DevKitC console is operated
// by touch alone; turn()/push()/back() still serve a rotary encoder and a back button if one is wired.
// On a subpage the encoder moves the focus between its
// controls and the header's back arrow; push goes back, toggles/presses, or enters edit mode for sliders and the colour wheel (turn
// changes the value, push confirms; the wheel edits hue, then saturation). Touch acts directly:
// tap toggles/presses, drag on a slider or the wheel sets the value, a horizontal swipe switches
// to the neighbouring subpage (Settings is left out of that ring). Back (button 2 / header arrow) leaves edit mode, then the page.
// Values set locally are shown immediately and held for a few seconds while Home Assistant
// echoes them back; sends while dragging/turning are throttled.
#pragma once

#include "esphome.h"
#include "gecko_history.h"
#include "gecko_scene.h"
#include "esphome/core/version.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

namespace gecko_ui {

using namespace esphome;
using display::Display;
using display::TextAlign;

static const char *const TAG = "gecko_ui";

static constexpr int W = 320;
static constexpr int H = 240;
static constexpr int STRIPES = 2;  // keep in sync with buffer_size of the display (50%)
static constexpr int HEADER_H = 26;
static constexpr uint32_t IDLE_TIMEOUT_MS = 60000;
static constexpr uint32_t HOLD_MS = 4000;       // show a locally set value this long
static constexpr uint32_t SEND_INTERVAL_MS = 300;  // throttle HA calls while dragging/turning
static constexpr uint32_t CONFIRM_MS = 3000;
static constexpr int SWIPE_MIN_DX = 60;

// Home Assistant entities of the Geckocontroller
static const char *const E_RAIN_AUTO = "switch.geckocontrol_rain_automation";
static const char *const E_LIGHT_AUTO = "switch.geckocontrol_light_automation";
static const char *const E_WIND_AUTO = "switch.geckocontrol_multifan_auto";
static const char *const E_LET_IT_RAIN = "button.geckocontrol_let_it_rain";
static const char *const E_REFILLED = "button.geckocontrol_reservoir_refilled";
static const char *const E_GROW_LIGHT = "light.geckocontrol_grow_light";
static const char *const E_SCENIC_LIGHT = "light.geckocontrol_scenic_light";
static const char *const E_WIND_SPEED = "number.geckocontrol_fan_manual_speed";
static const char *const E_WEATHER_SET = "switch.geckocontrol_weather_setpoint";
static const char *const E_THERMOSTAT = "climate.geckocontrol_thermostat_air";
static const char *const E_SUNRISE_TEST = "button.geckocontrol_sunrise_test";
static const char *const E_SUNSET_TEST = "button.geckocontrol_sunset_test";
static const char *const E_WEATHER_OVERRIDE = "select.geckocontrol_weather_override_test";
static const char *const E_DAY_MANUAL = "switch.geckocontrol_daylight_manual";
static const char *const E_DAY_LEVEL = "number.geckocontrol_daylight_manual_level";

// Material Design Icons 7.4.47 codepoints (see icon fonts in gecko-console.yaml)
#define ICON_THERMOMETER "\U000F050F"
#define ICON_WATER_PERCENT "\U000F058E"
#define ICON_WEATHER_RAINY "\U000F0597"
#define ICON_CUP_WATER "\U000F01AB"
#define ICON_WEATHER_WINDY "\U000F059D"
#define ICON_SPROUT "\U000F0E66"
#define ICON_PALETTE "\U000F03D8"
#define ICON_BELL_RING "\U000F009E"
#define ICON_BELL_OFF "\U000F009B"
#define ICON_ARROW_LEFT "\U000F004D"
#define ICON_COG "\U000F0493"
#define ICON_SUNSET_UP "\U000F059C"
#define ICON_SUNSET_DOWN "\U000F059B"
#define ICON_PARTLY_CLOUDY "\U000F0595"
#define ICON_SUNNY "\U000F0599"
#define ICON_ALERT "\U000F0026"
// weather icons of the start page (icon_big)
#define ICON_W_SUNNY "\U000F0599"
#define ICON_W_NIGHT "\U000F0594"
#define ICON_W_PARTLY "\U000F0595"
#define ICON_W_NIGHT_PARTLY "\U000F0F31"
#define ICON_W_CLOUDY "\U000F0590"
#define ICON_W_FOG "\U000F0591"
#define ICON_W_RAINY "\U000F0597"
#define ICON_W_POURING "\U000F0596"
#define ICON_W_PARTLY_RAINY "\U000F0F33"
#define ICON_W_SNOWY "\U000F0598"
#define ICON_W_STORM "\U000F067E"

// Theme
static const Color BG(8, 12, 16);
static const Color HEADER_BG(50, 0, 127);
static const Color TILE_BG(30, 36, 44);
static const Color FOCUS(255, 190, 40);
static const Color EDIT(60, 200, 255);
static const Color TEXT(235, 238, 240);
static const Color TEXT_DIM(150, 160, 170);
static const Color DISABLED(70, 76, 84);
static const Color NO_DATA(64, 64, 64);
static const Color ON_GREEN(60, 190, 90);
static const Color WARN(255, 70, 50);
static const Color ARMED(230, 120, 20);
// Graph trace colours. Value labels on the pages use them as the legend.
static const Color T_NOW(255, 100, 100);    // color_now
static const Color T_WEATHER(255, 230, 60);  // color_weather
static const Color T_TARGET(80, 220, 100);   // color_target
static const Color T_LEVEL(60, 200, 230);    // color_level (reservoir, wind speed)
static const Color T_CEILING(70, 120, 255);  // color_ceiling
static const Color T_GROUND(230, 90, 200);   // color_ground
static const Color T_DAYLIGHT(255, 150, 40);   // daylight lamp level (Thermo graph, own 0-100% scale)
static const Color T_HEAT_LAMP(255, 60, 120);  // heat lamp: graph background tint by its duty
static const Color T_GECKO(170, 110, 255);     // gecko weight, a light shade of the logo's purple
static const Color GRID(60, 66, 74);

// ------------------------------------------------------------------ history
// Every graphed value, sampled every 2 s by sample_history() (gecko-console.yaml) into all time
// ranges at once (gecko_history.h). Heater series are levels in % (0 while off).
enum Series : uint8_t {
  S_TEMP_NOW,
  S_TEMP_TARGET,
  S_TEMP_GROUND,
  S_DAYLIGHT,
  S_HEAT_LAMP,
  S_HUM_NOW,
  S_HUM_TARGET,
  S_HUM_CEILING,
  S_HUM_GROUND,
  S_RESERVOIR,
  S_WIND,
  S_WIND_WEATHER,
  S_WEIGHT,  // gecko weight, only while it sits on the scale (gaps otherwise)
  S_COUNT,
};
inline History<S_COUNT> history;

// One line of a graph: a fixed colour, or `color_of` colours every point by its value.
struct Trace {
  Series series;
  Color color;
  uint8_t thick;
  Color (*color_of)(float){nullptr};
};
static const Trace THERMO_TRACES[] = {
    {S_TEMP_GROUND, T_GROUND, 1}, {S_TEMP_TARGET, T_TARGET, 1}, {S_TEMP_NOW, T_NOW, 2}};
static const Trace HYGRO_TRACES[] = {{S_RESERVOIR, T_LEVEL, 1},
                                     {S_HUM_GROUND, T_GROUND, 1},
                                     {S_HUM_CEILING, T_CEILING, 1},
                                     {S_HUM_TARGET, T_WEATHER, 1},
                                     {S_HUM_NOW, T_NOW, 2}};
static const Trace WIND_TRACES[] = {{S_WIND_WEATHER, T_WEATHER, 1}, {S_WIND, T_LEVEL, 2}};
static const Trace GECKO_TRACES[] = {{S_WEIGHT, T_GECKO, 2}};

struct Box {
  int x{0}, y{0}, w{0}, h{0};
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  Box intersect(const Box &o) const {
    const int x0 = std::max(x, o.x), y0 = std::max(y, o.y);
    const int x1 = std::min(x + w, o.x + o.w), y1 = std::min(y + h, o.y + o.h);
    return Box{x0, y0, x1 - x0, y1 - y0};
  }
  bool intersects(const Box &o) const { return !intersect(o).empty(); }
};

enum class Page : uint8_t { OVERVIEW, MENU, THERMO, HYGRO, WIND, GROW, SCENIC, GECKO, TESTS, SETTINGS };

// Menu grid, row-major (4x2): climate and sunlight (Page::GROW) in the top row, scenic light, gecko, light
// tests and settings below. Swiping on a subpage follows this order, skipping Settings.
static const Page MENU_PAGES[] = {Page::THERMO, Page::HYGRO, Page::WIND,  Page::GROW,
                                  Page::SCENIC, Page::GECKO, Page::TESTS, Page::SETTINGS};
static constexpr int MENU_COUNT = sizeof(MENU_PAGES) / sizeof(MENU_PAGES[0]);
static constexpr int MENU_COLS = 4;

// Which value changed (sensor callbacks -> invalidate the boxes showing it on the current page)
enum Field : uint8_t {
  F_CONNECTION,
  F_CLOCK,
  F_WIFI,
  F_TEMP,
  F_HUM,
  F_RESERVOIR,
  F_RAIN_AUTO,
  F_GROW,
  F_SCENIC,
  F_LIGHT_AUTO,
  F_WIND,
  F_WIND_AUTO,
  F_HEATER,
  F_WEATHER_SET,
  F_SCALE,
  F_BACKLIGHT,
  F_TEST_STATE,  // scenic light effect or weather override (Light tests page)
  F_DAY_MANUAL,  // daylight lamp held by hand or by the thermostat
};

// Controls on the subpages
enum Ctl : uint8_t {
  C_RAIN_AUTO,
  C_LET_IT_RAIN,
  C_REFILLED,
  C_WIND_AUTO,
  C_WIND_SPEED,
  C_GROW_AUTO,
  C_GROW_LEVEL,
  C_SCENIC_AUTO,
  C_SCENIC_WHEEL,
  C_SCENIC_BRI,
  C_WEATHER_SET,
  C_THERMO_SET,
  C_GRAPH,  // the history graph: push/tap cycles its time range
  C_BACKLIGHT,
  C_SUNRISE_TEST,
  C_SUNSET_TEST,
  C_WEATHER_TEST,
  C_DAY_MANUAL,
  C_DAY_LEVEL,
  C_COUNT,
  C_NONE = 0xFF,
};
enum class Kind : uint8_t { TOGGLE, BUTTON, CONFIRM, SLIDER, WHEEL };

struct Control {
  Ctl id;
  Kind kind;
  Box box;
};

// ------------------------------------------------------------------ layout
// One layout for all subpages with a history graph (Thermo, Hygro & Rain, Wind): values on the
// left, the graph on the right (every graph is 158x94, see gecko-console.yaml), controls below.
// Sunlight and Scenic light have their own layouts.
static constexpr Box VALUES_LEFT{0, 30, 152, 104};
static constexpr int GRAPH_X = 156, GRAPH_Y = 32;
static constexpr Box GRAPH_BOX{GRAPH_X, GRAPH_Y, HIST_LEN, 94};
static constexpr Box TOGGLE_ROW{8, 134, W - 16, 32};
static constexpr int SLIDER_LABEL_Y = 180;
static constexpr Box SLIDER_BOX{8, 190, W - 16, 44};
static constexpr Box RESERVOIR_ROW{8, 168, W - 16, 26};

// Scenic light: the wheel sits at the left edge and everything that changes with the cursor stays
// out of its rows (hue/sat values and the brightness slider, filled with the colour, in the bottom
// row), so a cursor move only redraws a small strip around the cursor plus the bottom row. The wheel itself is a pre-rendered RGB565 image in flash
// (tools/gen_colorwheel.py -> colorwheel.png, WHEEL_R must match).
static constexpr Box WHEEL_BOX{4, 30, 176, 172};
static constexpr int WHEEL_CX = 4 + 88, WHEEL_CY = 30 + 86, WHEEL_R = 80;
static constexpr Box SCENIC_VALUES{186, 206, 130, 32};  // hue / sat numbers, right of the slider
static constexpr int SCENIC_HUE_X = 218, SCENIC_SAT_X = 284;  // column centres

static const Control THERMO_CONTROLS[] = {
    {C_WEATHER_SET, Kind::TOGGLE, TOGGLE_ROW},
    {C_THERMO_SET, Kind::SLIDER, SLIDER_BOX},
    {C_GRAPH, Kind::BUTTON, GRAPH_BOX},
};
static const Control HYGRO_CONTROLS[] = {
    {C_RAIN_AUTO, Kind::TOGGLE, TOGGLE_ROW},
    {C_LET_IT_RAIN, Kind::BUTTON, {8, 200, 148, 36}},
    {C_REFILLED, Kind::CONFIRM, {164, 200, 148, 36}},
    {C_GRAPH, Kind::BUTTON, GRAPH_BOX},
};
static const Control WIND_CONTROLS[] = {
    {C_WIND_AUTO, Kind::TOGGLE, TOGGLE_ROW},
    {C_WIND_SPEED, Kind::SLIDER, SLIDER_BOX},
    {C_GRAPH, Kind::BUTTON, GRAPH_BOX},
};
// Sunlight (Page::GROW): the growlight -- what the terrarium looks like, run by the light
// automation -- and the daylight lamp -- the daytime heater, run by the thermostat unless held by
// hand. Each a toggle, a label line and a slider.
static constexpr Box GROW_INFO{0, 64, W, 18};
static constexpr Box DAY_INFO{0, 168, W, 18};
static const Control GROW_CONTROLS[] = {
    {C_GROW_AUTO, Kind::TOGGLE, {8, 30, W - 16, 32}},
    {C_GROW_LEVEL, Kind::SLIDER, {8, 82, W - 16, 44}},
    {C_DAY_MANUAL, Kind::TOGGLE, {8, 134, W - 16, 32}},
    {C_DAY_LEVEL, Kind::SLIDER, {8, 186, W - 16, 44}},
};
static const Control SCENIC_CONTROLS[] = {
    {C_SCENIC_AUTO, Kind::TOGGLE, {186, 32, 130, 34}},
    {C_SCENIC_WHEEL, Kind::WHEEL, WHEEL_BOX},
    {C_SCENIC_BRI, Kind::SLIDER, {4, 206, 176, 32}},
};
// Gecko: the scale sits under its nest. How long it has been in / out, weight and graph like the
// other graph pages, the latest completed in / out periods below. In / out is decided and logged by
// the controller (gecko_nest.h there), which keeps it across reboots.
static constexpr Box GECKO_LOG{0, 136, W, H - 136};
static constexpr int GECKO_LOG_ROWS = 5;
static constexpr float GECKO_MIN_G = 5.f;  // graph only readings from here on (as gecko_nest.h)
static const Control GECKO_CONTROLS[] = {
    {C_GRAPH, Kind::BUTTON, GRAPH_BOX},
};
// Start page: the live sky behind two glass panels -- weather, temperature and humidity on top,
// the next sunrise / sunset and the nest below. Redrawn as a whole every frame.
static const gecko_scene::Glass START_GLASS[] = {{6, 30, 250, 94}, {6, 168, 308, 66}};
static constexpr Box ST_WEATHER{6, 30, 76, 94}, ST_TEMP{82, 30, 174, 47}, ST_HUM{82, 77, 174, 47};
static constexpr Box ST_SUN{6, 168, 308, 33}, ST_NEST{6, 201, 308, 33};
static constexpr uint32_t FRAME_MIN_MS = 100;  // at most 10 frames per second ...
// ... and at least twice the time a frame takes, so the animation leaves half the CPU to the rest

// Test sunrise / sunset on the start page, alongside the terrarium (see sweep_check()): over the
// sweep the sun rises from below the horizon (or sets), the clouds glowing in sunrise colours.
static constexpr uint32_t SHOW_STEP_MS = 10000;        // TEST_SWEEP_STEP_MS in geckolights.h
static constexpr uint32_t SHOW_MS = 6 * SHOW_STEP_MS;  // the sweep's six colour steps
static constexpr uint32_t SHOW_IN_MS = 2000, SHOW_OUT_MS = 6000;  // from / back to the real sky
static constexpr float SHOW_LOW = -10.f, SHOW_HIGH = 18.f;      // the sun's way, degrees
static constexpr float SHOW_COVER = 0.5f;  // at least partly cloudy: clouds to colour
static constexpr time_t SHOW_PRESS_S = 30;  // a sweep this soon after its test button press is the test

// Climate alarms of the controller (geckoalarm). A raised one takes the start page's sun row,
// pulsing red; two at once (too hot / cold and too dry) take turns. Tapping it opens its page.
enum class Alarm : uint8_t { OVERTEMPERATURE, UNDERTEMPERATURE, LOW_HUMIDITY, COUNT, NONE = COUNT };
static constexpr int ALARM_COUNT = static_cast<int>(Alarm::COUNT);
static constexpr uint32_t ALARM_TURN_MS = 3000;

// Light tests: test sunrise / sunset and the weather override (cycles through its options) for
// trying out the light scenes; below them the running scenic effect and the weather in force.
static constexpr Box TESTS_INFO{0, 164, W, H - 164};
static const Control TESTS_CONTROLS[] = {
    {C_SUNRISE_TEST, Kind::BUTTON, {8, 34, 148, 60}},
    {C_SUNSET_TEST, Kind::BUTTON, {164, 34, 148, 60}},
    {C_WEATHER_TEST, Kind::BUTTON, {8, 100, W - 16, 60}},
};

// Settings: network info lines; display brightness slider in the usual slider row.
static constexpr Box SETTINGS_INFO{0, 30, W, 74};
static const Control SETTINGS_CONTROLS[] = {
    {C_BACKLIGHT, Kind::SLIDER, SLIDER_BOX},
};

inline float sval(sensor::Sensor *s) { return s->has_state() ? s->state : NAN; }

// Direct access to the mipi_spi stripe buffer for the start page's sky, which writes every pixel:
// draw_pixel_at() costs a virtual call, a clipping test and four watermark updates per pixel. The
// members are protected; a derived class may name them, and member pointers taken there work on the
// display object itself (standard C++, no patch to ESPHome). Requires hardware rotation, so that
// the buffer holds the stripe in screen orientation, 16 bit pixels and no column rounding.
template<class D> struct StripeAccess : D {
  static uint16_t *buffer(D &d) { return d.*(&StripeAccess::buffer_); }
  static int start(D &d) { return d.*(&StripeAccess::start_line_); }
  static int end(D &d) { return d.*(&StripeAccess::end_line_); }
  static uint16_t pixel(uint8_t r, uint8_t g, uint8_t b) { return StripeAccess::convert_color(Color(r, g, b)); }
  // the whole stripe is to be sent
  static void mark(D &d) {
    d.*(&StripeAccess::x_low_) = 0;
    d.*(&StripeAccess::x_high_) = W - 1;
    d.*(&StripeAccess::y_low_) = start(d);
    d.*(&StripeAccess::y_high_) = end(d) - 1;
  }
  static_assert(D::round_buffer(W) == W, "stripe rows must be exactly the screen width");
};

// Parses HA's hs_color attribute, e.g. "(210.0, 80.0)" or "[210.0, 80.0]".
inline bool parse_hs(const std::string &txt, float &h, float &s) {
  const char *p = txt.c_str();
  float v[2];
  for (int i = 0; i < 2; i++) {
    while (*p && !(std::isdigit((unsigned char) *p) || *p == '-' || *p == '.'))
      p++;
    if (!*p)
      return false;
    char *end;
    v[i] = strtof(p, &end);
    if (end == p)
      return false;
    p = end;
  }
  h = v[0];
  s = v[1] / 100.f;
  return true;
}

inline Color hsv_color(float hue, float sat, float val) {
  float r, g, b;
  hsv_to_rgb((int) std::lround(hue) % 360, sat, val, r, g, b);
  return Color(r * 255, g * 255, b * 255);
}

class UI {
 public:
  // ---------------------------------------------------------------- redraw management
  void invalidate_all() {
    this->full_dirty_ = true;
    this->dirty_.clear();
  }
  void invalidate(const Box &b) {
    if (!this->full_dirty_ && !b.empty())
      this->dirty_.push_back(b);
  }

  // Called from a 20 ms interval: sends throttled values, performs pending redraws.
  void flush(Display *d) {
    this->send_due_();
    const uint32_t now = millis();
    const bool animate = this->page_ == Page::OVERVIEW && id(api_connected).state;
    if (animate && now - this->frame_start_ >= this->frame_gap_)
      this->full_dirty_ = true;
    if (this->full_dirty_) {
      this->full_dirty_ = false;
      this->dirty_.clear();
      this->partial_ = false;
      this->pass_ = 0;
      if (animate)
        this->start_step_(now);
      const uint32_t t0 = millis();
      d->update();
      if (animate)
        this->frame_done_(t0, millis() - t0);
      return;
    }
    if (animate) {
      this->dirty_.clear();  // the next frame redraws everything anyway
      return;
    }
    if (this->dirty_.empty())
      return;
    // Anchor every box at x = 0 (keep the right edge, see the stride quirk in the header comment),
    // then merge boxes whose rows overlap, e.g. the old and new colour wheel cursor.
    std::vector<Box> boxes;
    for (auto &d : this->dirty_) {
      Box b = d.intersect(Box{0, 0, W, H});
      if (b.empty())
        continue;
      b.w += b.x;
      b.x = 0;
      boxes.push_back(b);
    }
    this->dirty_.clear();
    std::sort(boxes.begin(), boxes.end(), [](const Box &a, const Box &b) { return a.y < b.y; });
    std::vector<Box> merged;
    for (auto &b : boxes) {
      if (!merged.empty() && b.y <= merged.back().y + merged.back().h) {
        Box &m = merged.back();
        const int bottom = std::max(m.y + m.h, b.y + b.h);
        m.w = std::max(m.w, b.w);
        m.h = bottom - m.y;
      } else {
        merged.push_back(b);
      }
    }
    for (auto &b : merged) {
      this->region_ = b;
      this->partial_ = true;
      this->pass_ = 0;
      d->update();
    }
    this->partial_ = false;
  }

  // Display lambda entry point; runs once per buffer stripe.
  void draw(Display &it) {
    const int sh = H / STRIPES;
    const int p = std::min(this->pass_++, STRIPES - 1);
    const Box stripe{0, p * sh, W, p == STRIPES - 1 ? H - p * sh : sh};

    if (!this->partial_) {
      this->area_ = stripe;
      if (this->page_ != Page::OVERVIEW || !id(api_connected).state)
        it.fill(BG);  // fast path: fills the stripe buffer directly (no clipping active)
      this->draw_page_(it);  // (the start page's sky covers every pixel itself)
      return;
    }

    this->area_ = this->region_.intersect(stripe);
    if (this->area_.empty()) {
      if (this->region_.y < stripe.y + stripe.h)
        return;  // region done; the driver stops at this empty stripe
      // Region lies in a later stripe: redraw one pixel of this stripe so the driver does not
      // abort the update here (see header comment).
      this->area_ = Box{this->region_.x, stripe.y + stripe.h - 1, 1, 1};
    }
    it.start_clipping(display::Rect(this->area_.x, this->area_.y, this->area_.w, this->area_.h));
    this->fill_(it, this->area_, BG);
    this->draw_page_(it);
    it.end_clipping();
  }

  // ---------------------------------------------------------------- data changes
  void changed(Field f) {
    if (f == F_CONNECTION || !id(api_connected).state) {
      this->invalidate_all();
      return;
    }
    if (this->page_ == Page::OVERVIEW)
      return;  // redrawn every frame
    if (f == F_WIFI)
      this->invalidate(Box{W - 34, 0, 34, HEADER_H});

    switch (this->page_) {
      case Page::OVERVIEW:
        break;
      case Page::MENU: {
        Page p = Page::MENU;
        switch (f) {
          case F_TEMP: p = Page::THERMO; break;
          case F_HUM: p = Page::HYGRO; break;
          case F_WIND: p = Page::WIND; break;
          case F_GROW: p = Page::GROW; break;
          case F_SCENIC: p = Page::SCENIC; break;
          case F_SCALE: p = Page::GECKO; break;
          case F_TEST_STATE: p = Page::TESTS; break;
          default: break;
        }
        const int tile = menu_index(p);
        if (tile >= 0)
          this->invalidate(menu_tile_box(tile));
        break;
      }
      case Page::THERMO:
        if (f == F_TEMP) {
          this->invalidate(VALUES_LEFT);
          if (!this->held_(C_THERMO_SET))
            this->invalidate_ctl_(C_THERMO_SET);
        } else if (f == F_HEATER) {
          this->invalidate(VALUES_LEFT);
        } else if (f == F_WEATHER_SET) {
          this->invalidate_all();  // slider enabled state changes
        }
        break;
      case Page::HYGRO:
        if (f == F_HUM)
          this->invalidate(VALUES_LEFT);
        else if (f == F_RESERVOIR)
          this->invalidate(RESERVOIR_ROW);
        else if (f == F_RAIN_AUTO)
          this->invalidate_ctl_(C_RAIN_AUTO);
        break;
      case Page::WIND:
        if (f == F_WIND) {
          this->invalidate(VALUES_LEFT);
          if (!this->held_(C_WIND_SPEED))
            this->invalidate_ctl_(C_WIND_SPEED);
        } else if (f == F_WIND_AUTO) {
          this->invalidate_all();  // slider enabled state changes
        }
        break;
      case Page::GROW:
        if (f == F_GROW) {
          this->invalidate(GROW_INFO);
          if (!this->held_(C_GROW_LEVEL))
            this->invalidate_ctl_(C_GROW_LEVEL);
        } else if (f == F_HEATER) {  // the daylight lamp's level
          if (!this->held_(C_DAY_LEVEL))
            this->invalidate_ctl_(C_DAY_LEVEL);
        } else if (f == F_LIGHT_AUTO || f == F_DAY_MANUAL) {
          this->invalidate_all();  // slider enabled state changes
        }
        break;
      case Page::SCENIC:
        if (f == F_SCENIC) {
          this->scenic_refresh_();
          if (!this->held_(C_SCENIC_BRI))
            this->invalidate_ctl_(C_SCENIC_BRI);
        } else if (f == F_LIGHT_AUTO) {
          this->invalidate_all();
        }
        break;
      case Page::GECKO:
        if (f == F_SCALE || f == F_CLOCK) {  // the clock ages the in / out duration
          this->invalidate(VALUES_LEFT);
          this->invalidate(GECKO_LOG);
        }
        break;
      case Page::TESTS:
        if (f == F_TEST_STATE) {
          for (Ctl c : {C_SUNRISE_TEST, C_SUNSET_TEST, C_WEATHER_TEST})
            this->invalidate_ctl_(c);
          this->invalidate(TESTS_INFO);
        }
        break;
      case Page::SETTINGS:
        if (f == F_WIFI)
          this->invalidate(SETTINGS_INFO);
        else if (f == F_BACKLIGHT && !this->held_(C_BACKLIGHT))
          this->invalidate_ctl_(C_BACKLIGHT);
        break;
    }
  }

  // The scenic light's effect or the last press of a test sweep button changed. A test sweep is
  // played on the start page; the real ones need no help, the sky follows the real sun anyway. Both
  // run the same "Sunrise Color" / "Sunset Color" effect: a sweep starting within SHOW_PRESS_S of
  // its button's press (Home Assistant keeps the time as the button's state) is the test, whether
  // pressed here or in Home Assistant. Either update may arrive first, so both call this.
  void sweep_check() {
    const std::string &fx = id(scenic_light_effect).state;
    const bool rise = fx == "Sunrise Color";
    if (!rise && fx != "Sunset Color") {
      this->show_stop_();
      return;
    }
    if (this->show_ && !this->show_ending_ && this->show_rise_ == rise)
      return;  // already playing
    const time_t now = epoch_now();
    const time_t pressed = parse_utc(id(rise ? sunrise_test_pressed : sunset_test_pressed).state);
    if (now == 0 || pressed == 0 || now - pressed > SHOW_PRESS_S || pressed - now > SHOW_PRESS_S)
      return;
    ESP_LOGI(TAG, "test %s: shown on the start page", rise ? "sunrise" : "sunset");
    this->show_ = true;
    this->show_ending_ = false;
    this->show_rise_ = rise;
    this->show_start_ = millis();
    this->go_(Page::OVERVIEW);
  }

  // "Gecko nest Log" from the controller: "<in>,<since>;<in>,<start>,<seconds>,<weight x10>;..."
  // with UTC epochs, newest period first.
  void gecko_log(const std::string &txt) {
    unsigned in = 0, since = 0;
    int pos = 0;
    this->gecko_known_ = sscanf(txt.c_str(), "%u,%u%n", &in, &since, &pos) == 2 && since > 0;
    this->gecko_in_ = in != 0;
    this->gecko_since_ = since;
    this->gecko_log_n_ = 0;
    this->last_stay_weight_ = NAN;
    const char *p = this->gecko_known_ ? txt.c_str() + pos : "";
    while (*p == ';' && this->gecko_log_n_ < GECKO_LOG_ROWS) {
      unsigned i, start, sec, w10;
      int k = 0;
      if (sscanf(p + 1, "%u,%u,%u,%u%n", &i, &start, &sec, &w10, &k) != 4)
        break;
      GeckoPeriod &e = this->gecko_log_[this->gecko_log_n_++];
      e.in = i != 0;
      e.start = start;
      e.seconds = sec;
      e.weight = e.in && w10 > 0 ? w10 / 10.f : NAN;
      if (std::isnan(this->last_stay_weight_) && !std::isnan(e.weight))
        this->last_stay_weight_ = e.weight;
      p += 1 + k;
    }
    this->changed(F_SCALE);
  }

  // ---------------------------------------------------------------- input
  void turn(int dir) {
    this->activity_();
    switch (this->page_) {
      case Page::OVERVIEW:
        this->go_(Page::MENU);
        return;
      case Page::MENU: {
        const int old = this->menu_focus_;
        this->menu_focus_ = (this->menu_focus_ + dir + MENU_COUNT) % MENU_COUNT;
        this->invalidate(menu_tile_box(old));
        this->invalidate(menu_tile_box(this->menu_focus_));
        return;
      }
      default:
        break;
    }
    int n;
    const Control *cs = controls(this->page_, n);
    if (n == 0) {
      this->swipe_(dir);  // nothing to focus (Thermo): turning flips pages
      return;
    }
    if (this->editing_ && this->focus_ >= 0) {
      this->edit_turn_(cs[this->focus_].id, dir);
      return;
    }
    // move focus to the next enabled control; -1 is the header's back arrow
    int f = this->focus_;
    for (int i = 0; i <= n; i++) {
      f += dir;
      if (f >= n)
        f = -1;
      else if (f < -1)
        f = n - 1;
      if (f == -1 || this->enabled_(cs[f].id))
        break;
    }
    this->set_focus_(f);
  }

  void push() {
    this->activity_();
    switch (this->page_) {
      case Page::OVERVIEW:
        this->go_(Page::MENU);
        return;
      case Page::MENU:
        this->go_(MENU_PAGES[this->menu_focus_]);
        return;
      default:
        break;
    }
    int n;
    const Control *cs = controls(this->page_, n);
    if (n == 0 || this->focus_ < 0) {
      this->back();
      return;
    }
    if (!this->enabled_(cs[this->focus_].id))
      return;
    const Control &c = cs[this->focus_];
    switch (c.kind) {
      case Kind::SLIDER:
        this->editing_ = !this->editing_;
        if (!this->editing_)
          this->send_now_(c.id);
        this->invalidate_ctl_(c.id);
        break;
      case Kind::WHEEL:
        if (!this->editing_) {
          this->editing_ = true;
          this->wheel_part_ = 0;
        } else if (this->wheel_part_ == 0) {
          this->wheel_part_ = 1;
          this->invalidate(cursor_box(this->shown_h_, this->shown_s_));  // circle -> square
        } else {
          this->editing_ = false;
          this->send_now_(c.id);
          this->invalidate_ctl_(c.id);
        }
        this->invalidate(SCENIC_VALUES);
        this->invalidate(this->focus_frame_box_(c));
        break;
      default:
        this->activate_(c);
        break;
    }
  }

  void back() {
    this->activity_();
    if (this->editing_) {
      int n;
      const Control *cs = controls(this->page_, n);
      this->editing_ = false;
      if (n > 0 && this->focus_ >= 0) {
        this->send_now_(cs[this->focus_].id);
        this->invalidate_ctl_(cs[this->focus_].id);
        if (cs[this->focus_].kind == Kind::WHEEL)
          this->invalidate(SCENIC_VALUES);
      }
      return;
    }
    switch (this->page_) {
      case Page::OVERVIEW:
        break;
      case Page::MENU:
        this->go_(Page::OVERVIEW);
        break;
      default:
        this->go_(Page::MENU);
        break;
    }
  }

  void touch_down(int x, int y) {
    this->activity_();
    this->t_x0_ = this->t_x_ = x;
    this->t_y0_ = this->t_y_ = y;
    this->t_down_ = true;
    this->drag_ = C_NONE;
    // Pressing an enabled slider or the wheel starts a drag right away.
    int n;
    const Control *cs = controls(this->page_, n);
    for (int i = 0; i < n; i++) {
      const Control &c = cs[i];
      if ((c.kind == Kind::SLIDER || c.kind == Kind::WHEEL) && c.box.contains(x, y) && this->enabled_(c.id)) {
        if (c.kind == Kind::WHEEL && !this->wheel_hit_(x, y))
          continue;
        if (this->editing_ && i != this->focus_)
          this->editing_ = false;
        this->set_focus_(i);
        this->drag_ = c.id;
        this->drag_to_(c, x, y);
        return;
      }
    }
  }
  void touch_move(int x, int y) {
    if (!this->t_down_)
      return;
    this->t_x_ = x;
    this->t_y_ = y;
    if (this->drag_ != C_NONE) {
      this->activity_();
      int n;
      const Control *cs = controls(this->page_, n);
      for (int i = 0; i < n; i++)
        if (cs[i].id == this->drag_)
          this->drag_to_(cs[i], x, y);
    }
  }
  void touch_up() {
    if (!this->t_down_)
      return;
    this->t_down_ = false;
    this->activity_();
    if (this->drag_ != C_NONE) {
      this->send_now_(this->drag_);
      this->drag_ = C_NONE;
      return;
    }
    const int dx = this->t_x_ - this->t_x0_, dy = this->t_y_ - this->t_y0_;
    if (std::abs(dx) >= SWIPE_MIN_DX && std::abs(dx) > 2 * std::abs(dy)) {
      this->swipe_(dx < 0 ? +1 : -1);
      return;
    }
    if (std::abs(dx) < 20 && std::abs(dy) < 20)
      this->tap_(this->t_x0_, this->t_y0_);
  }

  // Called every second: idle timeout, expired holds/confirmations, graph refresh.
  void tick() {
    const uint32_t now = millis();
    if (this->page_ != Page::OVERVIEW && now - this->last_activity_ > IDLE_TIMEOUT_MS) {
      this->editing_ = false;
      this->go_(Page::OVERVIEW);
      return;
    }
    for (int c = 0; c < C_COUNT; c++) {
      if (this->hold_until_[c] != 0 && (int32_t) (now - this->hold_until_[c]) >= 0) {
        this->hold_until_[c] = 0;
        if (c == C_SCENIC_WHEEL)
          this->scenic_refresh_();
        else
          this->invalidate_ctl_((Ctl) c);
      }
    }
    if (this->armed_until_ != 0 && (int32_t) (now - this->armed_until_) >= 0) {
      this->armed_until_ = 0;
      this->invalidate_ctl_(this->armed_ctl_);
    }
    // Redraw the graph only when its range has completed a pixel (every 5.7 s at 15 min, every
    // 18 min at 48 h).
    const uint32_t pushes = history.pushes(this->range_);
    if (pushes != this->graph_pushes_) {
      this->graph_pushes_ = pushes;
      this->invalidate_graph_();
    }
  }

 protected:
  // ---------------------------------------------------------------- layout helpers
  static constexpr Box BACK_BOX = {0, 0, 150, HEADER_H};     // tap target
static constexpr Box BACK_FOCUS = {2, 1, 30, HEADER_H - 2};  // encoder focus frame

  static Box menu_tile_box(int i) {
    constexpr int rows = (MENU_COUNT + MENU_COLS - 1) / MENU_COLS;
    const int gap = 6, tw = (W - (MENU_COLS + 1) * gap) / MENU_COLS, th = (H - HEADER_H - (rows + 1) * gap) / rows;
    return Box{gap + (i % MENU_COLS) * (tw + gap), HEADER_H + gap + (i / MENU_COLS) * (th + gap), tw, th};
  }
  static int menu_index(Page p) {
    for (int i = 0; i < MENU_COUNT; i++)
      if (MENU_PAGES[i] == p)
        return i;
    return -1;
  }

  static const Control *controls(Page p, int &n) {
    switch (p) {
      case Page::THERMO: n = 3; return THERMO_CONTROLS;
      case Page::HYGRO: n = 4; return HYGRO_CONTROLS;
      case Page::WIND: n = 3; return WIND_CONTROLS;
      case Page::GROW: n = 4; return GROW_CONTROLS;
      case Page::SCENIC: n = 3; return SCENIC_CONTROLS;
      case Page::GECKO: n = 1; return GECKO_CONTROLS;
      case Page::TESTS: n = 3; return TESTS_CONTROLS;
      case Page::SETTINGS: n = 1; return SETTINGS_CONTROLS;
      default: n = 0; return nullptr;
    }
  }
  const Control *find_ctl_(Ctl id) const {
    int n;
    const Control *cs = controls(this->page_, n);
    for (int i = 0; i < n; i++)
      if (cs[i].id == id)
        return &cs[i];
    return nullptr;
  }
  void invalidate_ctl_(Ctl id) {
    const Control *c = this->find_ctl_(id);
    if (c != nullptr)
      this->invalidate(c->kind == Kind::WHEEL ? WHEEL_BOX : c->box);
  }
  // The focus frame is drawn on the control's box edge; for the wheel just around the circle.
  Box focus_frame_box_(const Control &c) const { return c.box; }

  // ---------------------------------------------------------------- navigation
  void go_(Page p) {
    if (p == Page::MENU && menu_index(this->page_) >= 0)
      this->menu_focus_ = menu_index(this->page_);  // back from a subpage: focus its tile
    if (p != this->page_) {
      this->editing_ = false;
      this->armed_until_ = 0;
      this->drag_ = C_NONE;
      // focus the first enabled control (the back arrow if there is none)
      this->focus_ = -1;
      int n;
      const Control *cs = controls(p, n);
      for (int i = 0; i < n; i++) {
        if (this->enabled_after_(p, cs[i].id)) {
          this->focus_ = i;
          break;
        }
      }
    }
    this->page_ = p;
    if (p == Page::SCENIC)
      this->scenic_current_(this->shown_h_, this->shown_s_);
    this->invalidate_all();
  }

  void swipe_(int dir) {
    int i = menu_index(this->page_);
    if (i < 0 || this->page_ == Page::SETTINGS)
      return;
    do
      i = (i + dir + MENU_COUNT) % MENU_COUNT;
    while (MENU_PAGES[i] == Page::SETTINGS);
    this->go_(MENU_PAGES[i]);
  }

  void tap_(int x, int y) {
    if (this->page_ != Page::OVERVIEW && BACK_BOX.contains(x, y)) {
      this->back();
      return;
    }
    switch (this->page_) {
      case Page::OVERVIEW:
        if (ST_TEMP.contains(x, y))
          this->go_(Page::THERMO);
        else if (ST_HUM.contains(x, y))
          this->go_(Page::HYGRO);
        else if (ST_SUN.contains(x, y))
          this->go_(this->alarm_shown_ != Alarm::NONE ? alarm_page(this->alarm_shown_) : Page::SCENIC);
        else if (ST_NEST.contains(x, y))
          this->go_(Page::GECKO);
        else
          this->go_(Page::MENU);
        return;
      case Page::MENU:
        for (int i = 0; i < MENU_COUNT; i++) {
          if (menu_tile_box(i).contains(x, y)) {
            this->menu_focus_ = i;
            this->go_(MENU_PAGES[i]);
            return;
          }
        }
        return;
      default:
        break;
    }
    int n;
    const Control *cs = controls(this->page_, n);
    for (int i = 0; i < n; i++) {
      const Control &c = cs[i];
      if (!c.box.contains(x, y) || !this->enabled_(c.id))
        continue;
      if (c.kind == Kind::TOGGLE || c.kind == Kind::BUTTON || c.kind == Kind::CONFIRM) {
        this->editing_ = false;
        this->set_focus_(i);
        this->activate_(c);
      }
      return;
    }
  }

  void set_focus_(int f) {
    if (f == this->focus_)
      return;
    this->invalidate(this->focus_box_(this->focus_));
    this->focus_ = f;
    this->invalidate(this->focus_box_(f));
  }
  Box focus_box_(int f) const {
    int n;
    const Control *cs = controls(this->page_, n);
    if (f < 0 || f >= n)
      return BACK_FOCUS;
    return this->focus_frame_box_(cs[f]);
  }

  void activity_() { this->last_activity_ = millis(); }

  // ---------------------------------------------------------------- control state
  bool enabled_(Ctl c) const { return this->enabled_after_(this->page_, c); }
  bool enabled_after_(Page, Ctl c) const {
    switch (c) {
      case C_WIND_SPEED: return !id(multifan_auto).state;
      case C_THERMO_SET: return !id(weather_setpoint).state;
      case C_GROW_LEVEL: case C_SCENIC_WHEEL: case C_SCENIC_BRI: return !id(light_automation).state;
      case C_DAY_LEVEL: return id(daylight_manual).state;
      default: return true;
    }
  }

  bool held_(Ctl c) const { return this->hold_until_[c] != 0; }
  void hold_(Ctl c) { this->hold_until_[c] = millis() + HOLD_MS; }

  // Value range of a slider: touch snaps to `step`, the encoder moves by `enc_step`.
  // While disabled (its automation owns the value) a slider still shows the value, greyed, with
  // `fmt_auto`.
  struct SliderSpec {
    float min, max, step, enc_step;
    const char *fmt, *fmt_auto;
  };
  static SliderSpec slider_spec(Ctl c) {
    switch (c) {
      case C_THERMO_SET:  // thermostat visual range
        return {12.f, 36.f, 0.5f, 0.5f, "%.1f°C", "%.1f°C (weather)"};
      case C_WIND_SPEED:  // number step 5
        return {0.f, 100.f, 5.f, 5.f, "%.0f%%", "%.0f%% (auto)"};
      case C_BACKLIGHT:  // never fully dark: the screen must stay readable
        return {5.f, 100.f, 5.f, 5.f, "%.0f%%", "%.0f%%"};
      case C_DAY_LEVEL:
        return {0.f, 100.f, 1.f, 5.f, "%.0f%%", "%.0f%% (thermostat)"};
      default:
        return {0.f, 100.f, 1.f, 5.f, "%.0f%%", "%.0f%% (auto)"};
    }
  }

  // Current value of a slider (in its unit), the held local value while one is active.
  float slider_value_(Ctl c) const {
    if (this->held_(c))
      return this->local_[c];
    switch (c) {
      case C_THERMO_SET: {
        const float v = sval(&id(thermostat_target_temperature));
        return std::isnan(v) ? slider_spec(c).min : v;
      }
      case C_WIND_SPEED: {
        const float v = sval(&id(fan_manual_speed));
        return std::isnan(v) ? 0.f : v;
      }
      case C_GROW_LEVEL: {
        const float b = sval(&id(grow_light_brightness));
        return id(grow_light_on).state && !std::isnan(b) ? b / 2.55f : 0.f;
      }
      case C_SCENIC_BRI: {
        const float b = sval(&id(scenic_light_brightness));
        return id(scenic_light_on).state && !std::isnan(b) ? b / 2.55f : 0.f;
      }
      case C_DAY_LEVEL: {
        const float b = sval(&id(daylight_brightness));
        return id(daylight_on).state && !std::isnan(b) ? b / 2.55f : 0.f;
      }
      case C_BACKLIGHT: {
        auto &v = id(backlight).remote_values;
        return v.is_on() ? v.get_brightness() * 100.f : 0.f;
      }
      default:
        return 0.f;
    }
  }
  void set_slider_(Ctl c, float v) {
    const SliderSpec sp = slider_spec(c);
    v = clamp(std::round(v / sp.step) * sp.step, sp.min, sp.max);
    if (this->held_(c) && this->local_[c] == v)
      return;
    this->local_[c] = v;
    this->hold_(c);
    this->send_pending_[c] = true;
    this->invalidate_ctl_(c);
    if (c == C_SCENIC_BRI)
      this->invalidate(SCENIC_VALUES);
  }

  // Current scenic hue (deg) / saturation (0-1) as shown.
  void scenic_current_(float &h, float &s) const {
    if (this->held_(C_SCENIC_WHEEL)) {
      h = this->local_h_;
      s = this->local_s_;
      return;
    }
    h = 0.f;
    s = 0.f;
    if (id(scenic_light_hs).has_state())
      parse_hs(id(scenic_light_hs).state, h, s);
  }

  static Box cursor_box(float h, float s) {
    const float a = h * (float) M_PI / 180.f;
    const int px = WHEEL_CX + (int) std::lround(std::cos(a) * s * WHEEL_R);
    const int py = WHEEL_CY + (int) std::lround(std::sin(a) * s * WHEEL_R);
    return Box{px - 10, py - 10, 21, 21};
  }

  // Redraw only what moved: the wheel under the old cursor, the new cursor, the header values
  // and the brightness slider (whose fill shows the colour).
  void scenic_refresh_() {
    if (this->page_ != Page::SCENIC)
      return;
    float h, s;
    this->scenic_current_(h, s);
    if (h != this->shown_h_ || s != this->shown_s_) {
      this->invalidate(cursor_box(this->shown_h_, this->shown_s_));
      this->invalidate(cursor_box(h, s));
      this->shown_h_ = h;
      this->shown_s_ = s;
      this->invalidate_ctl_(C_SCENIC_BRI);
    }
    this->invalidate(SCENIC_VALUES);
  }

  void set_hs_(float h, float s) {
    h = std::fmod(h + 360.f, 360.f);
    s = clamp(s, 0.f, 1.f);
    this->local_h_ = h;
    this->local_s_ = s;
    this->hold_(C_SCENIC_WHEEL);
    this->send_pending_[C_SCENIC_WHEEL] = true;
    this->scenic_refresh_();
  }

  bool editing_sat_() const {
    return this->editing_ && this->wheel_part_ == 1 && this->page_ == Page::SCENIC && this->focus_ >= 0 &&
           SCENIC_CONTROLS[this->focus_].id == C_SCENIC_WHEEL;
  }

  bool wheel_hit_(int x, int y) const {
    const int dx = x - WHEEL_CX, dy = y - WHEEL_CY;
    return dx * dx + dy * dy <= (WHEEL_R + 8) * (WHEEL_R + 8);
  }

  void drag_to_(const Control &c, int x, int y) {
    if (c.kind == Kind::SLIDER) {
      const SliderSpec sp = slider_spec(c.id);
      const float frac = (x - c.box.x - 6) / (float) (c.box.w - 12);
      this->set_slider_(c.id, sp.min + frac * (sp.max - sp.min));
    } else if (c.kind == Kind::WHEEL) {
      const float dx = x - WHEEL_CX, dy = y - WHEEL_CY;
      float h = std::atan2(dy, dx) * 180.f / (float) M_PI;
      if (h < 0)
        h += 360.f;
      this->set_hs_(h, std::sqrt(dx * dx + dy * dy) / WHEEL_R);
    }
  }

  void edit_turn_(Ctl c, int dir) {
    if (c == C_SCENIC_WHEEL) {
      float h, s;
      this->scenic_current_(h, s);
      if (this->wheel_part_ == 0)
        // 48 steps per hue circle. Hue grows clockwise on the wheel, so a clockwise encoder turn
        // raises it and the cursor follows the knob.
        this->set_hs_(h + dir * 7.5f, s);
      else
        this->set_hs_(h, s + dir * 0.04f);
      return;
    }
    this->set_slider_(c, this->slider_value_(c) + dir * slider_spec(c).enc_step);
  }

  void activate_(const Control &c) {
    switch (c.id) {
      case C_RAIN_AUTO:
        this->ha_switch_(E_RAIN_AUTO, !id(rain_automation).state);
        break;
      case C_WIND_AUTO:
        this->ha_switch_(E_WIND_AUTO, !id(multifan_auto).state);
        break;
      case C_WEATHER_SET:
        this->ha_switch_(E_WEATHER_SET, !id(weather_setpoint).state);
        break;
      case C_GROW_AUTO:
      case C_SCENIC_AUTO:
        this->ha_switch_(E_LIGHT_AUTO, !id(light_automation).state);
        break;
      case C_DAY_MANUAL:
        this->ha_switch_(E_DAY_MANUAL, !id(daylight_manual).state);
        break;
      case C_GRAPH:
        this->range_ = (this->range_ + 1) % HIST_RANGE_COUNT;
        this->graph_pushes_ = history.pushes(this->range_);
        this->invalidate_graph_();
        break;
      case C_LET_IT_RAIN:
        id(ha_call).execute(std::string("button.press"), std::string(E_LET_IT_RAIN));
        break;
      case C_REFILLED:
        if (this->confirmed_(C_REFILLED))
          id(ha_call).execute(std::string("button.press"), std::string(E_REFILLED));
        break;
      case C_SUNRISE_TEST:
        id(ha_call).execute(std::string("button.press"), std::string(E_SUNRISE_TEST));
        break;
      case C_SUNSET_TEST:
        id(ha_call).execute(std::string("button.press"), std::string(E_SUNSET_TEST));
        break;
      case C_WEATHER_TEST:  // next option, wrapping around to "(live)"
        id(ha_call).execute(std::string("select.select_next"), std::string(E_WEATHER_OVERRIDE));
        break;
      default:
        break;
    }
  }

  // Two-step CONFIRM button: the first activation arms it for CONFIRM_MS ("Confirm?"), the second
  // one within that time returns true.
  bool confirmed_(Ctl c) {
    const bool armed = this->armed_(c);
    if (this->armed_until_ != 0 && !armed)
      this->invalidate_ctl_(this->armed_ctl_);  // another button was armed
    this->armed_until_ = armed ? 0 : millis() + CONFIRM_MS;
    this->armed_ctl_ = c;
    this->invalidate_ctl_(c);
    return armed;
  }
  bool armed_(Ctl c) const { return this->armed_until_ != 0 && this->armed_ctl_ == c; }

  void ha_switch_(const char *entity, bool on) {
    id(ha_call).execute(std::string(on ? "switch.turn_on" : "switch.turn_off"), std::string(entity));
  }

  // ---------------------------------------------------------------- sending values
  void send_due_() {
    const uint32_t now = millis();
    for (int c = 0; c < C_COUNT; c++)
      if (this->send_pending_[c] && now - this->last_send_[c] >= SEND_INTERVAL_MS)
        this->send_now_((Ctl) c);
  }

  void send_now_(Ctl c) {
    if (!this->send_pending_[c])
      return;
    this->send_pending_[c] = false;
    this->last_send_[c] = millis();
    this->hold_(c);
    switch (c) {
      case C_WIND_SPEED:
        id(ha_number).execute(std::string(E_WIND_SPEED), this->local_[c]);
        break;
      case C_THERMO_SET:
        id(ha_climate_temperature).execute(std::string(E_THERMOSTAT), this->local_[c]);
        break;
      case C_DAY_LEVEL:
        id(ha_number).execute(std::string(E_DAY_LEVEL), this->local_[c]);
        break;
      case C_GROW_LEVEL:
        if (this->local_[c] > 0)
          id(ha_light_brightness).execute(std::string(E_GROW_LIGHT), (int) std::lround(this->local_[c]));
        else
          id(ha_call).execute(std::string("light.turn_off"), std::string(E_GROW_LIGHT));
        break;
      case C_SCENIC_WHEEL:
      case C_SCENIC_BRI: {
        float h, s;
        this->scenic_current_(h, s);
        const float bri = this->slider_value_(C_SCENIC_BRI);
        // a colour pick on a dark light turns it on at full brightness
        const float b = (c == C_SCENIC_WHEEL && bri <= 0.f) ? 100.f : bri;
        if (b > 0)
          id(ha_light_hs).execute(std::string(E_SCENIC_LIGHT), h, s * 100.f, (int) std::lround(b * 2.55f));
        else
          id(ha_call).execute(std::string("light.turn_off"), std::string(E_SCENIC_LIGHT));
        break;
      }
      case C_BACKLIGHT: {
        auto call = id(backlight).turn_on();
        call.set_brightness(this->local_[c] / 100.f);
        call.perform();
        break;
      }
      default:
        break;
    }
  }

  // ---------------------------------------------------------------- drawing primitives
  // All primitives clip to area_ (the stripe, or the stripe part of the redraw region).
  bool vis_(const Box &b) const { return this->area_.intersects(b); }

  void fill_(Display &it, const Box &b, Color c) {
    const Box i = b.intersect(this->area_);
    if (!i.empty())
      it.filled_rectangle(i.x, i.y, i.w, i.h, c);
  }

  void frame_(Display &it, const Box &b, Color c, int t) {
    this->fill_(it, Box{b.x, b.y, b.w, t}, c);
    this->fill_(it, Box{b.x, b.y + b.h - t, b.w, t}, c);
    this->fill_(it, Box{b.x, b.y, t, b.h}, c);
    this->fill_(it, Box{b.x + b.w - t, b.y, t, b.h}, c);
  }

  void text_(Display &it, int x, int y, font::Font *f, Color c, TextAlign a, const char *s) {
    int bx, by, bw, bh;
    it.get_text_bounds(x, y, s, f, a, &bx, &by, &bw, &bh);
    if (this->vis_(Box{bx, by, bw, bh}))
      it.print(x, y, f, c, a, s);
  }

  // ---------------------------------------------------------------- widgets
  void draw_focus_(Display &it, const Control &c) {
    if (this->page_ == Page::OVERVIEW || this->page_ == Page::MENU)
      return;
    int n;
    const Control *cs = controls(this->page_, n);
    if (this->focus_ < 0 || this->focus_ >= n || cs[this->focus_].id != c.id)
      return;
    this->frame_(it, c.box, this->editing_ ? EDIT : FOCUS, this->editing_ ? 3 : 2);
  }

  void draw_toggle_(Display &it, const Control &c, const char *icon, const char *label, bool on) {
    if (!this->vis_(c.box))
      return;
    const int cy = c.box.y + c.box.h / 2;
    int x = c.box.x + 8;
    if (icon != nullptr) {
      this->text_(it, x, cy, &id(icon_small), on ? TEXT : TEXT_DIM, TextAlign::CENTER_LEFT, icon);
      x += 26;
    }
    this->text_(it, x, cy, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, label);
    const Box pill{c.box.x + c.box.w - 62, cy - 12, 54, 24};
    this->fill_(it, pill, on ? ON_GREEN : DISABLED);
    const Box knob{on ? pill.x + pill.w - 22 : pill.x + 2, pill.y + 2, 20, 20};
    this->fill_(it, knob, TEXT);
    this->draw_focus_(it, c);
  }

  void draw_button_(Display &it, const Control &c, const char *icon, const char *label, Color bg) {
    if (!this->vis_(c.box))
      return;
    this->fill_(it, Box{c.box.x + 3, c.box.y + 3, c.box.w - 6, c.box.h - 6}, bg);
    const int cy = c.box.y + c.box.h / 2;
    int bx, by, bw, bh;
    it.get_text_bounds(0, 0, label, &id(tiny_font), TextAlign::TOP_LEFT, &bx, &by, &bw, &bh);
    const int total = (icon != nullptr ? 26 : 0) + bw;
    int x = c.box.x + (c.box.w - total) / 2;
    if (icon != nullptr) {
      this->text_(it, x, cy, &id(icon_small), TEXT, TextAlign::CENTER_LEFT, icon);
      x += 26;
    }
    this->text_(it, x, cy, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, label);
    this->draw_focus_(it, c);
  }

  void draw_slider_(Display &it, const Control &c, Color fill = Color(90, 70, 20)) {
    if (!this->vis_(c.box))
      return;
    const bool en = this->enabled_(c.id);
    const float v = this->slider_value_(c.id);
    const SliderSpec sp = slider_spec(c.id);
    const Box track{c.box.x + 4, c.box.y + 4, c.box.w - 8, c.box.h - 8};
    this->fill_(it, track, TILE_BG);
    const int fw = (int) std::lround(track.w * clamp((v - sp.min) / (sp.max - sp.min), 0.f, 1.f));
    const Color fill_c = en ? fill : DISABLED;
    const Box filled{track.x, track.y, fw, track.h};
    const Box rest{track.x + fw, track.y, track.w - fw, track.h};
    this->fill_(it, filled, fill_c);
    char buf[24];
    snprintf(buf, sizeof(buf), en ? sp.fmt : sp.fmt_auto, v);
    // Inverted text: over the filled part black or white by the fill's luminance, over the dark
    // track white, split exactly at the fill edge.
    const int tx = track.x + track.w / 2, ty = track.y + track.h / 2;
    const Color on_track = en ? TEXT : TEXT_DIM;
    this->text_clipped_(it, filled, tx, ty, &id(tiny_font), en ? contrast_text(fill_c) : on_track, buf);
    this->text_clipped_(it, rest, tx, ty, &id(tiny_font), on_track, buf);
    this->draw_focus_(it, c);
  }

  // Black on light backgrounds, white on dark ones (perceived luminance).
  static Color contrast_text(Color bg) {
    return (299 * bg.r + 587 * bg.g + 114 * bg.b) / 1000 > 140 ? Color::BLACK : TEXT;
  }

  // Centred text, drawn only inside box b.
  void text_clipped_(Display &it, const Box &b, int x, int y, font::Font *f, Color c, const char *s) {
    const Box clip = b.intersect(this->area_);
    if (clip.empty())
      return;
    it.start_clipping(display::Rect(clip.x, clip.y, clip.w, clip.h));
    this->text_(it, x, y, f, c, TextAlign::CENTER, s);
    it.end_clipping();
  }

  void draw_slider_label_(Display &it, const char *label) {
    this->text_(it, 12, SLIDER_LABEL_Y, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, label);
  }

  // Colour wheel: hue = angle (0 deg = red at the right, clockwise like Home Assistant's wheel),
  // saturation = radius.
  // Computed per pixel for exactly the area being redrawn, so a cursor move costs ~2x 21x21 px.
  void draw_wheel_(Display &it, const Control &c) {
    if (!this->vis_(c.box))
      return;
    const bool en = this->enabled_(c.id);
    const int x0 = WHEEL_CX - WHEEL_R, y0 = WHEEL_CY - WHEEL_R;
    const Box b = Box{x0, y0, 2 * WHEEL_R + 1, 2 * WHEEL_R + 1}.intersect(this->area_);
    auto &img = id(colorwheel);
    for (int y = b.y; y < b.y + b.h; y++) {
      const int dy = WHEEL_CY - y;
      for (int x = b.x; x < b.x + b.w; x++) {
        const int dx = x - WHEEL_CX;
        if (dx * dx + dy * dy > WHEEL_R * WHEEL_R)
          continue;
        Color px = img.get_pixel(x - x0, y - y0);
        if (!en)  // dimmed while the light automation owns the scenic light
          px = Color(px.r * 3 / 8, px.g * 3 / 8, px.b * 3 / 8);
        it.draw_pixel_at(x, y, px);
      }
    }
    // cursor
    float h, s;
    this->scenic_current_(h, s);
    const Box cb = cursor_box(h, s);
    if (this->vis_(cb)) {
      const int px = cb.x + 10, py = cb.y + 10;
      if (this->editing_sat_()) {  // square while the encoder edits saturation
        it.rectangle(px - 8, py - 8, 17, 17, Color::BLACK);
        it.rectangle(px - 7, py - 7, 15, 15, Color::WHITE);
        it.rectangle(px - 6, py - 6, 13, 13, Color::WHITE);
        it.rectangle(px - 5, py - 5, 11, 11, Color::BLACK);
      } else {
        it.circle(px, py, 8, Color::BLACK);
        it.circle(px, py, 7, Color::WHITE);
        it.circle(px, py, 6, Color::WHITE);
        it.circle(px, py, 5, Color::BLACK);
      }
    }
    this->draw_focus_(it, c);
  }

  // ---------------------------------------------------------------- pages
  void draw_page_(Display &it) {
    if (!id(api_connected).state) {
      this->draw_connecting_(it);
      return;
    }
    switch (this->page_) {
      case Page::OVERVIEW:
        this->draw_start_(it);
        break;
      case Page::MENU:
        this->draw_header_(it, "Menu");
        for (int i = 0; i < MENU_COUNT; i++)
          this->draw_menu_tile_(it, i);
        break;
      case Page::THERMO:
        this->draw_header_(it, page_title(this->page_));
        this->draw_thermo_(it);
        break;
      case Page::HYGRO:
        this->draw_header_(it, page_title(this->page_));
        this->draw_hygro_(it);
        break;
      case Page::WIND:
        this->draw_header_(it, page_title(this->page_));
        this->draw_wind_(it);
        break;
      case Page::GROW:
        this->draw_header_(it, page_title(this->page_));
        this->draw_grow_(it);
        break;
      case Page::SCENIC:
        this->draw_header_(it, page_title(this->page_));
        this->draw_scenic_(it);
        break;
      case Page::GECKO:
        this->draw_header_(it, page_title(this->page_));
        this->draw_gecko_(it);
        break;
      case Page::TESTS:
        this->draw_header_(it, page_title(this->page_));
        this->draw_tests_(it);
        break;
      case Page::SETTINGS:
        this->draw_header_(it, page_title(this->page_));
        this->draw_settings_(it);
        break;
    }
  }

  static const char *page_title(Page p) {
    switch (p) {
      case Page::THERMO: return "Thermo";
      case Page::HYGRO: return "Hygro & Rain";
      case Page::WIND: return "Wind";
      case Page::GROW: return "Sunlight";
      case Page::SCENIC: return "Scenic light";
      case Page::GECKO: return "Gecko";
      case Page::TESTS: return "Light tests";
      case Page::SETTINGS: return "Settings";
      default: return "";
    }
  }
  // Shorter names for the 72 px menu tiles
  static const char *menu_label(Page p) {
    switch (p) {
      case Page::HYGRO: return "Hygro";
      case Page::SCENIC: return "Scenic";
      case Page::TESTS: return "Tests";
      default: return page_title(p);
    }
  }

  void draw_connecting_(Display &it) {
    const Box hdr{0, 0, W, HEADER_H};
    this->fill_(it, hdr, HEADER_BG);
    this->text_(it, 6, HEADER_H / 2, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, "Connecting to HASS");
    this->fill_(it, Box{W - 6 - 4 * 16, 9, 4 * id(wi), 8}, TEXT);
    if (this->vis_(Box{(W - 190) / 2, 30, 190, 184}))
      it.image((W - 190) / 2, 30, &id(gecko_logo));
    this->text_(it, W / 2, H - 4, &id(tiny_font), TEXT, TextAlign::BASELINE_CENTER, "Geckocontrol V2.0");
  }

  // Header: back arrow + title on menu/subpages, clock on the overview, wifi bars always.
  void draw_header_(Display &it, const char *title) {
    const Box hdr{0, 0, W, HEADER_H};
    if (!this->vis_(hdr))
      return;
    this->fill_(it, hdr, HEADER_BG);
    if (title != nullptr) {
      this->text_(it, 8, HEADER_H / 2, &id(icon_small), TEXT, TextAlign::CENTER_LEFT, ICON_ARROW_LEFT);
      this->text_(it, 36, HEADER_H / 2, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, title);
      if (this->page_ != Page::MENU && this->focus_ < 0)
        this->frame_(it, BACK_FOCUS, FOCUS, 2);
    } else {
      auto now = id(ha_time).now();
      if (now.is_valid()) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.minute);
        this->text_(it, 8, HEADER_H / 2, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, buf);
      }
    }
    this->draw_wifi_(it);
  }
  void draw_wifi_(Display &it) {
    const float dbm = sval(&id(wifisignal));
    const int bars = std::isnan(dbm) ? 0 : (int) std::lround((dbm + 100.f) / 10.f);
    for (int i = 0; i < 5; i++) {
      const Box b{W - 30 + i * 5, 15 - i * 2, 4, i * 2 + 4};
      if (!this->vis_(b))
        continue;
      if (i < bars)
        this->fill_(it, b, TEXT);
      else
        it.rectangle(b.x, b.y, b.w, b.h, TEXT_DIM);
    }
  }

  // Colours: green on target, red/blue for too warm/cold (too dry/wet)
  static Color temp_color(float d) {
    if (std::isnan(d))
      return NO_DATA;
    return Color(clamp(85.f * d, 0.f, 255.f), clamp(255.f - 28.f * d * d, 0.f, 255.f), clamp(-85.f * d, 0.f, 255.f));
  }
  static Color hum_color(float d) {
    if (std::isnan(d))
      return NO_DATA;
    return Color(clamp(-36.4f * d, 0.f, 255.f), clamp(255.f - 5.2f * d * d, 0.f, 255.f), clamp(36.4f * d, 0.f, 255.f));
  }

  void draw_menu_tile_(Display &it, int i) {
    const Box b = menu_tile_box(i);
    if (!this->vis_(b))
      return;
    const bool focused = i == this->menu_focus_;
    this->fill_(it, b, focused ? FOCUS : TILE_BG);
    this->fill_(it, Box{b.x + 3, b.y + 3, b.w - 6, b.h - 6}, TILE_BG);

    const int cx = b.x + b.w / 2;
    char value[24] = "";
    const char *icon = "";
    Color ic = TEXT;
    switch (MENU_PAGES[i]) {
      case Page::THERMO: {
        icon = ICON_THERMOMETER;
        const float t = sval(&id(thermostat_current_temperature));
        if (!std::isnan(t))
          snprintf(value, sizeof(value), "%.1f°", t);  // "°C" is wider than the tile
        ic = temp_color(t - sval(&id(thermostat_target_temperature)));
        break;
      }
      case Page::HYGRO: {
        icon = ICON_WATER_PERCENT;
        const float h = sval(&id(humidity_current));
        if (!std::isnan(h))
          snprintf(value, sizeof(value), "%.0f%%", h);
        ic = hum_color(h - sval(&id(humidity_target)));
        break;
      }
      case Page::WIND: {
        icon = ICON_WEATHER_WINDY;
        const float p = sval(&id(multifan_percentage));
        if (!std::isnan(p))
          snprintf(value, sizeof(value), "%.0f%%", p);
        break;
      }
      case Page::GROW: {  // the growlight: what the terrarium looks like
        icon = ICON_SUNNY;
        const float g = sval(&id(grow_light_brightness));
        if (id(grow_light_on).state && !std::isnan(g))
          snprintf(value, sizeof(value), "%.0f%%", g / 2.55f);
        else
          snprintf(value, sizeof(value), "off");
        ic = id(grow_light_on).state ? Color(120, 230, 90) : TEXT_DIM;
        break;
      }
      case Page::SCENIC: {
        icon = ICON_PALETTE;
        const float s = sval(&id(scenic_light_brightness));
        if (id(scenic_light_on).state && !std::isnan(s))
          snprintf(value, sizeof(value), "%.0f%%", s / 2.55f);
        else
          snprintf(value, sizeof(value), "off");
        if (id(scenic_light_on).state) {
          float h, sat;
          this->scenic_current_(h, sat);
          ic = hsv_color(h, sat, 1.f);
        } else {
          ic = TEXT_DIM;
        }
        break;
      }
      case Page::GECKO:  // its weight while in the nest, "out" otherwise
        if (this->gecko_known_ && !this->gecko_in_) {
          snprintf(value, sizeof(value), "out");
        } else if (this->gecko_known_) {
          const float g = sval(&id(scale_weight));
          snprintf(value, sizeof(value), std::isnan(g) ? "in" : "%.0f g", g);
        }
        break;
      case Page::TESTS:  // highlighted while a test runs
        icon = ICON_SUNSET_UP;
        ic = sweep_test() != nullptr || !weather_live() ? ARMED : TEXT;
        break;
      case Page::SETTINGS:
        icon = ICON_COG;
        break;
      default:
        break;
    }
    if (MENU_PAGES[i] == Page::GECKO)
      it.image(cx, b.y + 28, &id(gecko_icon), display::ImageAlign::CENTER);
    else
      this->text_(it, cx, b.y + 28, &id(icon_font), ic, TextAlign::CENTER, icon);
    this->text_(it, cx, b.y + 58, &id(tiny_font), TEXT_DIM, TextAlign::CENTER, menu_label(MENU_PAGES[i]));
    if (value[0])
      this->text_(it, cx, b.y + 84, &id(mid_font), TEXT, TextAlign::CENTER, value);
  }

  // A labelled value line: label left (in its graph trace colour = legend), value right
  void value_line_(Display &it, int y, const char *label, Color lc, const char *value) {
    this->text_(it, 12, y, &id(tiny_font), lc, TextAlign::CENTER_LEFT, label);
    this->text_(it, VALUES_LEFT.w - 10, y, &id(tiny_font), TEXT, TextAlign::CENTER_RIGHT, value);
  }

  // Values next to a graph: big current value with a legend dot in its trace colour, a status bar,
  // up to three labelled lines. Shared by Thermo, Hygro & Rain and Wind.
  void draw_values_left_(Display &it, const char *big, const char *unit, Color dot, Color bar, float bar_frac) {
    it.filled_circle(12, 52, 4, dot);
    this->text_(it, 80, 52, &id(big_font), TEXT, TextAlign::CENTER, big);
    if (unit != nullptr) {
      int bx, by, bw, bh;
      it.get_text_bounds(80, 52, big, &id(big_font), TextAlign::CENTER, &bx, &by, &bw, &bh);
      this->text_(it, bx + bw + 4, 58, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, unit);
    }
    const Box track{12, 76, VALUES_LEFT.w - 22, 5};
    if (bar_frac >= 0.f) {  // level bar
      this->fill_(it, track, TILE_BG);
      this->fill_(it, Box{track.x, track.y, (int) std::lround(track.w * clamp(bar_frac, 0.f, 1.f)), track.h}, bar);
    } else {  // status colour
      this->fill_(it, track, bar);
    }
  }
  static constexpr int LINE_Y[3] = {94, 110, 126};

  bool has_graph_() const {
    return this->page_ == Page::THERMO || this->page_ == Page::HYGRO || this->page_ == Page::WIND ||
           this->page_ == Page::GECKO;
  }
  // The graph and everything computed from its range
  void invalidate_graph_() {
    if (!this->has_graph_())
      return;
    this->invalidate(GRAPH_BOX);
    if (this->page_ == Page::GECKO)
      this->invalidate(VALUES_LEFT);  // the average follows the graph's range
  }

  // History graph in GRAPH_BOX for the selected range, oldest left, now at the right edge. The y
  // axis fits all traces, widened to whole multiples of y_grid (one dotted line per multiple).
  // Optional extras on their own 0-100% scale: `band` tints each column's background by its level,
  // `level` is drawn as a line wherever it is above 0. `mark` is a dashed red reference line (an
  // alarm threshold), always inside the y range. Drawn column by column, clipped to area_.
  void draw_graph_(Display &it, const Trace *traces, int n, float y_grid, int band = -1, int level = -1,
                   float mark = NAN) {
    const Box g = GRAPH_BOX;
    const Box clip = g.intersect(this->area_);
    if (clip.empty())
      return;
    const int r = this->range_;
    const int x0 = clip.x - g.x, x1 = clip.x + clip.w - g.x;  // visible columns

    float lo = NAN, hi = NAN;
    for (int t = 0; t < n; t++) {
      for (int x = 0; x < HIST_LEN; x++) {
        const float v = history.get(traces[t].series, r, x);
        if (std::isnan(v))
          continue;
        lo = std::isnan(lo) ? v : std::min(lo, v);
        hi = std::isnan(hi) ? v : std::max(hi, v);
      }
    }
    if (!std::isnan(mark)) {
      lo = std::isnan(lo) ? mark : std::min(lo, mark);
      hi = std::isnan(hi) ? mark : std::max(hi, mark);
    }
    float yn = 0.f, ym = y_grid;
    if (!std::isnan(lo)) {
      yn = std::floor(lo / y_grid) * y_grid;
      ym = std::max(std::ceil(hi / y_grid) * y_grid, yn + y_grid);
    }
    // at most ~6 grid lines: coarsen the grid on a wide range
    float step = y_grid;
    while ((ym - yn) / step > 6.f)
      step *= 2.f;
    const int bottom = g.y + g.h - 1;
    auto to_y = [&](float v, float a, float b) {
      return bottom - (int) std::lround(clamp((v - a) / (b - a), 0.f, 1.f) * (g.h - 1));
    };
    auto px = [&](int x, int y, Color c) {
      if (this->area_.contains(x, y))
        it.draw_pixel_at(x, y, c);
    };
    // vertical run in column x from y a to y b (any order), clipped
    auto vrun = [&](int x, int a, int b, Color c) {
      if (a > b)
        std::swap(a, b);
      this->fill_(it, Box{x, a, 1, b - a + 1}, c);
    };

    if (band >= 0) {
      for (int x = x0; x < x1; x++) {
        const float v = history.get(band, r, x);
        if (std::isnan(v) || v <= 0.f)
          continue;
        const float a = clamp(v / 100.f, 0.f, 1.f) * 0.55f;
        auto mix = [a](uint8_t from, uint8_t to) { return (uint8_t) std::lround(from + (to - from) * a); };
        const Color c(mix(BG.r, T_HEAT_LAMP.r), mix(BG.g, T_HEAT_LAMP.g), mix(BG.b, T_HEAT_LAMP.b));
        vrun(g.x + x, g.y, bottom, c);
      }
    }

    // grid: dotted lines per y step, and per time step counted back from now (right edge)
    for (float v = yn + step; v < ym - step / 2.f; v += step) {
      const int y = to_y(v, yn, ym);
      for (int x = x0 - (x0 & 1); x < x1; x += 2)
        px(g.x + x, y, GRID);
    }
    const int cols = (int) (HIST_RANGES[r].duration_ms / HIST_RANGES[r].grid_ms);
    for (int i = 1; i < cols; i++) {
      const int x = g.x + g.w - 1 - (int) std::lround((float) i * g.w / cols);
      if (x < clip.x || x >= clip.x + clip.w)
        continue;
      for (int y = g.y; y < g.y + g.h; y += 2)
        px(x, y, GRID);
    }

    auto line = [&](int s, float a, float b, Color c, int thick, bool skip_zero, Color (*color_of)(float)) {
      int prev = INT32_MIN;
      // start one column early so the run into the first visible column connects
      for (int x = std::max(0, x0 - 1); x < x1; x++) {
        const float v = history.get(s, r, x);
        if (std::isnan(v) || (skip_zero && v <= 0.f)) {
          prev = INT32_MIN;
          continue;
        }
        const int y = to_y(v, a, b);
        if (x >= x0) {
          const int from = prev == INT32_MIN ? y : prev;
          vrun(g.x + x, std::min(from, y), std::min(std::max(from, y) + thick - 1, bottom),
               color_of != nullptr ? color_of(v) : c);
        }
        prev = y;
      }
    };
    if (level >= 0)
      line(level, 0.f, 100.f, T_DAYLIGHT, 1, true, nullptr);
    if (!std::isnan(mark)) {  // dashed: 4 on, 2 off
      const int y = to_y(mark, yn, ym);
      for (int x = x0; x < x1; x++)
        if (x % 6 < 4)
          px(g.x + x, y, WARN);
    }
    for (int t = 0; t < n; t++)
      line(traces[t].series, yn, ym, traces[t].color, traces[t].thick, false, traces[t].color_of);

    this->frame_(it, g, TEXT_DIM, 1);
    this->text_(it, g.x + 4, g.y + 2, &id(tiny_font), TEXT_DIM, TextAlign::TOP_LEFT, HIST_RANGES[r].label);
    this->draw_focus_(it, THERMO_CONTROLS[2]);  // C_GRAPH, same box on every graph page
  }

  void draw_thermo_(Display &it) {
    if (this->vis_(VALUES_LEFT)) {
      const float cur = sval(&id(thermostat_current_temperature));
      const float tgt = sval(&id(thermostat_target_temperature));
      const float wx = sval(&id(weather_temperature));
      char buf[24];
      snprintf(buf, sizeof(buf), std::isnan(cur) ? "-" : "%.1f°", cur);
      this->draw_values_left_(it, buf, nullptr, T_NOW, temp_color(cur - tgt), -1.f);
      // The target is shown by the setpoint slider (and the green graph trace) only.
      const float gnd = sval(&id(ground_temperature));
      snprintf(buf, sizeof(buf), std::isnan(gnd) ? "-" : "%.1f°C", gnd);
      this->value_line_(it, LINE_Y[0], "ground", T_GROUND, buf);
      // Weather only while the setpoint is manual; otherwise it equals the target.
      if (!id(weather_setpoint).state) {
        snprintf(buf, sizeof(buf), std::isnan(wx) ? "-" : "%.1f°C", wx);
        this->value_line_(it, LINE_Y[2], "weather", T_WEATHER, buf);
      }
      // the active heater: daylight lamp by day, heat lamp at night
      const bool day = id(daytime).state;
      const bool on = day ? id(daylight_on).state : id(heat_lamp_on).state;
      const float b = sval(day ? &id(daylight_brightness) : &id(heat_lamp_brightness));
      if (on && !std::isnan(b))
        snprintf(buf, sizeof(buf), "%.0f%%", b / 2.55f);
      else
        snprintf(buf, sizeof(buf), "off");
      this->value_line_(it, LINE_Y[1], day ? "daylight" : "heat lamp", day ? T_DAYLIGHT : T_HEAT_LAMP, buf);
    }
    // Heating: heat lamp as a pink background by its duty, daylight lamp as an orange line on its
    // own 0-100% scale.
    this->draw_graph_(it, THERMO_TRACES, 3, 1.f, S_HEAT_LAMP, S_DAYLIGHT);
    this->draw_toggle_(it, THERMO_CONTROLS[0], ICON_THERMOMETER, "Weather setpoint", id(weather_setpoint).state);
    this->draw_slider_label_(it, "Setpoint");
    this->draw_slider_(it, THERMO_CONTROLS[1]);
  }

  void draw_hygro_(Display &it) {
    if (this->vis_(VALUES_LEFT)) {
      const float cur = sval(&id(humidity_current));
      const float tgt = sval(&id(humidity_target));
      char buf[24];
      snprintf(buf, sizeof(buf), std::isnan(cur) ? "-" : "%.0f%%", cur);
      this->draw_values_left_(it, buf, nullptr, T_NOW, hum_color(cur - tgt), -1.f);
      // The controller sets the hygrostat target from the weather humidity, so one line (in the
      // weather trace colour) covers both.
      snprintf(buf, sizeof(buf), std::isnan(tgt) ? "-" : "%.0f%%", tgt);
      this->value_line_(it, LINE_Y[0], "target", T_WEATHER, buf);
      const float ceil = sval(&id(ceiling_humidity)), gnd = sval(&id(ground_humidity));
      snprintf(buf, sizeof(buf), std::isnan(ceil) ? "-" : "%.0f%%", ceil);
      this->value_line_(it, LINE_Y[1], "ceiling", T_CEILING, buf);
      snprintf(buf, sizeof(buf), std::isnan(gnd) ? "-" : "%.0f%%", gnd);
      this->value_line_(it, LINE_Y[2], "ground", T_GROUND, buf);
    }
    this->draw_graph_(it, HYGRO_TRACES, 5, 10.f);

    this->draw_toggle_(it, HYGRO_CONTROLS[0], ICON_WEATHER_RAINY, "Rain automation", id(rain_automation).state);

    if (this->vis_(RESERVOIR_ROW)) {
      const float lvl = sval(&id(reservoir_level));
      const bool low = !std::isnan(lvl) && lvl < 20.f;
      const int cy = RESERVOIR_ROW.y + RESERVOIR_ROW.h / 2;
      this->text_(it, 16, cy, &id(icon_small), low ? WARN : TEXT, TextAlign::CENTER_LEFT, ICON_CUP_WATER);
      this->text_(it, 42, cy, &id(tiny_font), T_LEVEL, TextAlign::CENTER_LEFT, "Reservoir");
      const Box bar{130, cy - 7, 120, 14};
      this->fill_(it, bar, TILE_BG);
      if (!std::isnan(lvl))
        this->fill_(it, Box{bar.x, bar.y, (int) std::lround(bar.w * clamp(lvl, 0.f, 100.f) / 100.f), bar.h},
                    low ? WARN : T_LEVEL);
      char buf[12];
      snprintf(buf, sizeof(buf), std::isnan(lvl) ? "-" : "%.0f%%", lvl);
      this->text_(it, W - 12, cy, &id(tiny_font), low ? WARN : TEXT, TextAlign::CENTER_RIGHT, buf);
    }

    this->draw_button_(it, HYGRO_CONTROLS[1], ICON_WEATHER_RAINY, "Let it rain", TILE_BG);
    const bool armed = this->armed_(C_REFILLED);
    this->draw_button_(it, HYGRO_CONTROLS[2], armed ? nullptr : ICON_CUP_WATER, armed ? "Confirm?" : "Refilled",
                       armed ? ARMED : TILE_BG);
  }

  void draw_wind_(Display &it) {
    if (this->vis_(VALUES_LEFT)) {
      const float p = sval(&id(multifan_percentage));
      char buf[24];
      snprintf(buf, sizeof(buf), std::isnan(p) ? "-" : (p > 0 ? "%.0f%%" : "off"), p);
      this->draw_values_left_(it, buf, nullptr, T_LEVEL, T_LEVEL, std::isnan(p) ? 0.f : p / 100.f);
      const float rpm = sval(&id(fan_tacho));
      snprintf(buf, sizeof(buf), std::isnan(rpm) ? "-" : "%.0f rpm", rpm);
      this->value_line_(it, LINE_Y[0], "fan", TEXT_DIM, buf);
      this->value_line_(it, LINE_Y[1], "mode", TEXT_DIM, id(multifan_auto).state ? "automatic" : "manual");
      const float wx = sval(&id(weather_windspeed));
      snprintf(buf, sizeof(buf), std::isnan(wx) ? "-" : "%.0f km/h", wx);
      this->value_line_(it, LINE_Y[2], "weather", T_WEATHER, buf);
    }
    this->draw_graph_(it, WIND_TRACES, 2, 20.f);
    this->draw_toggle_(it, WIND_CONTROLS[0], ICON_WEATHER_WINDY, "Automatic", id(multifan_auto).state);
    this->draw_slider_label_(it, "Manual speed");
    this->draw_slider_(it, WIND_CONTROLS[1]);
  }

  void draw_grow_(Display &it) {
    // growlight: light automation, level; the sun's share it is following
    this->draw_toggle_(it, GROW_CONTROLS[0], ICON_SPROUT, "Light automation", id(light_automation).state);
    if (this->vis_(GROW_INFO)) {
      const int cy = GROW_INFO.y + GROW_INFO.h / 2;
      this->text_(it, 12, cy, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, "Grow light");
      const float ill = sval(&id(illumination));
      if (!std::isnan(ill)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "sun %.0f%%", ill);
        this->text_(it, W - 12, cy, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_RIGHT, buf);
      }
    }
    this->draw_slider_(it, GROW_CONTROLS[1]);
    // daylight lamp: by the thermostat or by hand
    this->draw_toggle_(it, GROW_CONTROLS[2], ICON_SUNNY, "Daylight manual", id(daylight_manual).state);
    if (this->vis_(DAY_INFO))
      this->text_(it, 12, DAY_INFO.y + DAY_INFO.h / 2, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT,
                  "Daylight lamp (heater)");
    this->draw_slider_(it, GROW_CONTROLS[3], Color(130, 100, 40));
  }

  void draw_scenic_(Display &it) {
    float h, s;
    this->scenic_current_(h, s);
    this->draw_wheel_(it, SCENIC_CONTROLS[1]);
    this->draw_slider_(it, SCENIC_CONTROLS[2], hsv_color(h, s, 1.f));
    this->draw_toggle_(it, SCENIC_CONTROLS[0], nullptr, "Auto", id(light_automation).state);
    // static texts (in rows no value change touches)
    this->text_(it, 192, 96, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, "drag the wheel,");
    this->text_(it, 192, 116, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, "or push + turn:");
    this->text_(it, 192, 136, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, "hue, then sat");
    this->text_(it, SCENIC_HUE_X, 192, &id(tiny_font), TEXT_DIM, TextAlign::CENTER, "Hue");
    this->text_(it, SCENIC_SAT_X, 192, &id(tiny_font), TEXT_DIM, TextAlign::CENTER, "Sat");
    if (this->vis_(SCENIC_VALUES))
      this->draw_scenic_values_(it);
  }

  // Hue / sat numbers right of the brightness slider; the one the encoder edits is framed.
  void draw_scenic_values_(Display &it) {
    float h, s;
    this->scenic_current_(h, s);
    const bool edit = this->editing_ && this->focus_ >= 0 && SCENIC_CONTROLS[this->focus_].id == C_SCENIC_WHEEL;
    char buf[12];
    const int cy = SCENIC_VALUES.y + 14;
    snprintf(buf, sizeof(buf), "%.0f°", h);
    this->text_(it, SCENIC_HUE_X, cy, &id(mid_font), TEXT, TextAlign::CENTER, buf);
    snprintf(buf, sizeof(buf), "%.0f%%", s * 100.f);
    this->text_(it, SCENIC_SAT_X, cy, &id(mid_font), TEXT, TextAlign::CENTER, buf);
    if (edit)  // same frame as an edited slider
      this->frame_(it, Box{(this->wheel_part_ == 0 ? SCENIC_HUE_X : SCENIC_SAT_X) - 31, SCENIC_VALUES.y, 62, 32}, EDIT,
                   3);
  }

  // ---------------------------------------------------------------- start page
  // The weather in force: the "Weather Override (test)" choice, else the live historic code.
  int weather_code_() const {
    const auto &o = id(weather_override);
    if (o.has_state() && !o.state.empty() && o.state != "(live)") {
      // labels and codes as in the controller's select (gecko.yaml)
      static const struct {
        const char *label;
        int code;
      } OVERRIDES[] = {{"Clear", 0},        {"Partly cloudy", 2},     {"Overcast", 3},  {"Fog", 45},
                       {"Drizzle", 53},     {"Light rain", 61},       {"Rain", 63},     {"Heavy rain", 65},
                       {"Snow", 73},        {"Rain showers", 81},     {"Thunderstorm", 95},
                       {"Severe thunderstorm", 99}};
      for (const auto &m : OVERRIDES)
        if (o.state == m.label)
          return m.code;
    }
    const float c = sval(&id(weather_wmo));
    return std::isnan(c) ? -1 : (int) c;
  }
  bool weather_overridden_() const {
    const auto &o = id(weather_override);
    return o.has_state() && !o.state.empty() && o.state != "(live)";
  }
  static void weather_look(gecko_scene::Sky s, bool night, const char *&icon, const char *&label, Color &col) {
    using gecko_scene::Sky;
    const Color sun(255, 214, 90), moon(215, 222, 255), cloud(225, 232, 242), rain(140, 190, 255);
    switch (s) {
      case Sky::CLEAR: icon = night ? ICON_W_NIGHT : ICON_W_SUNNY; label = "Clear"; col = night ? moon : sun; break;
      case Sky::FAIR: icon = night ? ICON_W_NIGHT_PARTLY : ICON_W_PARTLY; label = "Fair"; col = night ? moon : sun; break;
      case Sky::PARTLY: icon = night ? ICON_W_NIGHT_PARTLY : ICON_W_PARTLY; label = "Partly"; col = cloud; break;
      case Sky::OVERCAST: icon = ICON_W_CLOUDY; label = "Overcast"; col = cloud; break;
      case Sky::FOG: icon = ICON_W_FOG; label = "Fog"; col = cloud; break;
      case Sky::DRIZZLE: icon = ICON_W_RAINY; label = "Drizzle"; col = rain; break;
      case Sky::RAIN: icon = ICON_W_POURING; label = "Rain"; col = rain; break;
      case Sky::SHOWERS: icon = ICON_W_PARTLY_RAINY; label = "Showers"; col = rain; break;
      case Sky::SNOW: icon = ICON_W_SNOWY; label = "Snow"; col = TEXT; break;
      case Sky::STORM: icon = ICON_W_STORM; label = "Storm"; col = sun; break;
    }
  }
  // "2026-11-17T16:47:00Z" (the controller's next sunrise / sunset) -> UTC epoch, 0 if none
  static time_t parse_utc(const std::string &txt) {
    int y, mo, d, h, mi, sec;
    if (sscanf(txt.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6)
      return 0;
    ESPTime t{};
    t.year = y;
    t.month = mo;
    t.day_of_month = d;
    t.hour = h;
    t.minute = mi;
    t.second = sec;
    t.recalc_timestamp_utc(false);
    return t.timestamp;
  }

  void start_step_(uint32_t now) {
    const uint32_t t0 = micros();
    gecko_scene::Inputs in;
    in.wmo = this->weather_code_();
    const float e = sval(&id(sun_elevation));
    in.sun_elevation = !std::isnan(e) ? e : (id(daytime).state ? 30.f : -20.f);
    const float m = sval(&id(moon_illumination));
    in.moon = std::isnan(m) ? 50.f : m;
    if (this->show_)
      this->show_inputs_(in, now);
    this->scene_.set_inputs(in);
    this->scene_.step(now);
    this->step_us_ += micros() - t0;
  }
  // The test sweep's sky over the real one (sweep_check()): blended in over SHOW_IN_MS, and out
  // over SHOW_OUT_MS once the sweep is over.
  void show_inputs_(gecko_scene::Inputs &in, uint32_t now) {
    if (!this->show_ending_ && now - this->show_start_ > SHOW_MS + SHOW_STEP_MS)
      this->show_stop_();  // the end of the sweep went missing
    float w = gecko_scene::smooth(0.f, (float) SHOW_IN_MS, (float) (now - this->show_start_));
    if (this->show_ending_) {
      const uint32_t out = now - this->show_end_;
      if (out >= SHOW_OUT_MS) {
        this->show_ = false;
        return;
      }
      w = std::min(w, 1.f - gecko_scene::smooth(0.f, (float) SHOW_OUT_MS, (float) out));
    }
    // the sun: up from below the horizon (or down) at an even pace, held where it is when it ends
    const uint32_t ran = (this->show_ending_ ? this->show_end_ : now) - this->show_start_;
    const float k = gecko_scene::clamp01((float) ran / SHOW_MS);
    const float e = this->show_rise_ ? SHOW_LOW + (SHOW_HIGH - SHOW_LOW) * k : SHOW_HIGH - (SHOW_HIGH - SHOW_LOW) * k;
    in.sun_elevation += (e - in.sun_elevation) * w;
    in.cover_min = SHOW_COVER * w;
    in.sun_clear = w;
    in.glow = w;
  }
  void show_stop_() {
    if (!this->show_ || this->show_ending_)
      return;
    this->show_ending_ = true;
    this->show_end_ = millis();
  }

  void frame_done_(uint32_t start, uint32_t cost) {
    this->frame_start_ = start;
    this->frame_gap_ = std::max(FRAME_MIN_MS, 2 * cost);
    this->frame_cost_sum_ += cost;
    if (++this->frame_count_ == 100) {
      ESP_LOGD(TAG, "start page: %u ms per frame (step %u, sky %u, overlay %u, rest = transfer), one every %u ms",
               (unsigned) (this->frame_cost_sum_ / 100), (unsigned) (this->step_us_ / 100000),
               (unsigned) (this->sky_us_ / 100000), (unsigned) (this->overlay_us_ / 100000),
               (unsigned) this->frame_gap_);
      this->frame_count_ = 0;
      this->frame_cost_sum_ = 0;
      this->step_us_ = this->sky_us_ = this->overlay_us_ = 0;
    }
  }

  // Text with a drop shadow, legible on the moving sky
  void text_shadow_(Display &it, int x, int y, font::Font *f, Color c, TextAlign a, const char *s) {
    this->text_(it, x + 1, y + 1, f, Color(0, 0, 0), a, s);
    this->text_(it, x, y, f, c, a, s);
  }

  void draw_start_(Display &it) {
    const Box &a = this->area_;
    const uint32_t t0 = micros();
    if (!this->partial_) {  // a whole stripe: straight into the display's buffer
      using Disp = std::remove_reference_t<decltype(id(gecko_display))>;
      using Acc = StripeAccess<Disp>;
      Disp &d = id(gecko_display);
      uint16_t *const fb = Acc::buffer(d);
      const int y0 = Acc::start(d), y1 = Acc::end(d);
      auto put = [fb, y0, y1](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if ((unsigned) x < (unsigned) W && y >= y0 && y < y1)
          fb[(y - y0) * W + x] = Acc::pixel(r, g, b);
      };
      this->scene_.render(0, y0, W, y1 - y0, START_GLASS, 2, put);
      this->scene_.render_particles(0, y0, W, y1 - y0, START_GLASS, 2, put);
      Acc::mark(d);
    } else {
      auto put = [&it](int x, int y, uint8_t r, uint8_t g, uint8_t b) { it.draw_pixel_at(x, y, Color(r, g, b)); };
      this->scene_.render(a.x, a.y, a.w, a.h, START_GLASS, 2, put);
      this->scene_.render_particles(a.x, a.y, a.w, a.h, START_GLASS, 2, put);
    }
    const uint32_t t1 = micros();
    this->sky_us_ += t1 - t0;
    this->draw_start_overlay_(it);
    this->overlay_us_ += micros() - t1;
  }

  void draw_start_overlay_(Display &it) {
    char buf[32];

    // status line: clock, wifi
    auto now = id(ha_time).now();
    if (now.is_valid()) {
      snprintf(buf, sizeof(buf), "%02d:%02d", now.hour, now.minute);
      this->text_shadow_(it, 10, 14, &id(mid_font), TEXT, TextAlign::CENTER_LEFT, buf);
    }
    this->draw_wifi_(it);

    // weather: icon and what it is (orange while the test override is in force)
    if (this->vis_(ST_WEATHER)) {
      const char *icon, *label;
      Color col;
      weather_look(this->scene_.sky(), this->scene_.night(), icon, label, col);
      if (this->weather_code_() < 0)
        label = "-";
      this->text_(it, 44, 64, &id(icon_big), col, TextAlign::CENTER, icon);
      this->text_(it, 44, 107, &id(tiny_font), this->weather_overridden_() ? ARMED : TEXT_DIM, TextAlign::CENTER, label);
    }
    // temperature and humidity: live value, its difference to the setpoint, the setpoint
    auto climate = [&](const Box &b, float cur, float tgt, const char *fmt, const char *dfmt, const char *sfmt,
                       Color dc) {
      if (!this->vis_(b))
        return;
      const int cy = b.y + b.h / 2;
      snprintf(buf, sizeof(buf), std::isnan(cur) ? "-" : fmt, cur);
      this->text_(it, b.x + 4, cy + 1, &id(big_font), TEXT, TextAlign::CENTER_LEFT, buf);
      if (!std::isnan(cur) && !std::isnan(tgt)) {
        snprintf(buf, sizeof(buf), dfmt, cur - tgt);
        this->text_(it, b.x + b.w - 8, cy - 8, &id(tiny_font), dc, TextAlign::CENTER_RIGHT, buf);
      }
      if (!std::isnan(tgt)) {
        snprintf(buf, sizeof(buf), sfmt, tgt);
        this->text_(it, b.x + b.w - 8, cy + 10, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_RIGHT, buf);
      }
    };
    {
      const float cur = sval(&id(thermostat_current_temperature)), tgt = sval(&id(thermostat_target_temperature));
      climate(ST_TEMP, cur, tgt, "%.1f°", "%+.1f", "set %.1f", temp_color(cur - tgt));
    }
    {
      const float cur = sval(&id(humidity_current)), tgt = sval(&id(humidity_target));
      climate(ST_HUM, cur, tgt, "%.0f%%", "%+.0f", "set %.0f", hum_color(cur - tgt));
    }

    // a climate alarm, in place of the next sunrise / sunset
    this->alarm_shown_ = this->alarm_due_();
    if (this->alarm_shown_ != Alarm::NONE) {
      if (this->vis_(ST_SUN))
        this->draw_alarm_(it, this->alarm_shown_);
    }
    // next sunrise / sunset (the colour sweeps start then)
    else if (this->vis_(ST_SUN)) {
      const int cy = ST_SUN.y + ST_SUN.h / 2 + 1;
      const time_t t = epoch_now(), rise = parse_utc(id(next_sunrise).state), set = parse_utc(id(next_sunset).state);
      const std::string &fx = id(scenic_light_effect).state;
      bool sunrise = rise != 0 && (set == 0 || rise < set);
      const time_t next = sunrise ? rise : set;
      if (fx == "Sunrise Color" || fx == "Sunset Color") {
        sunrise = fx == "Sunrise Color";
        if (this->show_ && !this->show_ending_)
          snprintf(buf, sizeof(buf), "Test %s", sunrise ? "sunrise" : "sunset");
        else
          snprintf(buf, sizeof(buf), "%s now", sunrise ? "Sunrise" : "Sunset");
      } else if (next == 0 || t == 0) {
        snprintf(buf, sizeof(buf), "-");
      } else {
        char dur[16];
        fmt_duration(dur, sizeof(dur), next > t ? (uint32_t) (next - t) : 0);
        snprintf(buf, sizeof(buf), "%s in %s", sunrise ? "Sunrise" : "Sunset", dur);
      }
      this->text_(it, ST_SUN.x + 12, cy, &id(icon_small), Color(255, 190, 90), TextAlign::CENTER_LEFT,
                  sunrise ? ICON_SUNSET_UP : ICON_SUNSET_DOWN);
      this->text_(it, ST_SUN.x + 40, cy, &id(mid_font), TEXT, TextAlign::CENTER_LEFT, buf);
      if (next != 0) {
        fmt_clock(buf, sizeof(buf), next);
        this->text_(it, ST_SUN.x + ST_SUN.w - 10, cy, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_RIGHT, buf);
      }
    }
    // the nest: in / out and for how long, the gecko's weight
    if (this->vis_(ST_NEST)) {
      const int cy = ST_NEST.y + ST_NEST.h / 2 - 1;
      it.image(ST_NEST.x + 22, cy, &id(gecko_icon_small), display::ImageAlign::CENTER);
      uint32_t sec;
      if (!this->gecko_known_) {
        snprintf(buf, sizeof(buf), "Nest: no data");
      } else if (!this->gecko_for_(sec)) {
        snprintf(buf, sizeof(buf), "%s", this->gecko_in_ ? "In the nest" : "Out");
      } else {
        char dur[16];
        fmt_duration(dur, sizeof(dur), sec);
        snprintf(buf, sizeof(buf), "%s %s", this->gecko_in_ ? "In the nest" : "Out for", dur);
      }
      this->text_(it, ST_NEST.x + 40, cy, &id(mid_font), TEXT, TextAlign::CENTER_LEFT, buf);
      const float g = this->gecko_in_ ? sval(&id(scale_weight)) : this->last_stay_weight_;
      if (this->gecko_known_ && !std::isnan(g)) {
        snprintf(buf, sizeof(buf), "%.1f g", g);
        this->text_(it, ST_NEST.x + ST_NEST.w - 10, cy, &id(tiny_font), this->gecko_in_ ? TEXT : TEXT_DIM,
                    TextAlign::CENTER_RIGHT, buf);
      }
    }
  }

  // ---------------------------------------------------------------- gecko
  static time_t epoch_now() {
    auto t = id(ha_time).now();
    return t.is_valid() ? t.timestamp : 0;
  }
  // "45 min", "2 h 15", "3 d 4 h"
  static void fmt_duration(char *buf, size_t n, uint32_t seconds) {
    const uint32_t min = seconds / 60;
    if (min < 60)
      snprintf(buf, n, "%u min", (unsigned) min);
    else if (min < 24 * 60)
      snprintf(buf, n, "%u h %02u", (unsigned) (min / 60), (unsigned) (min % 60));
    else
      snprintf(buf, n, "%u d %u h", (unsigned) (min / (24 * 60)), (unsigned) (min / 60 % 24));
  }
  static void fmt_clock(char *buf, size_t n, time_t t) {
    if (t == 0) {
      snprintf(buf, n, "--:--");
      return;
    }
    const ESPTime e = ESPTime::from_epoch_local(t);
    snprintf(buf, n, "%02u:%02u", e.hour, e.minute);
  }
  // How long the current period has lasted; false while the clock is not set.
  bool gecko_for_(uint32_t &seconds) const {
    const time_t now = epoch_now();
    if (now == 0 || !this->gecko_known_)
      return false;
    seconds = now > (time_t) this->gecko_since_ ? (uint32_t) (now - this->gecko_since_) : 0;
    return true;
  }

  void draw_gecko_(Display &it) {
    if (this->vis_(VALUES_LEFT)) {
      char buf[24];
      // How long in / out, big; the state below it
      uint32_t sec;
      if (this->gecko_for_(sec)) {
        fmt_duration(buf, sizeof(buf), sec);
        this->text_(it, 80, 50, &id(big_font), TEXT, TextAlign::CENTER, buf);
      } else {
        this->text_(it, 80, 50, &id(big_font), TEXT, TextAlign::CENTER, "-");
      }
      const char *state = !this->gecko_known_ ? "no reading yet" : this->gecko_in_ ? "in the nest" : "out of the nest";
      this->text_(it, 80, 77, &id(tiny_font), this->gecko_in_ ? T_GECKO : TEXT_DIM, TextAlign::CENTER, state);
      // weight now (or of the last stay while out), average over the graph's range
      const float now = this->gecko_in_ ? sval(&id(scale_weight)) : this->last_stay_weight_;
      snprintf(buf, sizeof(buf), std::isnan(now) ? "-" : "%.1f g", now);
      this->value_line_(it, LINE_Y[0], this->gecko_in_ ? "weight" : "last stay", T_GECKO, buf);
      float sum = 0.f;
      int n = 0;
      for (int x = 0; x < HIST_LEN; x++) {
        const float v = history.get(S_WEIGHT, this->range_, x);
        if (!std::isnan(v)) {
          sum += v;
          n++;
        }
      }
      snprintf(buf, sizeof(buf), n > 0 ? "%.1f g" : "-", n > 0 ? sum / n : 0.f);
      char label[16];
      snprintf(label, sizeof(label), "avg %s", HIST_RANGES[this->range_].label);
      this->value_line_(it, LINE_Y[1], label, TEXT_DIM, buf);
      fmt_clock(buf, sizeof(buf), this->gecko_known_ ? this->gecko_since_ : 0);
      this->value_line_(it, LINE_Y[2], "since", TEXT_DIM, buf);
    }
    this->draw_graph_(it, GECKO_TRACES, 1, 5.f);
    if (this->vis_(GECKO_LOG))
      this->draw_gecko_log_(it);
  }

  // The latest completed periods, newest first: in / out, from - to, how long, mean weight of a stay.
  void draw_gecko_log_(Display &it) {
    this->fill_(it, Box{8, GECKO_LOG.y + 2, W - 16, 1}, GRID);
    if (this->gecko_log_n_ == 0) {
      this->text_(it, W / 2, GECKO_LOG.y + 50, &id(tiny_font), TEXT_DIM, TextAlign::CENTER,
                  "no visits to the nest logged yet");
      return;
    }
    char from[8], to[8], buf[24];
    for (int i = 0; i < this->gecko_log_n_; i++) {
      const GeckoPeriod &p = this->gecko_log_[i];
      const int y = GECKO_LOG.y + 15 + i * 19;
      this->text_(it, 12, y, &id(tiny_font), p.in ? T_GECKO : TEXT_DIM, TextAlign::CENTER_LEFT, p.in ? "in" : "out");
      fmt_clock(from, sizeof(from), p.start);
      fmt_clock(to, sizeof(to), p.start + p.seconds);
      snprintf(buf, sizeof(buf), "%s-%s", from, to);
      this->text_(it, 50, y, &id(tiny_font), TEXT, TextAlign::CENTER_LEFT, buf);
      fmt_duration(buf, sizeof(buf), p.seconds);
      this->text_(it, 236, y, &id(tiny_font), TEXT, TextAlign::CENTER_RIGHT, buf);
      if (p.in && !std::isnan(p.weight)) {
        snprintf(buf, sizeof(buf), "%.1f g", p.weight);
        this->text_(it, W - 12, y, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_RIGHT, buf);
      }
    }
  }

  static bool alarm_on(Alarm a) {
    switch (a) {
      case Alarm::OVERTEMPERATURE: return id(alarm_overtemperature).state;
      case Alarm::UNDERTEMPERATURE: return id(alarm_undertemperature).state;
      case Alarm::LOW_HUMIDITY: return id(alarm_low_humidity).state;
      default: return false;
    }
  }
  // The raised alarm to show now: one of them in turn, NONE if none is raised
  static Alarm alarm_due_() {
    Alarm on[ALARM_COUNT];
    int n = 0;
    for (int i = 0; i < ALARM_COUNT; i++)
      if (alarm_on(static_cast<Alarm>(i)))
        on[n++] = static_cast<Alarm>(i);
    return n == 0 ? Alarm::NONE : on[(millis() / ALARM_TURN_MS) % n];
  }
  static Page alarm_page(Alarm a) { return a == Alarm::LOW_HUMIDITY ? Page::HYGRO : Page::THERMO; }

  void draw_alarm_(Display &it, Alarm a) {
    // pulsing at 1 Hz, inside the glass panel's rounded corners
    const float p = 0.5f + 0.5f * std::sin(millis() * (2.f * (float) M_PI / 1000.f));
    this->fill_(it, Box{ST_SUN.x + 4, ST_SUN.y + 3, ST_SUN.w - 8, ST_SUN.h - 3},
                Color(130 + (int) (80 * p), 12 + (int) (18 * p), 12 + (int) (10 * p)));
    const int cy = ST_SUN.y + ST_SUN.h / 2 + 1;
    const bool temp = a != Alarm::LOW_HUMIDITY;
    const float cur = temp ? sval(&id(thermostat_current_temperature)) : sval(&id(humidity_current));
    const float set = temp ? sval(&id(thermostat_target_temperature)) : sval(&id(humidity_target));
    const char *label = a == Alarm::OVERTEMPERATURE ? "Too hot" : a == Alarm::UNDERTEMPERATURE ? "Too cold" : "Too dry";
    char buf[32] = "";
    if (!std::isnan(cur) && !std::isnan(set))
      snprintf(buf, sizeof(buf), temp ? "%.1f° / set %.1f°" : "%.0f%% / set %.0f%%", cur, set);
    this->text_(it, ST_SUN.x + 12, cy, &id(icon_small), TEXT, TextAlign::CENTER_LEFT, ICON_ALERT);
    this->text_(it, ST_SUN.x + 40, cy, &id(mid_font), TEXT, TextAlign::CENTER_LEFT, label);
    this->text_(it, ST_SUN.x + ST_SUN.w - 10, cy, &id(tiny_font), TEXT, TextAlign::CENTER_RIGHT, buf);
  }

  // The test sweep the scenic light runs ("Sunrise" / "Sunset"), nullptr if none
  static const char *sweep_test() {
    const std::string &fx = id(scenic_light_effect).state;
    return fx == "Sunrise Color" ? "Sunrise" : fx == "Sunset Color" ? "Sunset" : nullptr;
  }
  // No weather override in force (or its state unknown)
  static bool weather_live() {
    const auto &wx = id(weather_override);
    return !wx.has_state() || wx.state.empty() || wx.state == "(live)";
  }

  void draw_tests_(Display &it) {
    // Highlighted while the sweep runs / while a weather override is in force.
    const std::string &fx = id(scenic_light_effect).state;
    this->draw_button_(it, TESTS_CONTROLS[0], ICON_SUNSET_UP, "Test sunrise",
                       fx == "Sunrise Color" ? ARMED : TILE_BG);
    this->draw_button_(it, TESTS_CONTROLS[1], ICON_SUNSET_DOWN, "Test sunset",
                       fx == "Sunset Color" ? ARMED : TILE_BG);
    const auto &wx = id(weather_override);
    const bool live = weather_live();
    char label[48];
    snprintf(label, sizeof(label), "Weather: %s", !wx.has_state() ? "-" : live ? "live" : wx.state.c_str());
    this->draw_button_(it, TESTS_CONTROLS[2], ICON_PARTLY_CLOUDY, label, live ? TILE_BG : ARMED);
    if (this->vis_(TESTS_INFO)) {
      const char *sweep = sweep_test();
      char buf[48] = "No sweep running";
      if (sweep != nullptr)
        snprintf(buf, sizeof(buf), "%s sweep running", sweep);
      this->text_(it, 12, 182, &id(tiny_font), sweep != nullptr ? TEXT : TEXT_DIM, TextAlign::CENTER_LEFT, buf);
      this->text_(it, 12, 208, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT,
                  "Tap Weather again for the next one");
    }
  }

  void draw_settings_(Display &it) {
    if (this->vis_(SETTINGS_INFO)) {
      auto line = [&](int y, const char *label, const char *value) {
        this->text_(it, 12, y, &id(tiny_font), TEXT_DIM, TextAlign::CENTER_LEFT, label);
        this->text_(it, W - 12, y, &id(tiny_font), TEXT, TextAlign::CENTER_RIGHT, value);
      };
      auto text = [](text_sensor::TextSensor *t) { return t->has_state() && !t->state.empty() ? t->state.c_str() : "-"; };
      line(40, "Wi-Fi", text(&id(net_ssid)));
      line(58, "IP address", text(&id(net_ip)));
      const float dbm = sval(&id(wifisignal));
      char buf[16];
      snprintf(buf, sizeof(buf), std::isnan(dbm) ? "-" : "%.0f dBm", dbm);
      line(76, "Signal", buf);
      line(94, "ESPHome", ESPHOME_VERSION);
    }
    this->draw_slider_label_(it, "Display brightness");
    this->draw_slider_(it, SETTINGS_CONTROLS[0]);
  }

  // ---------------------------------------------------------------- state
  Page page_{Page::OVERVIEW};
  int menu_focus_{0};
  int focus_{0};
  bool editing_{false};
  uint8_t wheel_part_{0};  // 0 = hue, 1 = saturation
  uint32_t last_activity_{0};
  uint32_t armed_until_{0};
  Ctl armed_ctl_{C_NONE};  // the CONFIRM button armed_until_ belongs to
  int range_{HIST_DEFAULT_RANGE};  // time range shown by all graphs (HIST_RANGES)
  uint32_t graph_pushes_{0};

  // locally set values (sliders 0-100, wheel hue/sat) held while HA catches up
  float local_[C_COUNT]{};
  float local_h_{0}, local_s_{0};
  uint32_t hold_until_[C_COUNT]{};
  bool send_pending_[C_COUNT]{};
  uint32_t last_send_[C_COUNT]{};
  float shown_h_{0}, shown_s_{0};  // cursor position currently on screen

  // gecko in / out of its nest, from the controller (see gecko_log())
  struct GeckoPeriod {
    bool in;
    time_t start;  // UTC epoch
    uint32_t seconds;
    float weight;  // mean weight of a stay in the nest
  };
  bool gecko_known_{false};
  bool gecko_in_{false};
  uint32_t gecko_since_{0};  // UTC epoch
  float last_stay_weight_{NAN};
  GeckoPeriod gecko_log_[GECKO_LOG_ROWS]{};  // newest first
  int gecko_log_n_{0};

  // start page animation
  gecko_scene::Scene scene_;
  uint32_t frame_start_{0};
  uint32_t frame_gap_{FRAME_MIN_MS};
  uint32_t frame_count_{0}, frame_cost_sum_{0};
  uint32_t step_us_{0}, sky_us_{0}, overlay_us_{0};  // timing of the last 100 frames
  // test sunrise / sunset on the start page (sweep_check())
  bool show_{false};         // playing, or blending back to the real sky
  bool show_ending_{false};  // the sweep is over: blending back
  bool show_rise_{true};
  uint32_t show_start_{0}, show_end_{0};
  Alarm alarm_shown_{Alarm::NONE};  // in the sun row, as last drawn

  // touch gesture tracking
  bool t_down_{false};
  int t_x0_{0}, t_y0_{0}, t_x_{0}, t_y_{0};
  Ctl drag_{C_NONE};

  // rendering
  bool full_dirty_{true};
  std::vector<Box> dirty_;
  bool partial_{false};
  Box region_;
  Box area_;
  int pass_{0};
};

inline UI ui;

// Samples every graphed value into the history; called every 2 s from gecko-console.yaml. Nothing is
// recorded while Home Assistant is disconnected (the values would be stale), leaving a gap.
inline void sample_history() {
  float v[S_COUNT];
  for (auto &x : v)
    x = NAN;
  if (id(api_connected).state) {
    auto level = [](binary_sensor::BinarySensor *on, sensor::Sensor *bri) {
      if (!on->has_state())
        return NAN;
      if (!on->state)
        return 0.f;
      const float b = sval(bri);
      return std::isnan(b) ? NAN : b / 2.55f;
    };
    v[S_TEMP_NOW] = sval(&id(thermostat_current_temperature));
    v[S_TEMP_TARGET] = sval(&id(thermostat_target_temperature));
    v[S_TEMP_GROUND] = sval(&id(ground_temperature));
    v[S_DAYLIGHT] = level(&id(daylight_on), &id(daylight_brightness));
    v[S_HEAT_LAMP] = level(&id(heat_lamp_on), &id(heat_lamp_brightness));
    v[S_HUM_NOW] = sval(&id(humidity_current));
    v[S_HUM_TARGET] = sval(&id(humidity_target));
    v[S_HUM_CEILING] = sval(&id(ceiling_humidity));
    v[S_HUM_GROUND] = sval(&id(ground_humidity));
    v[S_RESERVOIR] = sval(&id(reservoir_level));
    v[S_WIND] = sval(&id(multifan_percentage));
    v[S_WIND_WEATHER] = sval(&id(weather_windspeed));
    const float g = sval(&id(scale_weight));
    if (g >= GECKO_MIN_G)  // the scale reads ~0 while the gecko is out of its nest
      v[S_WEIGHT] = g;
  }
  history.sample(v);
}

}  // namespace gecko_ui
