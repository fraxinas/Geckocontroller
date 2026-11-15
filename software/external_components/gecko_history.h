// Geckoconsole value history: every graphed series at several time ranges at once, so the graphs
// can switch range instantly and each range is already filled.
//
// Each range keeps one value per graph pixel: the mean of all samples taken during that pixel's
// time span (for a 0/1 or duty-cycle series that is the share of time it was on). Values are
// stored as int16 in fixed units (`scale` per series) to keep the whole store at ~16 KB.
//
// Included by gecko-console.yaml via `esphome: includes:` (before gecko_console.h).
#pragma once

#include "esphome/core/hal.h"
#include <cmath>
#include <cstdint>

namespace gecko_ui {

static constexpr int HIST_LEN = 158;  // graph width in pixels

struct HistRange {
  const char *label;
  uint32_t duration_ms;
  uint32_t grid_ms;  // vertical grid line spacing
};
static constexpr HistRange HIST_RANGES[] = {
    {"15 min", 15UL * 60 * 1000, 5UL * 60 * 1000},
    {"2 h", 2UL * 3600 * 1000, 30UL * 60 * 1000},
    {"12 h", 12UL * 3600 * 1000, 2UL * 3600 * 1000},
    {"48 h", 48UL * 3600 * 1000, 12UL * 3600 * 1000},
};
static constexpr int HIST_RANGE_COUNT = sizeof(HIST_RANGES) / sizeof(HIST_RANGES[0]);
static constexpr int HIST_DEFAULT_RANGE = 2;  // 12 h

template<int S> class History {
 public:
  static constexpr int16_t NO_VALUE = INT16_MIN;

  History() {
    for (int s = 0; s < S; s++) {
      this->scale_[s] = 10.f;
      for (int r = 0; r < HIST_RANGE_COUNT; r++)
        for (int i = 0; i < HIST_LEN; i++)
          this->data_[s][r][i] = NO_VALUE;
    }
  }

  // Stored resolution of a series: value * scale must fit in int16 (default 10 = 0.1 units).
  void set_scale(int s, float scale) { this->scale_[s] = scale; }

  // One sample of every series (NAN = no value); call every few seconds.
  void sample(const float *values) {
    const uint32_t now = esphome::millis();
    if (!this->started_) {
      this->started_ = true;
      for (int r = 0; r < HIST_RANGE_COUNT; r++)
        this->next_[r] = now + px_ms(r);
    }
    for (int r = 0; r < HIST_RANGE_COUNT; r++) {
      for (int s = 0; s < S; s++) {
        this->samples_[s][r]++;
        if (!std::isnan(values[s])) {
          this->sum_[s][r] += values[s];
          this->count_[s][r]++;
        }
      }
      // Close every pixel whose time span is over. A pixel shorter than the sampling interval gets
      // no sample of its own and repeats the previous value instead of leaving a gap.
      while ((int32_t) (now - this->next_[r]) >= 0) {
        for (int s = 0; s < S; s++) {
          int16_t v;
          if (this->samples_[s][r] == 0)
            v = this->data_[s][r][(this->head_[r] + HIST_LEN - 1) % HIST_LEN];
          else if (this->count_[s][r] == 0)
            v = NO_VALUE;
          else
            v = encode_(this->sum_[s][r] / this->count_[s][r], this->scale_[s]);
          this->data_[s][r][this->head_[r]] = v;
          this->sum_[s][r] = 0.f;
          this->count_[s][r] = 0;
          this->samples_[s][r] = 0;
        }
        this->head_[r] = (this->head_[r] + 1) % HIST_LEN;
        this->pushes_[r]++;
        this->next_[r] += px_ms(r);
      }
    }
  }

  // Value of series s in range r at graph column x (0 = oldest, HIST_LEN - 1 = newest), NAN if none.
  float get(int s, int r, int x) const {
    const int16_t v = this->data_[s][r][(this->head_[r] + x) % HIST_LEN];
    return v == NO_VALUE ? NAN : v / this->scale_[s];
  }

  // Counts the pixels completed in range r, to redraw a graph only when it has moved.
  uint32_t pushes(int r) const { return this->pushes_[r]; }

  static uint32_t px_ms(int r) { return HIST_RANGES[r].duration_ms / HIST_LEN; }

 protected:
  static int16_t encode_(float v, float scale) {
    const float x = std::round(v * scale);
    if (x >= 32767.f)
      return 32767;
    if (x <= -32767.f)
      return -32767;
    return (int16_t) x;
  }

  int16_t data_[S][HIST_RANGE_COUNT][HIST_LEN];
  float scale_[S];
  float sum_[S][HIST_RANGE_COUNT]{};
  uint16_t count_[S][HIST_RANGE_COUNT]{};    // valid samples in the open pixel
  uint16_t samples_[S][HIST_RANGE_COUNT]{};  // all samples (incl. NAN) in the open pixel
  int head_[HIST_RANGE_COUNT]{};             // next pixel to write = oldest pixel
  uint32_t next_[HIST_RANGE_COUNT]{};        // end of the open pixel (millis)
  uint32_t pushes_[HIST_RANGE_COUNT]{};
  bool started_{false};
};

}  // namespace gecko_ui
