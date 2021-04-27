// Geckocontrol

#pragma once

#define SUN_LUT_LEN 4

namespace esphome {
namespace light {
static uint8_t sun_lut[SUN_LUT_LEN][4] = {{5, 5, 5, 50}, {50, 5, 5, 150}, {100, 150, 5, 150}, {200, 200, 200, 200}};
}  // namespace light
}  // namespace esphome
