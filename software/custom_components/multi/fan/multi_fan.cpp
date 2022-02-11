#include <float.h>
#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace multi {

static const char *const TAG = "multi.fan";

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MultiFan...");
  auto traits = fan::FanTraits(true, true, false, 100);
  this->fan_->set_traits(traits);
  this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}

void MultiFan::dump_config() {
  ESP_LOGCONFIG(TAG, "MultiFan '%s':", this->fan_->get_name().c_str());
  if (this->hygro_.component != NULL) {
    ESP_LOGCONFIG(TAG, "Hygro sensor name='%s', weight=%.2f", this->hygro_.component->get_name().c_str(), this->hygro_.weight);
  }
  if (this->thermo_.component != NULL) {
    ESP_LOGCONFIG(TAG, "Thermo sensor name='%s', weight=%.2f", this->thermo_.component->get_name().c_str(), this->thermo_.weight);
  }
  if (this->vicinity_sens_ != NULL) {
    ESP_LOGCONFIG(TAG, "Vicinity temperature sensor name='%s'", this->vicinity_sens_->get_name().c_str());
  }
  if (this->windspeed_sens_ != NULL) {
    ESP_LOGCONFIG(TAG, "Vicinity windspeed sensor name='%s'", this->windspeed_sens_->get_name().c_str());
  }
}

void MultiFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  bool enable = this->fan_->oscillating;
  if (enable != this->enable_auto_mode_) {
    this->enable_auto_mode_ = enable;
    ESP_LOGD(TAG, "Setting auto mode: %s", ONOFF(enable));
  }

  float speed = 0.0f;
  if (this->fan_->state) {
    speed = static_cast<float>(this->fan_->speed) / 100.f;
  }
  ESP_LOGD(TAG, "Setting speed: %.2f", speed);
  this->output_->set_level(speed);
}

void MultiFan::write_state(float state) {
  ESP_LOGV(TAG, "write_state %.2f (igmored)", state);
}

float MultiFan::get_windspeed() {
  if (this->windspeed_sens_ && this->windspeed_sens_->has_state()) {
    return this->windspeed_sens_->state;
  }
  return 0.f;
}

bool MultiFan::get_vicinity_temperature(float *vicinity) {
  if (this->vicinity_sens_ && this->vicinity_sens_->has_state()) {
    *vicinity = this->vicinity_sens_->state;
    return true;
  }
  return false;
}

void MultiFan::set_weighted_fan_value() {
  climate::ClimateMode therm_mode = this->thermo_.component->mode;
  climate::ClimateMode hygro_mode = this->hygro_.component->mode;
  bool dehumidification = (hygro_mode == climate::CLIMATE_MODE_HEAT_COOL || hygro_mode == climate::CLIMATE_MODE_COOL);
  bool cooling = (therm_mode == climate::CLIMATE_MODE_HEAT_COOL || therm_mode == climate::CLIMATE_MODE_COOL);
  if (dehumidification || cooling) {
    float vt;
    bool have_vt = get_vicinity_temperature(&vt);
    auto fan = this->fan_->make_call();
    float speed;
    float hum_ctrl_val = this->hygro_.component->get_output_value() * (-1.0);
    float therm_ctrl_val = this->thermo_.component->get_output_value() * (-1.0);
    float set_val = hum_ctrl_val * 100.0;
    if (cooling) {
      if (have_vt && this->thermo_.component->target_temperature < vt-1.0 && therm_ctrl_val > 1.0) {
        float windspeed = get_windspeed();
        ESP_LOGD(TAG, "target_temp: %.2f°C < vicinity_temp: %.2f-1.0°C -> use weather data windspeed %.2f km/h",
            this->thermo_.component->target_temperature, vt, windspeed);
        float setv = (hum_ctrl_val*this->hygro_.weight + (windspeed/100.)*this->thermo_.weight) / (this->hygro_.weight+(windspeed/100.));
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
  ESP_LOGD(TAG, "PID Controllers dehumidifcation and cooling disabled, don't set fan value");
}

void MultiFan::set_hygro(const PidConf &hygro) {
  this->hygro_ = hygro;
  hygro.component->add_on_pid_computed_callback([this]() { this->update_from_pid_(true); });
}
void MultiFan::set_thermo(const PidConf &thermo) {
  this->thermo_ = thermo;
  thermo.component->add_on_pid_computed_callback([this]() { this->update_from_pid_(false); });
}

void MultiFan::update_from_pid_(bool hygro_action) {
  if (this->enable_auto_mode_) {
    set_weighted_fan_value();
  }
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace multi
}  // namespace esphome
