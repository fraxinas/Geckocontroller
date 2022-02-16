#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace terralight {

class TerraLightNumber : public Component, public number::Number {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void control(float value) override;
};

class TerraLight : public Component, public sensor::Sensor {
 public:
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void set_attribute(const std::string &attribute) { attribute_ = attribute; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_twilight_attenuation_number(TerraLightNumber *twilight_attenuation_);

 protected:
  std::string entity_id_;
  optional<std::string> attribute_;
  bool next_update_{true};
  TerraLightNumber *twilight_attenuation_;
};

}  // namespace terralight
}  // namespace esphome
