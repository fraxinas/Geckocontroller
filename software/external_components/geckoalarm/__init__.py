import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, climate
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM

CODEOWNERS = ["@fraxinas"]
DEPENDENCIES = ["climate"]
AUTO_LOAD = ["binary_sensor"]

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
    for key, (alarm, _) in ALARMS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(var.set_sensor(alarm, sens))
