// Gecko common

#include <string>
#include <sstream>
#include "gecko_common.h"
#include "esphome/core/color.h"
static const char *TAG = "geckoconsole";

using namespace esphome;

float ColorStringToF(const std::string &Text)
{
    std::istringstream ss(Text);
    float result;
    return ss >> result ? result : 0;
}

void HassHSColorTextToEsphomeColor(std::string text, Color *color)
{
    int h;
    float s, v, r, g, b;
    rgb_to_hsv(color->r, color->g, color->b, h, s, v);

    ESP_LOGD(TAG, "HassHSColorTextToEsphomeColor text='%s' prev rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f)", text.c_str(), color->r, color->g, color->b, h, s, v);

    int p1 = text.find(",");
    int p2 = text.find(")");
    std::string h_str = text.substr(1, p1-1).erase(0, text.find_first_not_of(" "));
    std::string s_str = text.substr(p1+2, p2-p1-2);

    h = ColorStringToF(h_str);
    s = ColorStringToF(s_str) / 100.;
    hsv_to_rgb(h, s, v, r, g, b);

    *color = Color(uint8_t (round(r)), uint8_t (round(g)), uint8_t (round(b)));
    ESP_LOGD(TAG, "HassHSColorTextToEsphomeColor h='%s' s='%s' rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f)", h_str.c_str(), s_str.c_str(), color->r, color->g, color->b, h, s, v);
}

void UpdateBrightnessToEsphomeColor(double brightness, Color *color)
{
    int h;
    float s, v, r, g, b;
    rgb_to_hsv(color->r, color->g, color->b, h, s, v);

    ESP_LOGD(TAG, "UpdateBrightnessToEsphomeColor b=%3.2f prev rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f)", brightness, color->r, color->g, color->b, h, s, v);
    hsv_to_rgb(h, s, brightness, r, g, b);
    *color = Color(uint8_t (round(r)), uint8_t (round(g)), uint8_t (round(b)));
    ESP_LOGD(TAG, "UpdateBrightnessToEsphomeColor new rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f)", color->r, color->g, color->b, h, s, brightness);
}

std::string HSColorFromRotary(float rotary_state, unsigned char subpage, Color *color)
{
    int h;
    float s, v, r, g, b;
    rgb_to_hsv(color->r, color->g, color->b, h, s, v);

    ESP_LOGD(TAG, "HSColorFromRotary prev rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f),rot=%.2f,subpage=%d", color->r, color->g, color->b, h, s, v, rotary_state, subpage);

    if (subpage == 3) {
        h = round(rotary_state * 360. / 48.);
    } else if (subpage == 4) {
        s = rotary_state / 25.;
    }
    hsv_to_rgb(h, s, v, r, g, b);
    *color = Color(uint8_t (round(r)), uint8_t (round(g)), uint8_t (round(b)));

    std::ostringstream oss;
    oss.precision(0);
    oss << std::fixed << "(" << h << ", " << s*100. << ")";
    std::string out = oss.str();

    ESP_LOGD(TAG, "HSColorFromRotary new  rgb=[%3d,%3d,%3d] hsv=(%3d,%.2f,%3.0f) -> '%s'", color->r, color->g, color->b, h, s, v, out.c_str());
    return out;
}
