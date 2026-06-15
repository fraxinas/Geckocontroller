import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, sensor, globals
from esphome.components.pid import climate as pid_climate
from esphome.const import CONF_ID, CONF_PERIOD

from . import geckorain_ns

DEPENDENCIES = ["output"]

CONF_PUMP = "pump"
CONF_WEATHER = "weather"
CONF_HUMIDITY = "humidity"
CONF_HYGROSTAT = "hygrostat"
CONF_CALIBRATION_TARGET = "calibration_target"

RainPumpOutput = geckorain_ns.class_("RainPumpOutput", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(RainPumpOutput),
        cv.Required(CONF_PUMP): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_PERIOD, default="180s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_WEATHER): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HYGROSTAT): cv.use_id(pid_climate.PIDClimate),
        cv.Optional(CONF_CALIBRATION_TARGET): cv.use_id(globals.GlobalsComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cg.add(var.set_pump(await cg.get_variable(config[CONF_PUMP])))
    cg.add(var.set_period(config[CONF_PERIOD]))

    for key, setter in [
        (CONF_WEATHER, var.set_weather_sensor),
        (CONF_HUMIDITY, var.set_humidity_sensor),
        (CONF_HYGROSTAT, var.set_hygrostat),
        (CONF_CALIBRATION_TARGET, var.set_calibration_target),
    ]:
        if key in config:
            cg.add(setter(await cg.get_variable(config[key])))
