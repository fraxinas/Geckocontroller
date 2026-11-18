#pragma once

// Climate alarms: the terrarium too hot, too cold or too dry for a while, whatever the cause -- a
// blown heat lamp or daylight lamp, a clogged rain pipe, an empty reservoir, a lamp left on by hand
// -- and whether the automations are on or not. It only looks at the outcome: the thermostat's and
// the hygrostat's current value against their setpoint.
//
// Each alarm is a binary sensor of device class "problem" (Home Assistant: "Problem" / "OK"), for
// the Geckoconsole and for automations such as an instant message. An alarm is raised once its value
// has been out of range for `delay` without a break, and cleared once it has been back inside the
// limit by the hysteresis for `clear_delay`, so a value hovering at the limit does not keep raising
// it. A missing reading (sensor failure) neither raises nor clears an alarm.

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace geckoalarm {

enum class Alarm : uint8_t {
  OVERTEMPERATURE,   // temperature above the setpoint by more than temperature_deviation
  UNDERTEMPERATURE,  // temperature below the setpoint by more than temperature_deviation
  LOW_HUMIDITY,      // humidity below the setpoint by more than humidity_deficit
  COUNT,
};

class GeckoAlarm : public Component {
 public:
  void set_thermostat(climate::Climate *c) { this->thermostat_ = c; }
  void set_hygrostat(climate::Climate *c) { this->hygrostat_ = c; }
  void set_temperature_deviation(float k) { this->temp_deviation_ = k; }
  void set_humidity_deficit(float rh) { this->hum_deficit_ = rh; }
  void set_temperature_hysteresis(float k) { this->temp_hysteresis_ = k; }
  void set_humidity_hysteresis(float rh) { this->hum_hysteresis_ = rh; }
  void set_delay(uint32_t ms) { this->delay_ms_ = ms; }
  void set_clear_delay(uint32_t ms) { this->clear_delay_ms_ = ms; }
  void set_sensor(Alarm a, binary_sensor::BinarySensor *s) { this->state_[idx(a)].sensor = s; }

  bool active(Alarm a) const { return this->state_[idx(a)].on; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  static constexpr size_t N = static_cast<size_t>(Alarm::COUNT);
  static constexpr size_t idx(Alarm a) { return static_cast<size_t>(a); }

  struct State {
    binary_sensor::BinarySensor *sensor{nullptr};
    bool on{false};
    bool timing{false};  // the condition that changes it (out of range / back) holds since `since`
    uint32_t since{0};
  };

  void check_();
  // `excess`: how far the value is beyond its setpoint in the alarm's direction (NAN = unknown)
  void update_(Alarm a, float excess, float limit, float hysteresis, float value, float setpoint);

  climate::Climate *thermostat_{nullptr};
  climate::Climate *hygrostat_{nullptr};
  float temp_deviation_{4.0f};
  float hum_deficit_{10.0f};
  float temp_hysteresis_{0.5f};
  float hum_hysteresis_{2.0f};
  uint32_t delay_ms_{600000};
  uint32_t clear_delay_ms_{120000};
  State state_[N];
};

}  // namespace geckoalarm
}  // namespace esphome
