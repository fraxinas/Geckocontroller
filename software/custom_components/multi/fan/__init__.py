import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_OUTPUT,
    CONF_ID
)
from .. import multi_ns

DEPENDENCIES = ["fan"]

CODEOWNERS = ["@fraxinas"]

MultiFan = multi_ns.class_("MultiFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MultiFan),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    output_ = await cg.get_variable(config[CONF_OUTPUT])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], output_)
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
