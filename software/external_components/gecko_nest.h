// Gecko in / out of its nest, from the scale underneath it. Included by gecko.yaml via
// `esphome: includes:`; tick() runs every second from an interval there.
//
// A change between in and out counts once it has held for DEBOUNCE_MS (the gecko crossing the
// nest, readings while it climbs in or out) and is dated back to when it began. The current state,
// since when, and the last LOG_LEN completed periods are kept in flash, so they survive a reboot.
//
// Published:
//  * "Gecko in nest" (binary, occupancy) -- in Home Assistant's logbook
//  * "Gecko nest Since" (timestamp) -- start of the current period
//  * "Gecko Stay Weight" (g) -- mean weight of a stay in the nest, when the gecko leaves
//  * "Gecko nest Log" -- compact state + log for the Geckoconsole:
//      "<in>,<since>;<in>,<start>,<seconds>,<weight x10>;..."   newest period first, UTC epochs
#pragma once

#include "esphome.h"
#include <cmath>
#include <string>

namespace gecko_nest {

using namespace esphome;

static const char *const TAG = "gecko_nest";

static constexpr float MIN_G = 5.f;            // reading that counts as "in the nest"
static constexpr uint32_t DEBOUNCE_MS = 60000;  // a change must hold this long to count
static constexpr uint32_t SAMPLE_MS = 10000;    // stay weight sampling
static constexpr int LOG_LEN = 5;

struct Period {
  uint32_t start;     // UTC epoch
  uint32_t seconds;
  uint16_t weight10;  // mean weight x10 of a stay, 0 = none / out
  uint8_t in;
  uint8_t reserved;
};

struct Saved {
  uint32_t since;  // UTC epoch, start of the current period
  uint8_t valid;
  uint8_t in;
  uint8_t n;     // log entries in use
  uint8_t head;  // next log slot to write
  Period log[LOG_LEN];
};

class nest {
 public:
  void setup() {
    this->pref_ = global_preferences->make_preference<Saved>(fnv1_hash("gecko_nest"), true);
    if (!this->pref_.load(&this->s_))
      this->s_ = Saved{};
    if (this->s_.valid)
      this->publish_();
  }

  void tick() {
    const float g = id(gecko_scale).state;
    const ESPTime t = id(esptime).now();
    if (std::isnan(g) || !t.is_valid())
      return;  // wait for a reading and the clock
    const uint32_t now = t.timestamp;
    const uint32_t ms = millis();
    const bool in = g >= MIN_G;

    if (!this->s_.valid) {  // first ever reading
      this->s_ = Saved{};
      this->s_.valid = 1;
      this->s_.in = in;
      this->s_.since = now;
      this->save_();
      return;
    }

    if (in == this->s_.in) {
      this->pending_ = false;
      if (in && ms - this->last_sample_ >= SAMPLE_MS) {
        this->last_sample_ = ms;
        this->sum_ += g;
        this->samples_++;
      }
      return;
    }
    if (!this->pending_) {
      this->pending_ = true;
      this->pending_ms_ = ms;
      return;
    }
    if (ms - this->pending_ms_ < DEBOUNCE_MS)
      return;

    // The change has held: close the current period, dated back to when the change began.
    const uint32_t event = now - (ms - this->pending_ms_) / 1000;
    const float mean = this->samples_ > 0 ? this->sum_ / this->samples_ : NAN;
    Period &p = this->s_.log[this->s_.head];
    p.in = this->s_.in;
    p.start = this->s_.since;
    p.seconds = event > this->s_.since ? event - this->s_.since : 0;
    p.weight10 = p.in && !std::isnan(mean) ? (uint16_t) std::lround(mean * 10.f) : 0;
    p.reserved = 0;
    this->s_.head = (this->s_.head + 1) % LOG_LEN;
    if (this->s_.n < LOG_LEN)
      this->s_.n++;
    if (p.in && !std::isnan(mean))
      id(gecko_stay_weight).publish_state(mean);

    this->s_.in = in;
    this->s_.since = event;
    this->pending_ = false;
    this->sum_ = 0.f;
    this->samples_ = 0;
    ESP_LOGI(TAG, "gecko %s the nest (after %u min)", in ? "entered" : "left", (unsigned) (p.seconds / 60));
    this->save_();
  }

 protected:
  void save_() {
    this->pref_.save(&this->s_);
    this->publish_();
  }

  void publish_() {
    id(gecko_in_nest).publish_state(this->s_.in);
    id(gecko_nest_since).publish_state(ESPTime::from_epoch_utc(this->s_.since).strftime("%Y-%m-%dT%H:%M:%S+00:00"));
    std::string log = str_sprintf("%u,%u", (unsigned) this->s_.in, (unsigned) this->s_.since);
    for (int i = 0; i < this->s_.n; i++) {
      const Period &p = this->s_.log[(this->s_.head + LOG_LEN - 1 - i) % LOG_LEN];
      log += str_sprintf(";%u,%u,%u,%u", (unsigned) p.in, (unsigned) p.start, (unsigned) p.seconds,
                         (unsigned) p.weight10);
    }
    id(gecko_nest_log).publish_state(log);
  }

  ESPPreferenceObject pref_;
  Saved s_{};
  bool pending_{false};
  uint32_t pending_ms_{0};
  uint32_t last_sample_{0};
  float sum_{0.f};
  uint32_t samples_{0};
};

inline nest nest;

}  // namespace gecko_nest
