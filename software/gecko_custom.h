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

class SunLightEffect : public LightEffect {
 public:
  explicit SunLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override {
    ESP_LOGD(TAG, "Starting SunLightEffect '%s' %s", this->name_.c_str(), this->inverse_ ? "inversed" : "");
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
      ESP_LOGD(TAG, "SunLightEffect '%s' finished", this->name_.c_str());
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
  bool inverse_{0};
  uint32_t last_color_change_{0};
  uint32_t transition_length_{};
  size_t index_{};
  size_t final_index_{};
  ssize_t incr_{};
};

class GeckoCustomComponent : public Component {
 public:
  void setup() override {
    sun_effect_delay = new globals::GlobalsComponent<uint16_t>(10);
    App.register_component(sun_effect_delay);

    SunLightEffect *sunrise_lighteffect, *sunset_lighteffect;
    sunrise_lighteffect = new SunLightEffect("Sunrise");
    sunset_lighteffect = new SunLightEffect("Sunset");
    sunset_lighteffect->set_inverse(true);
    scenic_light->add_effects({sunrise_lighteffect, sunset_lighteffect});
    ESP_LOGD(TAG, "Added SunLightEffects");
  }
};
