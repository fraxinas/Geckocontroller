// Geckocontrol

#include "esphome.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

static const char *TAG = "gecko";

globals::GlobalsComponent<uint16_t> *sun_effect_delay;
// in seconds, to control SunLightEffects speed

globals::GlobalsComponent<float> *sun_rise_max;
// sunrise maximum (for high power daylight lamps when not using a thermostat)

#define SUN_LUT_LEN 7
static uint8_t sun_lut[SUN_LUT_LEN][4] = {
  {100, 20, 250, 20},
  {120, 5, 250, 150},
  {140, 5, 5, 250},
  {160, 200, 5, 200},
  {180, 250, 130, 5},
  {200, 250, 200, 20},
  {220, 250, 250, 250}};

class SunColorLightEffect : public LightEffect {
 public:
  explicit SunColorLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override {
    ESP_LOGD(TAG, "Starting SunColorLightEffect '%s' %s",
      this->name_.c_str(), this->inverse_ ? "inversed" : "");
    this->transition_length_ = id(sun_effect_delay)*1000;
    if (this->inverse_) {
      this->index_ = SUN_LUT_LEN;
      this->final_index_ = 0;
      this->incr_ = -1;
    } else {
      this->index_ = 0;
      this->final_index_ = SUN_LUT_LEN;
      this->incr_ = +1;
    }
  }

  void apply() override {
    const uint32_t now = millis();
    if (now < this->last_color_change_ + this->transition_length_) {
      return;
    }
    auto call = scenic_light->make_call();
    size_t i = this->index_ += this->incr_;
    if (i == this->final_index_) {
      ESP_LOGD(TAG, "SunColorLightEffect '%s' finished", this->name_.c_str());
      call.set_effect(0);
    } else {
      float a, r, g, b;
      a = sun_lut[i][0] / 255.f;
      r = sun_lut[i][1] / 255.f;
      g = sun_lut[i][2] / 255.f;
      b = sun_lut[i][3] / 255.f;

      call.set_brightness(a);
      call.set_rgb(r,g,b);
      call.set_publish(true);
      call.set_transition_length(this->transition_length_);
      call.set_state(true);
      this->last_color_change_ = now;
    }
    call.perform();
  }

  void set_inverse(bool direction) { this->inverse_ = direction; }

 protected:
  bool inverse_{false};
  uint32_t last_color_change_{0};
  uint32_t transition_length_{};
  size_t index_{};
  size_t final_index_{};
  ssize_t incr_{};
};

class SunDayLightEffect : public LightEffect {
 public:
  explicit SunDayLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override {
    ESP_LOGD(TAG, "Starting SunDayLightEffect '%s' %s",
      this->name_.c_str(), this->inverse_ ? "inversed" : "");
    this->transition_length_ = id(sun_effect_delay)*235;
    // with delay of 180 the entire transition will take 3 hours
    if (this->inverse_) {
      id(twilight_attenuation) = 1.0;
      this->incr_ = (-1.f)/255.f;
    } else {
      id(is_daytime) = true;
      id(twilight_attenuation) = 0.0;
      this->incr_ = (+1.f)/255.f;
    }
  }

  void apply() override {
    const uint32_t now = millis();
    if (now < this->last_brightness_change_ + this->transition_length_) {
      return;
    }
    auto call = day_light->make_call();
    id(twilight_attenuation) += this->incr_;
    if (!this->inverse_ && id(twilight_attenuation) >= 1.0) {
      ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_.c_str());
      call.set_effect(0);
    } else if (this->inverse_ && id(twilight_attenuation) <= 0.0) {
      ESP_LOGD(TAG, "SunDayLightEffect '%s' finished.", this->name_.c_str());
      call.set_state(false);
      id(is_daytime) = false;
    } else {
      id(day_light).current_values_as_brightness(&this->brightness_);
      ESP_LOGD(TAG, "SunDayLightEffect '%s' apply. current_values_as_brightness=%.3f",
        this->name_.c_str(), this->brightness_);
      if (this->inverse_) {
        id(day_light).current_values_as_brightness(&this->brightness_);
        this->brightness_ /= (id(twilight_attenuation)*id(skycond_attenuation));
      }
      if (!this->inverse_) {
        this->brightness_ = id(sun_rise_max);
      }
      if (this->brightness_ > 1.0) {
          this->brightness_ = 1.0;
      }
      float new_brightness = this->brightness_*id(twilight_attenuation)*id(skycond_attenuation);
      ESP_LOGD(TAG, "SunDayLightEffect normalized brightness=%.3f, twilight_attenuation=%.3f, skycond_attenuation=%.3f, new_brightness=%.3f",
        this->brightness_, id(twilight_attenuation), id(skycond_attenuation), new_brightness);
      this->brightness_ = new_brightness;
      call.set_brightness(this->brightness_);
      call.set_publish(true);
      call.set_transition_length(this->transition_length_);
      call.set_state(true);
      this->last_brightness_change_ = now;
    }
    call.perform();
  }

  void set_inverse(bool direction) { this->inverse_ = direction; }

 protected:
  bool inverse_{false};
  uint32_t last_brightness_change_{0};
  uint32_t transition_length_{};
  float brightness_{};
  float incr_{};
};

class RainPumpOutput : public FloatOutput, public Component {
 public:
  void set_period(unsigned int period) { period_ = period; };

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  void loop() override;

  float state_{0};
  bool current_state_{false};
  unsigned int period_start_time_{0};
  unsigned int period_{180*1000};
};

void RainPumpOutput::setup() {
  this->turn_off();
  this->current_state_ = false;
}

void RainPumpOutput::loop() {
  unsigned long now = millis();
  float scaled_state = this->state_ * (float)this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGD(TAG, "now=%lu period=%u End of period. State: %.2f, Scaled state: %.0f",
      now, this->period_, this->state_, scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
    if (!this->current_state_) {
      ESP_LOGD(TAG, "Turn on rain pump");
      id(rain_pump).turn_on();
      this->current_state_ = true;
    }
    return;
  } else if (this->current_state_) {
    ESP_LOGD(TAG, "Turn off rain pump");
    id(rain_pump).turn_off();
    this->current_state_ = false;
  }
}

void RainPumpOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Rain Pump Output:");
  int h, m, s;
  h = (this->period_/3600000);
  m = (this->period_/1000 -(3600*h))/60;
  s = (this->period_/1000 -(3600*h)-(m*60));
  ESP_LOGCONFIG(TAG, "  Period: %02d:%02d:%02d", h, m, s);
  LOG_FLOAT_OUTPUT(this);
}

void RainPumpOutput::write_state(float state) {
  ESP_LOGD(TAG, "RainPumpOutput write_state %.2f", state);
  this->state_ = state;
}

class WeightedFanOutput : public FloatOutput, public Component {
 public:
  WeightedFanOutput(fan::FanState *fan, sensor::Sensor *vicinity,
    sensor::Sensor *target_thermo, int hygro_weight, int thermo_weight)
      : fan_(fan), vicinity_(vicinity), target_thermo_(target_thermo),
        hygro_weight_(hygro_weight), thermo_weight_(thermo_weight) {}

  void setup() override;

  void set_hygro(pid::PIDClimate *hygro);
  void set_thermo(pid::PIDClimate *thermo);

  void set_weighted_fan_value(bool hygro_action, float state);

 protected:
  void write_state(float state) override;

  fan::FanState *fan_;
  pid::PIDClimate *hygro_{NULL};
  pid::PIDClimate *thermo_{NULL};
  sensor::Sensor *vicinity_{NULL};
  sensor::Sensor *target_thermo_{NULL};
  float hygro_weight_{1};
  float thermo_weight_{1};
  float windspeed_{0};
};

void WeightedFanOutput::setup() {
  ESP_LOGD(TAG, "WeightedFanOutput setup");
}

void WeightedFanOutput::set_hygro(pid::PIDClimate *hygro) {
  ESP_LOGD(TAG, "WeightedFanOutput setup set_hygro %s", hygro->get_name().c_str());
  this->hygro_ = hygro;
}

void WeightedFanOutput::set_thermo(pid::PIDClimate *thermo) {
  ESP_LOGD(TAG, "WeightedFanOutput setup set_thermo %s", thermo->get_name().c_str());
  this->thermo_ = thermo;
}

void WeightedFanOutput::write_state(float state) {
  ESP_LOGD(TAG, "WeightedFanOutput write_state %.2f km/h", state);
  this->windspeed_ = state*100.;
}

void WeightedFanOutput::set_weighted_fan_value(bool hygro_action, float state) {
    std::string name = hygro_action ? this->hygro_->get_name() : this->thermo_->get_name();
    ClimateMode therm_mode = this->thermo_->mode;
    ClimateMode humid_mode = this->hygro_->mode;
    ClimateMode primary_mode = hygro_action ? humid_mode : therm_mode;
    if (primary_mode == CLIMATE_MODE_HEAT_COOL || primary_mode == CLIMATE_MODE_COOL) {
      auto fan = this->fan_->make_call();
      float hum_ctrl_val = this->hygro_->get_output_value() * (-1.0);
      float therm_ctrl_val = this->thermo_->get_output_value() * (-1.0);
      float set_val = state * 100.0;
      float speed;

      if (therm_mode != CLIMATE_MODE_OFF) {
        float target_temp = this->target_thermo_->get_state();
        float vicinity_temp = this->vicinity_->get_state();
        ESP_LOGD(TAG, "%s PID Controller fan write_action target_temp: %.2f°C / vicinity_temp: %.2f°C",
          name.c_str(), target_temp, vicinity_temp);
        if (target_temp < vicinity_temp-1.0 && hum_ctrl_val >= 1.0) {
          ESP_LOGD(TAG, "%s PID Controller fan write_action use weather data windspeed %.2f km/h",
            name.c_str(), this->windspeed_);
          set_val = this->windspeed_;
        } else
        set_val = (hum_ctrl_val*this->hygro_weight_ + therm_ctrl_val*this->thermo_weight_) / (this->hygro_weight_+this->thermo_weight_) * 100.;
      }
      speed = clamp(set_val, 0.f, 100.f);
      ESP_LOGD(TAG, "%s PID Controller clamp fan write_action: %.3f / humidifier: %.2f / thermostat: %.2f / set_val: %.2f / speed: %.0f%%",
        name.c_str(), state, hum_ctrl_val, therm_ctrl_val, set_val, speed);
      fan.set_state(!!speed);
      fan.set_speed(speed);
      fan.perform();
      return;
    }
    ESP_LOGD(TAG, "%s PID Controller %s disabled, ignore fan value of %.3f",
      name.c_str(), hygro_action ? "dehumidification" : "cooling", state);
  }

class HygroWeightedFanOutput : public FloatOutput, public Component {
 public:
  HygroWeightedFanOutput(WeightedFanOutput *weighted_fan, pid::PIDClimate *hygro)
      : weighted_fan_(weighted_fan), hygro_(hygro) {}
  void setup() override;

 protected:
  void write_state(float state) override;
  WeightedFanOutput *weighted_fan_{NULL};
  pid::PIDClimate *hygro_{NULL};
};

void HygroWeightedFanOutput::setup() {
  ESP_LOGD(TAG, "HygroWeightedFanOutput setup");
  if (this->weighted_fan_ && this->hygro_) {
    this->weighted_fan_->set_hygro(this->hygro_);
  }
}

void HygroWeightedFanOutput::write_state(float state) {
  ESP_LOGD(TAG, "HygroWeightedFanOutput write_state %.2f", state);
  this->weighted_fan_->set_weighted_fan_value(true, state);
}

class ThermoWeightedFanOutput : public FloatOutput, public Component {
 public:
  ThermoWeightedFanOutput(WeightedFanOutput *weighted_fan, pid::PIDClimate *thermo)
      : weighted_fan_(weighted_fan), thermo_(thermo) {}
  void setup() override;

 protected:
  void write_state(float state) override;
  WeightedFanOutput *weighted_fan_{NULL};
  pid::PIDClimate *thermo_{NULL};
};

void ThermoWeightedFanOutput::setup() {
  ESP_LOGD(TAG, "ThermoWeightedFanOutput setup");
  if (this->weighted_fan_ && this->thermo_) {
    this->weighted_fan_->set_thermo(this->thermo_);
  }
}

void ThermoWeightedFanOutput::write_state(float state) {
  ESP_LOGD(TAG, "ThermoWeightedFanOutput write_state %.2f", state);
  this->weighted_fan_->set_weighted_fan_value(false, state);
}

class GeckoCustomComponent : public Component {
 public:
  void setup() override {
    sun_effect_delay = new globals::GlobalsComponent<uint16_t>(180);
    App.register_component(sun_effect_delay);
    sun_rise_max = new globals::GlobalsComponent<float>(0.75);
    App.register_component(sun_rise_max);

    SunColorLightEffect *sunrise_colorlighteffect, *sunset_colorlighteffect;
    sunrise_colorlighteffect = new SunColorLightEffect("Sunrise Color");
    sunset_colorlighteffect = new SunColorLightEffect("Sunset Color");
    sunset_colorlighteffect->set_inverse(true);
    scenic_light->add_effects({sunrise_colorlighteffect, sunset_colorlighteffect});

    SunDayLightEffect *sunrise_daylighteffect, *sunset_daylighteffect;
    sunrise_daylighteffect = new SunDayLightEffect("Sunrise Daylight");
    sunset_daylighteffect = new SunDayLightEffect("Sunset Daylight");
    sunset_daylighteffect->set_inverse(true);
    day_light->add_effects({sunrise_daylighteffect, sunset_daylighteffect});
    ESP_LOGD(TAG, "Added SunLightEffects");
  }
};
