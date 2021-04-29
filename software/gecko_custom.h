// Geckocontrol

#include "esphome.h"
#include "esphome/core/log.h"

static const char *TAG = "gecko";

globals::GlobalsComponent<uint16_t> *sun_effect_delay;
// in seconds, to control SunLightEffects speed

#define SUN_LUT_LEN 6
static uint8_t sun_lut[SUN_LUT_LEN][4] = {
  {10, 5, 5, 250},
  {80, 5, 5, 250},
  {100, 200, 5, 200},
  {150, 250, 130, 5},
  {180, 250, 200, 20},
  {220, 250, 250, 250}};

class SunColorLightEffect : public LightEffect {
 public:
  explicit SunColorLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override {
    ESP_LOGD(TAG, "Starting SunColorLightEffect '%s' %s", this->name_.c_str(), this->inverse_ ? "inversed" : "");
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

#define MAX_DAYLIGHT 0.9

class SunDayLightEffect : public LightEffect {
 public:
  explicit SunDayLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override {
    ESP_LOGD(TAG, "Starting SunDayLightEffect '%s' %s", this->name_.c_str(), this->inverse_ ? "inversed" : "");
    this->transition_length_ = id(sun_effect_delay)*10;
    float current_brightness;
    id(day_light).current_values_as_brightness(&current_brightness);
    if (this->inverse_) {
      this->brightness_ = current_brightness;
      this->final_brightness_ = 0.01;
      this->incr_ = (-1.f)/255.f;
    } else {
      this->brightness_ = current_brightness;
      this->final_brightness_ = MAX_DAYLIGHT;
      this->incr_ = (+1.f)/255.f;
    }
  }

  void apply() override {
    const uint32_t now = millis();
    if (now < this->last_brightness_change_ + this->transition_length_) {
      return;
    }
    auto call = day_light->make_call();
    if (!this->inverse_ && this->brightness_ >= this->final_brightness_) {
      ESP_LOGD(TAG, "SunDayLightEffect '%s' at max. brightness.", this->name_.c_str());
      call.set_effect(0);
    } else if (this->inverse_ && this->brightness_ <= this->final_brightness_) {
      ESP_LOGD(TAG, "SunDayLightEffect '%s' off.", this->name_.c_str());
      call.set_state(false);
    } else {
      this->brightness_ += this->incr_;
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
  float final_brightness_{};
  float incr_{};
};

class GeckoCustomComponent : public Component {
 public:
  void setup() override {
    sun_effect_delay = new globals::GlobalsComponent<uint16_t>(10);
    App.register_component(sun_effect_delay);

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
