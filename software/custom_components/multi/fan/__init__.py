import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output
from esphome.components.pid import climate
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_OUTPUT,
    CONF_WEIGHT,
    CONF_WIND_SPEED,
    CONF_ID
)
from .. import multi_ns

DEPENDENCIES = ["fan", "climate"]

CODEOWNERS = ["@fraxinas"]
_LOGGER = logging.getLogger(__name__)

MultiFan = multi_ns.class_("MultiFan", cg.Component)

CONF_HYGRO = 'hygrostat'
CONF_THERMO = 'thermostat'
CONF_TOTAL = 'total'

PidConf = multi_ns.struct("PidConf")

UNIT_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(climate.PIDClimate),
    cv.Optional(CONF_WEIGHT, default=1.0): cv.positive_float,
})

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MultiFan),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_WIND_SPEED): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_HYGRO): cv.ensure_list(UNIT_SCHEMA),
        cv.Optional(CONF_THERMO): cv.ensure_list(UNIT_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    output_ = await cg.get_variable(config[CONF_OUTPUT])
    state = await fan.create_fan_state(config)
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], state, output_)
    await cg.register_component(var, config)

    for conf in config.get(CONF_HYGRO, []):
    #[config.get(unit, []) for unit in (CONF_HYGRO, CONF_THERMO)]:

        _LOGGER.info("multifan to_code conf = %s", str(conf))
        unit_id = await cg.get_variable(conf[CONF_ID])

        if CONF_WEIGHT in conf:
            unit_weight = await cg.get_variable(conf[CONF_WEIGHT])

        unit = cg.StructInitializer(
            PidConf,
            ('id', unit_id),
            ('weight', unit_weight)
        )

        cg.add(var.set_hygro(unit))
