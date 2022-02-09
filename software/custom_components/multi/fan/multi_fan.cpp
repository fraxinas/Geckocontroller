#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace multi {

static const char *const TAG = "multi.fan";

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MultiFan...");
    this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}

void MultiFan::dump_config() {
  ESP_LOGCONFIG(TAG, "MultiFan '%s':", this->fan_->get_name().c_str());
  if (this->hygro_.component != NULL) {
    ESP_LOGCONFIG(TAG, "MultiFan hygro name='%s', weight=%.2f", this->hygro_.component->get_name().c_str(), this->hygro_.weight);
  }
}

void MultiFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;
}

void MultiFan::update() {
  ESP_LOGCONFIG(TAG, "MultiFan::update");
  if (this->vicinity_sens_ && this->vicinity_sens_->has_state()) {
    float state = this->vicinity_sens_->state;
    if (state != this->vicinity_temp_) {
      this->vicinity_temp_ = state;
      ESP_LOGCONFIG(TAG, "updated vicinity temperature: %.2f°C", this->vicinity_temp_);
    }
  }
  if (this->windspeed_sens_ && this->windspeed_sens_->has_state()) {
    float state = this->windspeed_sens_->state * 100;
    if (state != this->windspeed_) {
      this->windspeed_ = state;
      ESP_LOGCONFIG(TAG, "updated wind speed: %.2f km/h", this->windspeed_);
    }
  }

  if (this->thermo_.component)
  {
    auto call = this->thermo_.component->make_call();
    ESP_LOGD(TAG, "therm @ %p", call);
    if (call.get_target_temperature().has_value()) {
      float temp = call.get_target_temperature().value();
      ESP_LOGD(TAG, "has value %f", temp);
      this->target_temperature_ = temp;
    }
  }
  set_weighted_fan_value();
}

void MultiFan::set_weighted_fan_value() {
  ESP_LOGD(TAG, "set_weighted_fan_value");
  climate::ClimateMode therm_mode = this->thermo_.component->mode;
  climate::ClimateMode hygro_mode = this->hygro_.component->mode;
  if (hygro_mode == climate::CLIMATE_MODE_HEAT_COOL || hygro_mode == climate::CLIMATE_MODE_COOL) {
    auto fan = this->fan_->make_call();
    float speed;
    float hum_ctrl_val = this->hygro_.component->get_output_value() * (-1.0);
    float therm_ctrl_val = this->thermo_.component->get_output_value() * (-1.0);
    float set_val = hum_ctrl_val * 100.0;
    if (therm_mode != climate::CLIMATE_MODE_OFF) {
      if (this->target_temperature_ < this->vicinity_temp_-1.0 && therm_ctrl_val > 1) {
        ESP_LOGD(TAG, "target_temp: %.2f°C < vicinity_temp: %.2f-1.0°C -> use weather data windspeed %.2f km/h",
            this->target_temperature_, this->vicinity_temp_, this->windspeed_);
        float setv = (hum_ctrl_val*this->hygro_.weight + (this->windspeed_/100.)*this->thermo_.weight) / (this->hygro_.weight+(this->windspeed_/100.));
        set_val = clamp(setv, hum_ctrl_val, setv) * 100.;
      } else
      set_val = (hum_ctrl_val*this->hygro_.weight + therm_ctrl_val*this->thermo_.weight) / (this->hygro_.weight+this->thermo_.weight) * 100.;
    }
    speed = clamp(set_val, 0.f, 100.f);
    ESP_LOGD(TAG, "calculate fan value. humidifier: %.2f / thermostat: %.2f / set_val: %.2f / speed: %.0f%%",
      hum_ctrl_val, therm_ctrl_val, set_val, speed);
    fan.set_state(!!speed);
    fan.set_speed(speed);
    fan.perform();
    return;
  }
  ESP_LOGD(TAG, "Hygro PID Controller %s dehumidifcation disabled, don't set fan value",
    this->hygro_.component->get_name().c_str());
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace multi
}  // namespace esphome
