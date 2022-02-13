#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace terralight {

class TerraLight : public Component, public sensor::Sensor {
 public:
//  TerraLight() {}
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void set_attribute(const std::string &attribute) { attribute_ = attribute; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  std::string entity_id_;
  optional<std::string> attribute_;
  bool next_update_{true};
};

}  // namespace terralight
}  // namespace esphome
