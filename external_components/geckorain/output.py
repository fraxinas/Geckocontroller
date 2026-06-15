import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_PERIOD

from . import geckorain_ns

DEPENDENCIES = ["output"]

CONF_PUMP = "pump"

RainPumpOutput = geckorain_ns.class_("RainPumpOutput", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(RainPumpOutput),
        cv.Required(CONF_PUMP): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_PERIOD, default="180s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pump = await cg.get_variable(config[CONF_PUMP])
    cg.add(var.set_pump(pump))
    cg.add(var.set_period(config[CONF_PERIOD]))
