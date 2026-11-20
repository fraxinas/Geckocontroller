import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, climate, sensor, text_sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_SENSORS, CONF_TIMEOUT, DEVICE_CLASS_PROBLEM

from ..geckoweather import GeckoWeather

CODEOWNERS = ["@fraxinas"]
DEPENDENCIES = ["climate"]
AUTO_LOAD = ["binary_sensor", "text_sensor"]

geckoalarm_ns = cg.esphome_ns.namespace("geckoalarm")
GeckoAlarm = geckoalarm_ns.class_("GeckoAlarm", cg.Component)
Alarm = geckoalarm_ns.enum("Alarm", is_class=True)

CONF_THERMOSTAT = "thermostat"
CONF_HYGROSTAT = "hygrostat"
CONF_TEMPERATURE_DEVIATION = "temperature_deviation"
CONF_HUMIDITY_DEFICIT = "humidity_deficit"
CONF_TEMPERATURE_HYSTERESIS = "temperature_hysteresis"
CONF_HUMIDITY_HYSTERESIS = "humidity_hysteresis"
CONF_DELAY = "delay"
CONF_CLEAR_DELAY = "clear_delay"
CONF_SENSOR_TIMEOUT = "sensor_timeout"
CONF_SENSOR_PROBLEM = "sensor_problem"
CONF_SENSOR_PROBLEM_DETAILS = "sensor_problem_details"
CONF_RAW = "raw"
CONF_WEATHER = "weather"
CONF_STALE_TIMEOUT = "stale_timeout"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_MAX_TEMPERATURE_DELAY = "max_temperature_delay"
CONF_MAX = "max"

# A watched sensor: its id alone, or with a timeout of its own. `raw`: judged by the driver's
# readings before the filters -- for sensors whose filters hold steady values back (delta).
WATCHED_SENSOR_SCHEMA = cv.maybe_simple_value(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RAW, default=False): cv.boolean,
    },
    key=CONF_SENSOR,
)

# Alarm binary sensors (device class "problem"), in the order of the C++ enum
ALARMS = {
    "overtemperature": (Alarm.OVERTEMPERATURE, "mdi:thermometer-alert"),
    "undertemperature": (Alarm.UNDERTEMPERATURE, "mdi:snowflake-thermometer"),
    "low_humidity": (Alarm.LOW_HUMIDITY, "mdi:water-alert"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GeckoAlarm),
        cv.Required(CONF_THERMOSTAT): cv.use_id(climate.Climate),
        cv.Required(CONF_HYGROSTAT): cv.use_id(climate.Climate),
        # Temperature this far (K) above / below the thermostat setpoint is out of range.
        cv.Optional(CONF_TEMPERATURE_DEVIATION, default=4.0): cv.positive_float,
        # Humidity this far (%RH, absolute) below the hygrostat setpoint is out of range.
        cv.Optional(CONF_HUMIDITY_DEFICIT, default=10.0): cv.positive_float,
        # An alarm clears once its value is this much back inside the limit ...
        cv.Optional(CONF_TEMPERATURE_HYSTERESIS, default=0.5): cv.positive_float,
        cv.Optional(CONF_HUMIDITY_HYSTERESIS, default=2.0): cv.positive_float,
        # ... raised after being out of range this long without a break, cleared after being back
        # this long.
        cv.Optional(CONF_DELAY, default="10min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CLEAR_DELAY, default="2min"): cv.positive_time_period_milliseconds,
        # Absolute maximum temperatures, whatever the setpoint: a sensor above its `max` (°C) for
        # `max_temperature_delay` raises the overtemperature alarm as well.
        cv.Optional(CONF_MAX_TEMPERATURE, default=[]): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
                    cv.Required(CONF_MAX): cv.float_,
                }
            )
        ),
        cv.Optional(CONF_MAX_TEMPERATURE_DELAY, default="2min"): cv.positive_time_period_milliseconds,
        # Sensor health: a watched sensor without a valid reading for its timeout (missing, broken,
        # read errors) is a problem; `sensor_problem_details` names the sensors concerned.
        cv.Optional(CONF_SENSOR_TIMEOUT, default="2min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SENSORS, default=[]): cv.ensure_list(WATCHED_SENSOR_SCHEMA),
        # The historic weather feeds from Home Assistant: a sensor problem while geckoweather's
        # fallback stands in for one (missing), or while the historic temperature -- the one feed
        # that changes nearly every hour -- has not changed for `stale_timeout` (stuck).
        cv.Optional(CONF_WEATHER): cv.maybe_simple_value(
            {
                cv.Required(CONF_ID): cv.use_id(GeckoWeather),
                cv.Optional(CONF_STALE_TIMEOUT, default="3h"): cv.positive_time_period_milliseconds,
            },
            key=CONF_ID,
        ),
        cv.Optional(CONF_SENSOR_PROBLEM): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM, icon="mdi:alert-circle-outline"
        ),
        cv.Optional(CONF_SENSOR_PROBLEM_DETAILS): text_sensor.text_sensor_schema(
            icon="mdi:alert-circle-outline"
        ),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM, icon=icon
            )
            for key, (_, icon) in ALARMS.items()
        },
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_thermostat(await cg.get_variable(config[CONF_THERMOSTAT])))
    cg.add(var.set_hygrostat(await cg.get_variable(config[CONF_HYGROSTAT])))
    cg.add(var.set_temperature_deviation(config[CONF_TEMPERATURE_DEVIATION]))
    cg.add(var.set_humidity_deficit(config[CONF_HUMIDITY_DEFICIT]))
    cg.add(var.set_temperature_hysteresis(config[CONF_TEMPERATURE_HYSTERESIS]))
    cg.add(var.set_humidity_hysteresis(config[CONF_HUMIDITY_HYSTERESIS]))
    cg.add(var.set_delay(config[CONF_DELAY]))
    cg.add(var.set_clear_delay(config[CONF_CLEAR_DELAY]))
    for m in config[CONF_MAX_TEMPERATURE]:
        cg.add(var.add_max_temperature(await cg.get_variable(m[CONF_SENSOR]), m[CONF_MAX]))
    cg.add(var.set_max_temperature_delay(config[CONF_MAX_TEMPERATURE_DELAY]))
    for key, (alarm, _) in ALARMS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(var.set_sensor(alarm, sens))
    for watched in config[CONF_SENSORS]:
        sens = await cg.get_variable(watched[CONF_SENSOR])
        timeout = watched.get(CONF_TIMEOUT, config[CONF_SENSOR_TIMEOUT])
        cg.add(var.add_watched_sensor(sens, timeout, watched[CONF_RAW]))
    if CONF_WEATHER in config:
        cg.add_define("USE_GECKOALARM_WEATHER")
        weather = await cg.get_variable(config[CONF_WEATHER][CONF_ID])
        cg.add(var.set_weather(weather, config[CONF_WEATHER][CONF_STALE_TIMEOUT]))
    if CONF_SENSOR_PROBLEM in config:
        cg.add(var.set_sensor_problem(await binary_sensor.new_binary_sensor(config[CONF_SENSOR_PROBLEM])))
    if CONF_SENSOR_PROBLEM_DETAILS in config:
        cg.add(var.set_sensor_problem_details(await text_sensor.new_text_sensor(config[CONF_SENSOR_PROBLEM_DETAILS])))
