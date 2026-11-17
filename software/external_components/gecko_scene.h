// Geckoconsole start page background: a live sky that follows the simulated weather and the sun.
//
// Plain C++ (no ESPHome), so it also builds on a PC for previews. Rendered straight into the display
// stripes through a put(x, y, r, g, b) callback, without a frame buffer of its own: the sky is
// computed per 2x2 pixel block, the particles (rain, snow, lightning) are drawn on top.
//
// Demo-scene tricks, all a few hundred bytes of tables:
//  * sky gradient: one colour per block row, keyed to the sun's elevation (night, twilight, golden
//    hour, day) and greyed by the weather
//  * sun: soft glow (1 / (1 + d^2)) with slowly turning rays (angle from an octant atan, sine table)
//  * moon: disc with its phase, glow; stars: hashed per block, twinkling on the sine table
//  * clouds: three octaves of value noise over a 256 byte permutation table, evaluated once per frame
//    on a coarse 81x61 byte grid (every 4 px) and interpolated per block; bump-shaded towards the
//    sun, scrolled by the wind, cover / darkness / light (day, sunset, night) from the weather;
//    fog: a slower, wider layer on the same grid
//  * rain streaks blended into the sky colour, rain curtains drifting through heavy rain, snow, hail,
//    lightning: strikes light up the clouds around them, a branched bolt, sheet lightning
//  * every WMO code its own look: light / moderate / heavy rain differ in cover, darkness, drops
//  * "glass" panels behind the text: the sky darkened and tinted, rounded corners, a lit top edge
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// The firmware is built for size (-Os); the per-pixel float code here is worth optimising for speed.
#pragma GCC push_options
#pragma GCC optimize("O2")

namespace gecko_scene {

static constexpr int SW = 320, SH = 240;
static constexpr int HORIZON_Y = 196;  // where the sky gradient reaches its horizon colour

enum class Sky : uint8_t { CLEAR, FAIR, PARTLY, OVERCAST, FOG, DRIZZLE, RAIN, SHOWERS, SNOW, STORM };

// WMO weather interpretation code (Open-Meteo) -> what the sky looks like
inline Sky sky_for_wmo(int c) {
  if (c <= 0) return Sky::CLEAR;  // also "unknown" (-1)
  if (c == 1) return Sky::FAIR;
  if (c == 2) return Sky::PARTLY;
  if (c == 3) return Sky::OVERCAST;
  if (c == 45 || c == 48) return Sky::FOG;
  if (c >= 51 && c <= 57) return Sky::DRIZZLE;
  if (c >= 61 && c <= 67) return Sky::RAIN;
  if (c >= 71 && c <= 77) return Sky::SNOW;
  if (c >= 80 && c <= 82) return Sky::SHOWERS;
  if (c == 85 || c == 86) return Sky::SNOW;
  if (c >= 95) return Sky::STORM;
  return Sky::OVERCAST;
}

struct Glass {
  int x, y, w, h;
};

struct Inputs {
  int wmo{-1};
  float sun_elevation{30.f};  // degrees
  float moon{50.f};           // illuminated share, 0-100
};

struct RGB {
  float r, g, b;
};
inline RGB mix(const RGB &a, const RGB &b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float smooth(float e0, float e1, float x) {
  const float t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3.f - 2.f * t);
}

class Scene {
 public:
  Scene() {
    uint32_t s = 0x9E3779B9u;
    for (int i = 0; i < 256; i++)
      perm_[i] = (uint8_t) i;
    for (int i = 255; i > 0; i--) {  // Fisher-Yates
      s = xorshift(s);
      std::swap(perm_[i], perm_[s % (i + 1)]);
    }
    for (int i = 0; i < 256; i++)
      sin_[i] = std::sin(i * 6.2831853f / 256.f);
    for (auto &d : drops_)
      this->spawn_drop_(d, true);
    for (auto &f : flakes_)
      this->spawn_flake_(f, true);
  }

  void set_inputs(const Inputs &in) { this->in_ = in; }

  // Once per frame, before rendering it: advances time, particles and lightning, and derives the
  // per-frame colours.
  void step(uint32_t now_ms) {
    const float dt = this->last_ms_ == 0 ? 0.f : std::min(0.3f, (now_ms - this->last_ms_) / 1000.f);
    this->last_ms_ = now_ms;
    this->t_ += dt;
    this->sky_ = sky_for_wmo(this->in_.wmo);
    this->look_ = look_for_wmo(this->in_.wmo);
    this->derive_colours_();
    this->step_field_();
    this->step_particles_(dt);
    this->step_lightning_(dt);
  }

  Sky sky() const { return this->sky_; }
  bool night() const { return this->in_.sun_elevation < -1.f; }

  // The sky for the pixels in [x0, x0 + w) x [y0, y0 + h), in 2x2 blocks; glass panels darkened.
  template<class Put> void render(int x0, int y0, int w, int h, const Glass *glass, int n_glass, Put &&put) const {
    const int bx0 = std::max(0, x0) >> 1, by0 = std::max(0, y0) >> 1;
    const int bx1 = std::min(SW, x0 + w + 1) >> 1, by1 = std::min(SH, y0 + h + 1) >> 1;
    const float cover_th = 0.78f - 0.72f * this->look_.cover;
    for (int by = by0; by < by1; by++) {
      const int y = by * 2;
      const RGB row = this->rows_[by];
      for (int bx = bx0; bx < bx1; bx++) {
        const int x = bx * 2;
        RGB c = row;

        // stars (behind everything else), hashed per block
        if (this->star_ > 0.f) {
          const uint8_t hsh = this->hash_(bx * 3, by * 5);
          if (hsh < 2 && this->hash_(bx + 101, by + 7) < 96) {
            const float tw = 0.55f + 0.45f * this->sinf_(this->hash_(by, bx) + this->t_ * (30.f + hsh * 20.f));
            const float b = this->star_ * tw * (0.6f + 0.4f * (this->hash_(bx, by + 3) / 255.f));
            c = mix(c, RGB{255, 250, 235}, b);
          }
        }
        // sun: glow, rays when the sky is clear enough to show them
        if (this->sun_vis_ > 0.f) {
          const float dx = x - this->sun_x_, dy = y - this->sun_y_;
          const float d2 = dx * dx + dy * dy;
          if (d2 < 120.f * 120.f) {
            const float core = this->sun_r_ * this->sun_r_;
            float g = core * 2.2f / (d2 + core * 2.2f);
            if (d2 < core)
              g = 1.f;
            if (this->rays_ > 0.f && d2 > core) {
              const float ang = this->angle_(dx, dy);
              const float ray = this->sinf_(ang * 9.f + this->t_ * 6.f) * this->sinf_(ang * 4.f - this->t_ * 3.f);
              g += this->rays_ * std::max(0.f, ray) * (1.f - std::sqrt(d2) / 120.f) * 0.45f;
            }
            c = mix(c, this->sun_col_, clamp01(g * this->sun_vis_));
          }
        }
        // moon: disc with its phase (lit from the right), faint glow
        if (this->moon_vis_ > 0.f) {
          const float dx = x - MOON_X, dy = y - MOON_Y;
          const float d2 = dx * dx + dy * dy;
          constexpr float r = 10.f;
          if (d2 < r * r) {
            const float half = std::sqrt(r * r - dy * dy);
            const bool lit = dx > half * (1.f - 2.f * this->moon_phase_);
            c = mix(c, lit ? RGB{235, 232, 210} : RGB{30, 34, 50}, this->moon_vis_);
          } else if (d2 < 44.f * 44.f) {
            c = mix(c, RGB{120, 135, 170}, this->moon_vis_ * this->moon_phase_ * 90.f / (d2 + 300.f));
          }
        }
        // clouds from the grid: density -> coverage, bump shading towards the sun -> light
        if (this->look_.cover > 0.f) {
          const float n = field_at<GRID>(this->cloud_, x, y);
          const float a = smooth(cover_th, cover_th + 0.2f, n);
          if (a > 0.f) {
            const float lit =
                clamp01(0.5f + (n - field_at<GRID>(this->cloud_, x + this->light_dx_, y + this->light_dy_)) *
                                   this->look_.relief);
            const RGB cloud = mix(this->cloud_lo_, this->cloud_hi_, lit * (0.55f + 0.45f * (1.f - a * 0.5f)));
            c = mix(c, cloud, a * this->look_.opacity);
          }
        }
        // fog: a slow, wide layer; or curtains of heavy rain drifting with the wind
        if (this->look_.fog > 0.f) {
          const float f = field_at<FOG_GRID>(this->fog_, x, y);
          c = mix(c, this->fog_col_, this->look_.fog * (0.4f + 0.6f * f));
        } else if (this->look_.veil > 0.f) {
          const float f = field_at<FOG_GRID>(this->fog_, x, y);
          c = mix(c, this->fog_col_, this->look_.veil * smooth(0.4f, 0.85f, f));
        }
        // lightning: lights up the clouds around the strike, the rest of the sky a little
        if (this->flash_ > 0.f) {
          const float dx = (x - this->flash_x_) * (1.f / 80.f);
          const float near = 1.f / (1.f + dx * dx);
          c = mix(c, RGB{215, 220, 255}, this->flash_ * (0.12f + 0.6f * near * this->flash_reach_));
        }

        // glass panels
        int glass_edge = -1;
        for (int i = 0; i < n_glass; i++) {
          if (in_glass(glass[i], x, y)) {
            glass_edge = (y - glass[i].y) < 2 ? 1 : 0;
            break;
          }
        }
        if (glass_edge >= 0) {
          c = {c.r * 0.40f + 6.f, c.g * 0.40f + 9.f, c.b * 0.40f + 18.f};
          if (glass_edge) {  // lit top edge, subtle on a dark sky
            const float e = 10.f + 0.25f * (c.r + c.g + c.b) / 3.f;
            c = {c.r + e, c.g + e, c.b + e * 1.2f};
          }
        }

        const uint8_t r = (uint8_t) std::min(255.f, c.r), g = (uint8_t) std::min(255.f, c.g),
                      b = (uint8_t) std::min(255.f, c.b);
        for (int py = y; py < y + 2; py++)
          if (py >= y0 && py < y0 + h)
            for (int px = x; px < x + 2; px++)
              if (px >= x0 && px < x0 + w)
                put(px, py, r, g, b);
      }
    }
  }

  // Rain, snow and the lightning bolt over the sky (dimmed on glass), clipped to the given area.
  template<class Put> void render_particles(int x0, int y0, int w, int h, const Glass *glass, int n_glass,
                                            Put &&put) const {
    auto plot = [&](int x, int y, RGB c) {
      if (x < x0 || x >= x0 + w || y < y0 || y >= y0 + h)
        return;
      for (int i = 0; i < n_glass; i++)
        if (in_glass(glass[i], x, y)) {
          c = {c.r * 0.45f, c.g * 0.45f, c.b * 0.5f};
          break;
        }
      put(x, y, (uint8_t) c.r, (uint8_t) c.g, (uint8_t) c.b);
    };
    // rain: streaks blended into the sky behind them, fading towards the tail
    const int nd = this->look_.drops;
    for (int i = 0; i < nd; i++) {
      const Drop &d = this->drops_[i];
      const int x = d.x8 >> 3, y = d.y8 >> 3;
      // the sky behind the drop, lit by a strike like the sky itself
      float lit = 0.f;
      if (this->flash_ > 0.f) {
        const float dx = (x - this->flash_x_) * (1.f / 80.f);
        lit = this->flash_ * (0.12f + 0.6f * this->flash_reach_ / (1.f + dx * dx));
      }
      for (int k = 0; k < d.len; k++) {
        const int py = y + k;
        RGB bg = this->rows_[std::max(0, std::min(SH / 2 - 1, py >> 1))];
        if (lit > 0.f)
          bg = mix(bg, RGB{215, 220, 255}, lit);
        plot(x + (int) (k * this->look_.slant), py, mix(bg, this->drop_col_, this->look_.drop_alpha * (0.3f + 0.7f * k / d.len)));
      }
    }
    // hail: small white pellets, falling fast
    for (int i = nd; i < nd + this->look_.hail; i++) {
      const Drop &d = this->drops_[i];
      const int x = d.x8 >> 3, y = d.y8 >> 3;
      const RGB c{235, 240, 250};
      plot(x, y, c);
      plot(x + 1, y, c);
      plot(x, y + 1, c);
      plot(x + 1, y + 1, c);
    }
    // snow: swaying flakes; grains are single pixels
    const int nf = this->look_.flakes;
    for (int i = 0; i < nf; i++) {
      const Flake &f = this->flakes_[i];
      const int x = (int) (f.x + 5.f * this->sinf_(f.phase + this->t_ * 25.f)), y = (int) f.y;
      const RGB c = this->night() ? RGB{150, 155, 175} : RGB{245, 248, 255};
      plot(x, y, c);
      if (f.big && !this->look_.grains) {
        plot(x + 1, y, c);
        plot(x, y + 1, c);
        plot(x + 1, y + 1, c);
      }
    }
    if (this->bolt_life_ > 0.f) {
      for (int i = 0; i + 1 < this->bolt_n_; i++)
        this->line_(this->bolt_[i][0], this->bolt_[i][1], this->bolt_[i + 1][0], this->bolt_[i + 1][1], plot, true);
      for (int i = 0; i + 1 < this->branch_n_; i++)
        this->line_(this->branch_[i][0], this->branch_[i][1], this->branch_[i + 1][0], this->branch_[i + 1][1], plot,
                    false);
    }
  }

  static bool in_glass(const Glass &g, int x, int y) {
    if (x < g.x || x >= g.x + g.w || y < g.y || y >= g.y + g.h)
      return false;
    constexpr int R = 8;  // rounded corners
    const int cx = x < g.x + R ? g.x + R : (x >= g.x + g.w - R ? g.x + g.w - R - 1 : x);
    const int cy = y < g.y + R ? g.y + R : (y >= g.y + g.h - R ? g.y + g.h - R - 1 : y);
    const int dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= R * R;
  }

 protected:
  static constexpr int MOON_X = 278, MOON_Y = 62;
  static constexpr int MAX_DROPS = 220, MAX_FLAKES = 120;  // drops: rain + hail

  // What a weather looks like. dark: how grey and dim the sky and clouds get.
  struct Look {
    float cover{0}, opacity{1}, dark{0}, fog{0}, veil{0}, wind{1}, slant{0}, drop_alpha{0.6f};
    float relief{7};  // bump shading contrast: towering cumulus high, a flat rain deck low
    uint8_t drops{0}, hail{0}, flakes{0};
    bool grains{false};
    uint8_t len_min{5}, len_span{6};
    uint16_t speed_min{170}, speed_span{100};
    float strike_s{0};  // mean time between lightning strikes, 0 = none
  };
  static Look clouds(float cover, float dark, float wind = 1.f, float opacity = 1.f) {
    Look l;
    l.cover = cover;
    l.dark = dark;
    l.wind = wind;
    l.opacity = opacity;
    l.relief = cover > 0.9f ? 2.5f : 7.f;
    return l;
  }
  // rain on top of clouds: drops, streak length, speed (px/s), how opaque, wind slant, curtains
  static Look rain(Look l, uint8_t drops, uint8_t len_min, uint8_t len_span, uint16_t speed_min, float alpha,
                   float slant, float veil = 0.f) {
    l.drops = drops;
    l.len_min = len_min;
    l.len_span = len_span;
    l.speed_min = speed_min;
    l.speed_span = speed_min / 2;
    l.drop_alpha = alpha;
    l.slant = slant;
    l.veil = veil;
    return l;
  }
  static Look snow(Look l, uint8_t flakes, bool grains = false) {
    l.flakes = flakes;
    l.grains = grains;
    return l;
  }
  static Look storm(Look l, float strike_s, uint8_t hail = 0) {
    l.strike_s = strike_s;
    l.hail = hail;
    l.relief = 5.f;  // dark, but with structure
    return l;
  }
  static Look look_for_wmo(int c) {
    switch (c) {
      case 0: return clouds(0.05f, 0.f, 1.0f, 0.8f);
      case 1: return clouds(0.30f, 0.f, 1.0f, 0.9f);
      case 2: return clouds(0.50f, 0.1f, 1.3f, 0.95f);
      case 3: return clouds(0.97f, 0.35f, 0.8f);
      case 45: case 48: { Look l = clouds(0.6f, 0.2f, 0.4f, 0.7f); l.fog = 0.85f; return l; }
      // drizzle: light / moderate / dense (56, 57 freezing)
      case 51: return rain(clouds(0.75f, 0.30f), 35, 4, 3, 120, 0.55f, 0.10f);
      case 53: case 56: return rain(clouds(0.82f, 0.38f), 60, 4, 4, 130, 0.60f, 0.10f);
      case 55: case 57: return rain(clouds(0.88f, 0.45f), 90, 5, 4, 140, 0.62f, 0.12f, 0.08f);
      // rain: light / moderate / heavy (66, 67 freezing)
      case 61: case 66: return rain(clouds(0.85f, 0.45f, 1.2f), 55, 5, 6, 180, 0.65f, 0.18f);
      case 63: return rain(clouds(0.95f, 0.60f, 1.4f), 105, 6, 8, 220, 0.55f, 0.22f, 0.12f);
      case 65: case 67: return rain(clouds(1.0f, 0.75f, 1.8f), 170, 9, 9, 280, 0.60f, 0.30f, 0.30f);
      // snow: light / moderate / heavy / grains; snow showers
      case 71: return snow(clouds(0.80f, 0.25f, 0.6f), 40);
      case 73: return snow(clouds(0.88f, 0.32f, 0.6f), 75);
      case 75: return snow(clouds(0.97f, 0.42f, 0.8f), 120);
      case 77: return snow(clouds(0.85f, 0.30f, 0.6f), 90, true);
      case 85: return snow(clouds(0.65f, 0.30f, 1.2f), 50);
      case 86: return snow(clouds(0.75f, 0.40f, 1.4f), 100);
      // showers: slight / moderate / violent -- broken cloud, gusty
      case 80: return rain(clouds(0.60f, 0.40f, 1.8f), 50, 5, 6, 190, 0.50f, 0.25f);
      case 81: return rain(clouds(0.70f, 0.52f, 2.0f), 95, 6, 8, 230, 0.55f, 0.30f, 0.10f);
      case 82: return rain(clouds(0.82f, 0.70f, 2.4f), 160, 9, 9, 290, 0.60f, 0.38f, 0.28f);
      // thunderstorm; with hail
      case 95: return storm(rain(clouds(1.0f, 0.90f, 2.2f), 140, 8, 9, 270, 0.55f, 0.32f, 0.20f), 5.f);
      case 96: return storm(rain(clouds(1.0f, 0.92f, 2.4f), 150, 8, 9, 280, 0.55f, 0.34f, 0.22f), 3.5f, 20);
      case 99: return storm(rain(clouds(1.0f, 0.95f, 2.8f), 170, 9, 10, 300, 0.60f, 0.40f, 0.30f), 2.f, 40);
      default: break;
    }
    if (c < 0)
      return clouds(0.05f, 0.f, 1.0f, 0.8f);  // unknown: a clear sky
    return clouds(0.97f, 0.35f, 0.8f);         // unmapped: overcast
  }

  // Sky colours by sun elevation: {elevation, zenith, horizon}
  struct Key {
    float elev;
    RGB top, hor;
  };
  static constexpr int N_KEYS = 6;
  static const Key *keys() {
    static const Key k[N_KEYS] = {
        {-12.f, {3, 5, 18}, {10, 14, 36}},     {-6.f, {10, 12, 42}, {45, 32, 78}},
        {0.f, {38, 40, 104}, {235, 110, 70}},  {6.f, {60, 98, 182}, {252, 178, 110}},
        {15.f, {40, 100, 200}, {150, 196, 240}}, {30.f, {24, 84, 196}, {140, 190, 240}},
    };
    return k;
  }

  // Density grids, 0..255: clouds (3 octaves) every GRID px, fog (1 octave, smoother) every FOG_GRID px
  static constexpr int GRID = 4, FOG_GRID = 8;
  template<int G> struct Grid {
    uint8_t v[SH / G + 2][SW / G + 2];
  };
  void step_field_() {
    const float wind = this->look_.wind, t = this->t_;
    if (this->look_.cover > 0.f) {
      for (int gy = 0; gy < SH / GRID + 2; gy++) {
        const float y = gy * GRID;
        for (int gx = 0; gx < SW / GRID + 2; gx++) {
          const float x = gx * GRID;
          float n = 0.55f * this->vnoise_(x / 58.f + t * 0.045f * wind, y / 30.f) +
                    0.30f * this->vnoise_(x / 24.f + t * 0.09f * wind + 17.3f, y / 13.f + 5.1f) +
                    0.15f * this->vnoise_(x / 10.f + t * 0.16f * wind + 3.7f, y / 6.f + 11.9f);
          n *= 1.08f - 0.25f * (y / SH);  // a little thinner towards the horizon
          this->cloud_.v[gy][gx] = (uint8_t) (clamp01(n) * 255.f);
        }
      }
    }
    if (this->look_.fog > 0.f) {
      for (int gy = 0; gy < SH / FOG_GRID + 2; gy++)
        for (int gx = 0; gx < SW / FOG_GRID + 2; gx++)
          this->fog_.v[gy][gx] =
              (uint8_t) (this->vnoise_(gx * FOG_GRID / 70.f + t * 0.03f, gy * FOG_GRID / 18.f + 40.f) * 255.f);
    } else if (this->look_.veil > 0.f) {
      // rain curtains: narrow, tall, slanted like the rain, sweeping along with the wind
      for (int gy = 0; gy < SH / FOG_GRID + 2; gy++)
        for (int gx = 0; gx < SW / FOG_GRID + 2; gx++) {
          const float x = gx * FOG_GRID - gy * FOG_GRID * this->look_.slant, y = gy * FOG_GRID;
          this->fog_.v[gy][gx] = (uint8_t) (this->vnoise_(x / 22.f - t * 0.6f * wind, y / 90.f + t * 0.15f) * 255.f);
        }
    }
    // bump shading: compare with the density one grid step towards the sun (up and right by day)
    const float sx = this->sun_x_ - SW / 2.f, sy = this->sun_y_ - SH;
    const float len = std::sqrt(sx * sx + sy * sy) + 1e-3f;
    this->light_dx_ = (int) std::lround(sx / len * GRID * 1.5f);
    this->light_dy_ = (int) std::lround(sy / len * GRID * 1.5f);
  }
  // bilinear lookup in a grid, 0..1
  template<int G> static float field_at(const Grid<G> &f, int x, int y) {
    x = std::max(0, std::min(SW + G - 1, x));
    y = std::max(0, std::min(SH + G - 1, y));
    const int gx = x / G, gy = y / G;
    const float fx = (x % G) * (1.f / G), fy = (y % G) * (1.f / G);
    const float a = f.v[gy][gx], b = f.v[gy][gx + 1], c = f.v[gy + 1][gx], d = f.v[gy + 1][gx + 1];
    const float top = a + (b - a) * fx, bot = c + (d - c) * fx;
    return (top + (bot - top) * fy) * (1.f / 255.f);
  }

  void derive_colours_() {
    const float e = this->in_.sun_elevation;
    const Key *k = keys();
    RGB top = k[0].top, hor = k[0].hor;
    if (e >= k[N_KEYS - 1].elev) {
      top = k[N_KEYS - 1].top;
      hor = k[N_KEYS - 1].hor;
    } else {
      for (int i = 0; i + 1 < N_KEYS; i++) {
        if (e >= k[i].elev && e < k[i + 1].elev) {
          const float t = (e - k[i].elev) / (k[i + 1].elev - k[i].elev);
          top = mix(k[i].top, k[i + 1].top, t);
          hor = mix(k[i].hor, k[i + 1].hor, t);
        }
      }
    }
    // bad weather: greyer and darker
    const float dark = this->look_.dark;
    auto grey = [dark](RGB c) {
      const float l = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
      const RGB g{l, l, l + 6.f};
      RGB m = mix(c, g, 0.25f + 0.65f * dark);
      const float f = 1.f - 0.72f * dark;
      return RGB{m.r * f, m.g * f, m.b * f};
    };
    if (this->look_.cover > 0.6f) {
      top = grey(top);
      hor = grey(hor);
    }
    for (int by = 0; by < SH / 2; by++) {
      const float t = clamp01(by * 2.f / HORIZON_Y);
      this->rows_[by] = mix(top, hor, t * t);
    }

    // day light share: 0 at night, 1 from ~8 degrees
    const float day = clamp01((e + 4.f) / 12.f);
    const float golden = clamp01(1.f - std::fabs(e - 2.f) / 9.f);  // around sunrise / sunset
    this->star_ = clamp01((-e - 1.f) / 6.f) * (1.f - clamp01((this->look_.cover - 0.5f) * 2.f));

    // clouds: lit by day, glowing at sunset, dim at night; heavier = darker
    RGB hi = mix(RGB{72, 78, 104}, RGB{246, 246, 250}, day);
    RGB lo = mix(RGB{16, 18, 28}, RGB{150, 156, 172}, day);
    hi = mix(hi, RGB{255, 178, 140}, golden * 0.75f);
    lo = mix(lo, RGB{110, 70, 92}, golden * 0.6f);
    const float d = 1.f - 0.72f * dark;
    this->cloud_hi_ = {hi.r * d, hi.g * d, hi.b * d * 1.04f};
    this->cloud_lo_ = {lo.r * d * 0.8f, lo.g * d * 0.82f, lo.b * d * 0.95f};
    // fog; rain curtains: a shade lighter than the clouds they hang from
    this->fog_col_ = this->look_.fog > 0.f ? mix(RGB{38, 42, 54}, RGB{176, 182, 190}, day)
                                           : RGB{this->cloud_hi_.r * 1.1f + 12.f, this->cloud_hi_.g * 1.1f + 12.f,
                                                 this->cloud_hi_.b * 1.1f + 16.f};
    this->drop_col_ = mix(RGB{130, 140, 170}, RGB{225, 235, 250}, day);

    // sun: from the horizon up the right-hand side, orange when low
    this->sun_vis_ = clamp01((e + 3.f) / 3.f) * (1.f - 0.85f * clamp01((this->look_.cover - 0.55f) * 2.5f));
    this->sun_x_ = 278.f;
    this->sun_y_ = 172.f - clamp01((e + 3.f) / 60.f) * 136.f;
    this->sun_r_ = 9.f + 4.f * golden;
    this->sun_col_ = mix(RGB{255, 248, 215}, RGB{255, 150, 64}, golden);
    this->rays_ = this->look_.cover < 0.4f ? (1.f - golden * 0.5f) : 0.f;
    // moon: at night, phase from its illumination
    this->moon_phase_ = clamp01(this->in_.moon / 100.f);
    this->moon_vis_ = this->moon_phase_ > 0.03f ? clamp01((-e - 1.f) / 4.f) * (1.f - 0.8f * clamp01(this->look_.cover - 0.5f) * 2.f) : 0.f;
  }

  // positions in 1/8 px, speed in 2 px/s: 6 bytes a drop
  struct Drop {
    int16_t x8, y8;
    uint8_t v2, len;
  };
  struct Flake {
    float x, y, v;
    uint8_t phase;
    bool big;
  };

  void spawn_drop_(Drop &d, bool anywhere) {
    this->rng_ = xorshift(this->rng_);
    d.x8 = (int16_t) (((int) (this->rng_ % (SW + 80)) - 80) * 8);
    d.y8 = (int16_t) ((anywhere ? (int) (this->rng_ / 7 % SH) : -(int) (this->rng_ / 7 % 40)) * 8);
    const uint32_t v = this->look_.speed_min + this->rng_ / 13 % (this->look_.speed_span + 1);
    d.v2 = (uint8_t) std::min<uint32_t>(255, v / 2);
    d.len = (uint8_t) (this->look_.len_min + this->rng_ / 17 % (this->look_.len_span + 1));
  }
  void spawn_flake_(Flake &f, bool anywhere) {
    this->rng_ = xorshift(this->rng_);
    f.x = (float) (this->rng_ % SW);
    f.y = anywhere ? (float) (this->rng_ / 7 % SH) : -4.f;
    f.v = 14.f + (this->rng_ / 13 % 26);
    f.phase = (uint8_t) (this->rng_ >> 24);
    f.big = (this->rng_ & 3) != 0;
  }
  void step_particles_(float dt) {
    const int n = std::min(MAX_DROPS, this->look_.drops + this->look_.hail);
    for (int i = 0; i < n; i++) {
      Drop &d = this->drops_[i];
      const float v8 = d.v2 * 16.f * dt * (i >= this->look_.drops ? 1.4f : 1.f);  // hail falls faster
      d.y8 = (int16_t) (d.y8 + (int) v8);
      d.x8 = (int16_t) (d.x8 + (int) (v8 * this->look_.slant));
      if ((d.y8 >> 3) > SH || (d.x8 >> 3) > SW)
        this->spawn_drop_(d, false);
    }
    for (int i = 0; i < this->look_.flakes; i++) {
      Flake &f = this->flakes_[i];
      f.y += f.v * dt;
      f.x += 6.f * dt * this->look_.wind;
      if (f.y > SH)
        this->spawn_flake_(f, false);
      if (f.x > SW)
        f.x -= SW;
    }
  }

  void step_lightning_(float dt) {
    this->flash_ = std::max(0.f, this->flash_ - dt * 3.5f);
    this->bolt_life_ = std::max(0.f, this->bolt_life_ - dt);
    if (this->restrike_ > 0.f) {  // the flicker of a multi-stroke flash
      this->restrike_ -= dt;
      if (this->restrike_ <= 0.f) {
        this->flash_ = 0.85f;
        this->bolt_life_ = std::max(this->bolt_life_, 0.1f);
      }
    }
    if (this->look_.strike_s <= 0.f || dt <= 0.f)
      return;
    this->rng_ = xorshift(this->rng_);
    if ((this->rng_ % 10000) >= (uint32_t) (dt / this->look_.strike_s * 10000.f))
      return;
    this->rng_ = xorshift(this->rng_);
    this->flash_ = 1.f;
    this->flash_x_ = 30.f + (float) (this->rng_ % (SW - 60));
    this->restrike_ = (this->rng_ & 1) ? 0.08f + (this->rng_ / 3 % 80) / 1000.f : 0.f;
    if ((this->rng_ / 5) % 10 < 4) {  // sheet lightning: the clouds light up, no bolt
      this->flash_reach_ = 1.8f;
      this->flash_ = 0.7f;
      this->bolt_n_ = this->branch_n_ = 0;
      return;
    }
    this->flash_reach_ = 1.f;
    this->bolt_life_ = 0.22f;
    int x = (int) this->flash_x_, y = 0;
    this->bolt_n_ = 0;
    while (y < 160 && this->bolt_n_ < MAX_BOLT) {
      this->bolt_[this->bolt_n_][0] = (int16_t) x;
      this->bolt_[this->bolt_n_][1] = (int16_t) y;
      this->bolt_n_++;
      this->rng_ = xorshift(this->rng_);
      x += (int) (this->rng_ % 27) - 13;
      y += 10 + (int) (this->rng_ / 31 % 12);
    }
    // a branch from somewhere along the upper half, off to one side
    this->branch_n_ = 0;
    if (this->bolt_n_ > 4) {
      const int from = 1 + (int) (this->rng_ / 7 % (this->bolt_n_ / 2));
      int bx = this->bolt_[from][0], by = this->bolt_[from][1];
      const int dir = (this->rng_ & 2) ? 1 : -1;
      while (this->branch_n_ < MAX_BRANCH) {
        this->branch_[this->branch_n_][0] = (int16_t) bx;
        this->branch_[this->branch_n_][1] = (int16_t) by;
        this->branch_n_++;
        this->rng_ = xorshift(this->rng_);
        bx += dir * (6 + (int) (this->rng_ % 10));
        by += 6 + (int) (this->rng_ / 11 % 10);
      }
    }
  }

  // a bolt segment: white core, blue glow; the main channel thicker than a branch
  template<class Plot> void line_(int x0, int y0, int x1, int y1, Plot &plot, bool main) const {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    const RGB core{252, 252, 255}, halo{140, 150, 255};
    while (true) {
      plot(x0, y0, core);
      if (main) {
        plot(x0 + 1, y0, core);
        plot(x0 - 1, y0, halo);
        plot(x0 + 2, y0, halo);
      } else {
        plot(x0 + 1, y0, halo);
      }
      if (x0 == x1 && y0 == y1)
        break;
      const int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  static uint32_t xorshift(uint32_t s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  uint8_t hash_(int x, int y) const { return this->perm_[(this->perm_[x & 255] + y) & 255]; }
  // value noise in 0..1, smoothstep interpolation between hashed lattice points
  float vnoise_(float x, float y) const {
    x += 4096.f;
    y += 4096.f;
    const int ix = (int) x, iy = (int) y;
    const float fx = x - ix, fy = y - iy;
    const float sx = fx * fx * (3.f - 2.f * fx), sy = fy * fy * (3.f - 2.f * fy);
    const float a = this->hash_(ix, iy), b = this->hash_(ix + 1, iy);
    const float c = this->hash_(ix, iy + 1), d = this->hash_(ix + 1, iy + 1);
    const float top = a + (b - a) * sx, bot = c + (d - c) * sx;
    return (top + (bot - top) * sy) * (1.f / 255.f);
  }
  // sine over a 256 step circle
  float sinf_(float phase) const { return this->sin_[((int) phase) & 255]; }
  // angle of (dx, dy) on the same 256 step circle, octant approximation (a few degrees off)
  static float angle_(float dx, float dy) {
    const float ax = std::fabs(dx), ay = std::fabs(dy);
    const float a = (ax > ay ? ay / (ax + 1e-6f) : 2.f - ax / (ay + 1e-6f)) * 32.f;  // 0..64 per quadrant
    if (dx >= 0.f)
      return dy >= 0.f ? a : 256.f - a;
    return dy >= 0.f ? 128.f - a : 128.f + a;
  }

  Inputs in_;
  Sky sky_{Sky::CLEAR};
  Look look_{look_for_wmo(-1)};
  uint8_t perm_[256];
  float sin_[256];
  RGB rows_[SH / 2];
  Grid<GRID> cloud_{};
  Grid<FOG_GRID> fog_{};
  int light_dx_{4}, light_dy_{-4};
  RGB cloud_hi_{}, cloud_lo_{}, fog_col_{}, drop_col_{}, sun_col_{};
  float star_{0}, sun_vis_{0}, sun_x_{0}, sun_y_{0}, sun_r_{9}, rays_{0}, moon_vis_{0}, moon_phase_{0};
  float t_{0}, flash_{0}, flash_x_{160}, flash_reach_{1}, restrike_{0}, bolt_life_{0};
  uint32_t last_ms_{0};
  uint32_t rng_{0x12345678u};
  Drop drops_[MAX_DROPS];
  Flake flakes_[MAX_FLAKES];
  static constexpr int MAX_BOLT = 18, MAX_BRANCH = 6;
  int16_t bolt_[MAX_BOLT][2]{};
  int16_t branch_[MAX_BRANCH][2]{};
  int bolt_n_{0}, branch_n_{0};
};

}  // namespace gecko_scene

#pragma GCC pop_options
