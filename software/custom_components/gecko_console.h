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

Color HassColorTextToEsphomeColor(std::string text)
{
    int p1 = text.find(",");
    int p2 = text.find(",",p1+1);
    std::string r = text.substr(1, p1-1).erase(0, text.find_first_not_of(" "));
    std::string g = text.substr(p1+2, p2-p1-2).erase(0, text.find_first_not_of(" "));
    std::string b = text.substr(p2+2, text.length()-2).erase(0, text.find_first_not_of(" "));
    return Color(ColorStringToF(r), ColorStringToF(g), ColorStringToF(b));
}

std::string HSColorFromEsphomeColor(float rotary_state, int ticks, Color color)
{
    int h;
    float s, v;
    rgb_to_hsv(color.r, color.g, color.b, h, s, v);
    h = round(rotary_state * 360. / ticks);
    s = round(s * 100.);

    std::ostringstream out;
    out.precision(0);
    out << std::fixed << "(" << h << ", " << s << ")";
    return out.str();
}
