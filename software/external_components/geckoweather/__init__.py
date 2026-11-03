import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import globals, sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@fraxinas"]

geckoweather_ns = cg.esphome_ns.namespace("geckoweather")
GeckoWeather = geckoweather_ns.class_("GeckoWeather", cg.Component)

CONF_CODE_SENSOR = "code_sensor"
CONF_PRECIPITATION = "precipitation"
CONF_ATTENUATION = "attenuation"
CONF_DEFAULT = "default"
CONF_RULES = "rules"
CONF_CODES = "codes"
CONF_OKTAS = "oktas"
CONF_CLOUDS = "clouds"
CONF_RAIN = "rain"
CONF_EFFECT = "effect"


# Cloud cover in oktas (0 = clear .. 8 = overcast) -> sky attenuation, 0..1. This is the one
# attenuation figure in the system: it says how bright the terrarium *looks* under this weather,
# driving the growlight/scenic pair by day and the moonlight by night. The daylight lamp is not
# weather-dimmed -- it is the daytime heater and answers to the thermostat PID alone.
#
# /9 rather than /8 keeps a floor, so even full overcast is dim rather than fully dark.
def _atten(oktas):
    return 1.0 - oktas / 9.0


# One weather behaviour (cloud cover in oktas 0..8, passing clouds 0..100%, rain 0..5, optional
# named light effect). `oktas` is the steady sky; `clouds` is broken cloud drifting across the sun
# on top of it, which the illumination controller renders as individual clouds dimming the light.
_RULE_BODY = {
    cv.Optional(CONF_OKTAS, default=0): cv.int_range(min=0, max=8),
    cv.Optional(CONF_CLOUDS, default=0.0): cv.percentage,
    cv.Optional(CONF_RAIN, default=0.0): cv.float_,
    cv.Optional(CONF_EFFECT, default=""): cv.string,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GeckoWeather),
        cv.Required(CONF_CODE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_PRECIPITATION): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ATTENUATION): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_DEFAULT): cv.Schema(_RULE_BODY),
        cv.Optional(CONF_RULES, default=[]): cv.ensure_list(
            cv.Schema({cv.Required(CONF_CODES): cv.ensure_list(cv.int_), **_RULE_BODY})
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_code_sensor(await cg.get_variable(config[CONF_CODE_SENSOR])))
    if CONF_PRECIPITATION in config:
        cg.add(var.set_precipitation_sensor(await cg.get_variable(config[CONF_PRECIPITATION])))
    if CONF_ATTENUATION in config:
        cg.add(var.set_attenuation(await cg.get_variable(config[CONF_ATTENUATION])))
    if CONF_DEFAULT in config:
        d = config[CONF_DEFAULT]
        cg.add(var.set_default_rule(_atten(d[CONF_OKTAS]), d[CONF_CLOUDS], d[CONF_RAIN], d[CONF_EFFECT]))
    # Flatten each rule's code list into one add_rule call per WMO code.
    for rule in config[CONF_RULES]:
        for code in rule[CONF_CODES]:
            cg.add(var.add_rule(code, _atten(rule[CONF_OKTAS]), rule[CONF_CLOUDS], rule[CONF_RAIN], rule[CONF_EFFECT]))
