import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, output, sensor
from esphome.components.pid import climate
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_NAME,
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

MultiFan = multi_ns.class_("MultiFan", sensor.Sensor, cg.Component, output.FloatOutput)

CONF_FAN_ID = 'fan_id'
CONF_HYGRO = 'hygrostat'
CONF_THERMO = 'thermostat'
CONF_VICINITY = 'vicinity'

PidConf = multi_ns.struct("PidConf")

def get_item_schema(item_type):
    return cv.Schema({
        cv.Required(CONF_ID): cv.use_id(climate.PIDClimate),
        cv.Optional(CONF_WEIGHT, default=1.0): cv.positive_float,
    })
ITEM_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(climate.PIDClimate),
    cv.Optional(CONF_WEIGHT, default=1.0): cv.positive_float,
}).extend(cv.polling_component_schema("10s"))

CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_FAN_ID): cv.declare_id(fan.FanState),
        cv.GenerateID(CONF_ID): cv.declare_id(MultiFan),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Required(CONF_HYGRO): cv.ensure_list(ITEM_SCHEMA),
        cv.Required(CONF_THERMO): cv.ensure_list(ITEM_SCHEMA),
        cv.Optional(CONF_VICINITY): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_WIND_SPEED): cv.use_id(sensor.Sensor)
    }
).extend(cv.COMPONENT_SCHEMA)

ITEMLIST = (CONF_HYGRO, CONF_THERMO)

async def to_code(config):
    fan_config = {
        CONF_ID: config[CONF_FAN_ID],
        CONF_NAME: config[CONF_NAME]+'_fanstate',
        CONF_DISABLED_BY_DEFAULT: config[CONF_DISABLED_BY_DEFAULT]
    }

    fan_state = cg.new_Pvariable(fan_config[CONF_ID])
    output_ = await cg.get_variable(config[CONF_OUTPUT])
    await fan.register_fan(fan_state, fan_config)
    var = cg.new_Pvariable(config[CONF_ID], fan_state, output_)
    await cg.register_component(var, config)

    for (conf, item) in zip([config.get(item, []) for item in ITEMLIST], ITEMLIST):
        conf = conf[0]
        item_id = await cg.get_variable(conf[CONF_ID])

        struct = cg.StructInitializer(
            PidConf,
            ('component', item_id),
            ('weight', conf[CONF_WEIGHT])
        )

        if item == CONF_HYGRO:
            cg.add(var.set_hygro(struct))

        elif item == CONF_THERMO:
            cg.add(var.set_thermo(struct))

    for item, func in [(CONF_VICINITY, var.set_vicinity), (CONF_WIND_SPEED, var.set_windspeed)]:
        if item in config:
            item = await cg.get_variable(config[item])
            cg.add(func(item))
