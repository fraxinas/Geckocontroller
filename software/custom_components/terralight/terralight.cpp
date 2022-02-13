#include "terralight.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace terralight {

static const char *const TAG = "terralight";

void TerraLight::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TerraLight...");
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, this->attribute_, [this](const std::string &state) {
        auto val = parse_number<float>(state);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", state.c_str());
          this->publish_state(NAN);
          return;
        }

        if (this->attribute_.has_value()) {
          ESP_LOGD(TAG, "'%s::%s': Got attribute state %.2f", this->entity_id_.c_str(),
                   this->attribute_.value().c_str(), *val);
        } else {
          ESP_LOGD(TAG, "'%s': Got state %.2f", this->entity_id_.c_str(), *val);
        }
        this->publish_state(*val);
      });
}

void TerraLight::dump_config() {
  ESP_LOGCONFIG(TAG, "TerraLight '%s':", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, " Sensor Entity ID: '%s'", this->entity_id_.c_str());
  if (this->attribute_.has_value()) {
    ESP_LOGCONFIG(TAG, " Sensor Attribute: '%s'", this->attribute_.value().c_str());
  }
}

void TerraLight::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;
}

float TerraLight::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace multi
}  // namespace esphome
