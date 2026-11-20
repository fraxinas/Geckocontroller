from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import globals, sensor
from esphome.components import time as time_
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_TIMEOUT, CONF_TRIGGER_ID

CODEOWNERS = ["@fraxinas"]

geckoweather_ns = cg.esphome_ns.namespace("geckoweather")
GeckoWeather = geckoweather_ns.class_("GeckoWeather", cg.Component)
FallbackRainTrigger = geckoweather_ns.class_("FallbackRainTrigger", automation.Trigger.template())

CONF_CODE_SENSOR = "code_sensor"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_HUMIDITY_SENSOR = "humidity_sensor"
CONF_TEMPERATURE_SETPOINT = "temperature_setpoint"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_FALLBACK = "fallback"
CONF_LONGITUDE = "longitude"
CONF_WARMEST_HOUR = "warmest_hour"
CONF_TEMPERATURE = "temperature"
CONF_HUMIDITY = "humidity"
CONF_DAY = "day"
CONF_NIGHT = "night"
CONF_CODE = "code"
CONF_RAIN_HOUR = "rain_hour"
CONF_ON_RAIN = "on_rain"
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


def _day_night(day, night):
    return cv.Schema(
        {
            cv.Optional(CONF_DAY, default=day): cv.float_,
            cv.Optional(CONF_NIGHT, default=night): cv.float_,
        }
    )


_hour = cv.float_range(min=0.0, max=24.0)

# Standard day/night cycle standing in for the historic feed while HA does not provide it. Hours are
# *solar* hours at `longitude` -- set it to the sun's longitude so the cycle follows the simulated day.
FALLBACK_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        cv.Optional(CONF_TIMEOUT, default="10min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LONGITUDE, default=0.0): cv.float_range(min=-180.0, max=180.0),
        cv.Optional(CONF_WARMEST_HOUR, default=15.0): _hour,
        cv.Optional(CONF_TEMPERATURE, default={}): _day_night(26.0, 21.0),
        cv.Optional(CONF_HUMIDITY, default={}): _day_night(65.0, 80.0),
        cv.Optional(CONF_CODE, default=1): cv.int_,
        cv.Optional(CONF_RAIN_HOUR, default=8.0): _hour,
        cv.Optional(CONF_ON_RAIN): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FallbackRainTrigger)}
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GeckoWeather),
        cv.Required(CONF_CODE_SENSOR): cv.use_id(sensor.Sensor),
        # Historic temperature/humidity from HA, passed on to the setpoint sensors the climates
        # follow -- or, while the feed is lost, the fallback cycle.
        cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_SETPOINT): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HUMIDITY_SETPOINT): cv.use_id(sensor.Sensor),
        # Cap (°C) of the temperature setpoint: a hotter historic (or fallback) value is passed on as
        # this, never above it.
        cv.Optional(CONF_MAX_TEMPERATURE): cv.float_,
        cv.Optional(CONF_PRECIPITATION): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ATTENUATION): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_DEFAULT): cv.Schema(_RULE_BODY),
        cv.Optional(CONF_RULES, default=[]): cv.ensure_list(
            cv.Schema({cv.Required(CONF_CODES): cv.ensure_list(cv.int_), **_RULE_BODY})
        ),
        cv.Optional(CONF_FALLBACK): FALLBACK_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_code_sensor(await cg.get_variable(config[CONF_CODE_SENSOR])))
    for key, setter in [
        (CONF_TEMPERATURE_SENSOR, var.set_temperature_sensor),
        (CONF_HUMIDITY_SENSOR, var.set_humidity_sensor),
        (CONF_TEMPERATURE_SETPOINT, var.set_temperature_setpoint_sensor),
        (CONF_HUMIDITY_SETPOINT, var.set_humidity_setpoint_sensor),
    ]:
        if key in config:
            cg.add(setter(await cg.get_variable(config[key])))
    if CONF_MAX_TEMPERATURE in config:
        cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))
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

    if CONF_FALLBACK in config:
        fb = config[CONF_FALLBACK]
        cg.add(var.set_fallback_time(await cg.get_variable(fb[CONF_TIME_ID])))
        cg.add(var.set_fallback_timeout(fb[CONF_TIMEOUT]))
        cg.add(var.set_fallback_longitude(fb[CONF_LONGITUDE]))
        cg.add(var.set_fallback_warmest_hour(fb[CONF_WARMEST_HOUR]))
        cg.add(var.set_fallback_temperature(fb[CONF_TEMPERATURE][CONF_DAY], fb[CONF_TEMPERATURE][CONF_NIGHT]))
        cg.add(var.set_fallback_humidity(fb[CONF_HUMIDITY][CONF_DAY], fb[CONF_HUMIDITY][CONF_NIGHT]))
        cg.add(var.set_fallback_code(fb[CONF_CODE]))
        cg.add(var.set_fallback_rain_hour(fb[CONF_RAIN_HOUR]))
        for conf in fb.get(CONF_ON_RAIN, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)
