#include <float.h>
#include "multi_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace geckofan {

static const char *const TAG = "geckofan";

void MultiFan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MultiFan...");
  // auto traits = fan::FanTraits(true, true, false, 100);
  // this->fan_->set_traits(traits);
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
    ESP_LOGCONFIG(TAG, "Windspeed sensor name='%s'", this->windspeed_sens_->get_name().c_str());
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
  // During cooling / dehumidification calibration the PID relay output is routed here as the
  // climate's cool_output; drive the PWM directly so the autotune relay reaches the fan.
  if (this->calibrating_fan_()) {
    float level = clamp(state, 0.0f, 1.0f);
    ESP_LOGD(TAG, "calibration pass-through: fan output %.2f", level);
    this->output_->set_level(level);
    return;
  }
  ESP_LOGV(TAG, "write_state %.2f (ignored)", state);
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

  // The Home Assistant windspeed sensor reports the simulated ambient wind in km/h.
  // It defines a band the fan output must stay within (see README.md):
  //   windspeed == 0 km/h  -> fan must never exceed 10%   (ceil = 10)
  //   windspeed >  5 km/h  -> fan must be at least 10%     (floor >= 10)
  //   windspeed > 30 km/h  -> fan must be at least 90%
  //   windspeed > 50 km/h  -> fan must run at 100%
  // The floor is a hard minimum that the PID weighting may not pull below; the ceiling
  // only caps the fan in calm weather and ramps up to 100% by 5 km/h.
  float windspeed = get_windspeed();
  float wind_ceil = clamp(10.0f + windspeed / 5.0f * 90.0f, 10.0f, 100.0f);  // 10% @ 0 .. 100% @ >=5 km/h
  float wind_floor;                                                          // piecewise ambient-wind minimum
  if (windspeed <= 5.0f)
    wind_floor = windspeed / 5.0f * 10.0f;                       // 0% .. 10%
  else if (windspeed <= 30.0f)
    wind_floor = 10.0f + (windspeed - 5.0f) / 25.0f * 80.0f;     // 10% .. 90%
  else if (windspeed <= 50.0f)
    wind_floor = 90.0f + (windspeed - 30.0f) / 20.0f * 10.0f;    // 90% .. 100%
  else
    wind_floor = 100.0f;

  float hum_ctrl_val = this->hygro_.component->get_output_value() * (-1.0f);
  float therm_ctrl_val = this->thermo_.component->get_output_value() * (-1.0f);

  // PID-demanded cooling/dehumidification share of the fan output (0..100%).
  float set_val = 0.0f;
  if (dehumidification || cooling) {
    set_val = hum_ctrl_val * 100.0f;  // dehumidification baseline
    if (cooling) {
      float vt;
      bool have_vt = get_vicinity_temperature(&vt);
      if (have_vt && this->thermo_.component->target_temperature < vt - 1.0f && therm_ctrl_val > 1.0f) {
        // The fan can only equalize towards the vicinity temperature, never cool below it.
        // The thermostat is asking for the impossible, so ignore its demand here and let the
        // ambient windspeed band drive the fan instead of blasting it pointlessly.
        ESP_LOGD(TAG, "target_temp: %.2f°C < vicinity_temp: %.2f-1.0°C -> ignore thermostat, follow ambient wind %.2f km/h",
            this->thermo_.component->target_temperature, vt, windspeed);
        set_val = hum_ctrl_val * 100.0f;
      } else {
        set_val = (hum_ctrl_val * this->hygro_.weight + therm_ctrl_val * this->thermo_.weight) /
                  (this->hygro_.weight + this->thermo_.weight) * 100.0f;
      }
    }
  }

  // Clamp the PID demand into the windspeed band: the floor enforces the minimum ambient
  // wind, the ceiling enforces the calm-weather cap.
  float speed = clamp(clamp(set_val, wind_floor, wind_ceil), 0.0f, 100.0f);
  ESP_LOGD(TAG, "fan value: windspeed=%.2f km/h, humidifier=%.2f, thermostat=%.2f, set_val=%.2f, band=[%.1f, %.1f] -> speed=%.0f%%",
      windspeed, hum_ctrl_val, therm_ctrl_val, set_val, wind_floor, wind_ceil, speed);

  auto fan = this->fan_->make_call();
  fan.set_state(speed > 0.0f);
  fan.set_speed((int) speed);
  fan.perform();
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
  // While the fan is being autotuned its output is driven directly via write_state();
  // don't let the weighted formula clobber the relay test.
  if (this->calibrating_fan_()) {
    return;
  }
  if (this->enable_auto_mode_) {
    set_weighted_fan_value();
  }
}

float MultiFan::get_setup_priority() const { return setup_priority::PROCESSOR; }

}  // namespace geckofan
}  // namespace esphome
