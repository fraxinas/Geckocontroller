import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, sensor, number
from esphome.const import (
    CONF_ATTRIBUTE,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_ID,
    CONF_NAME,
    CONF_OUTPUT_ID,
    CONF_OUTPUT,
    CONF_WIND_SPEED,
    CONF_ID,
    STATE_CLASS_MEASUREMENT
)
terra_ns = cg.esphome_ns.namespace('terralight')

DEPENDENCIES = ["sensor", "api"]

CODEOWNERS = ["@fraxinas"]
_LOGGER = logging.getLogger(__name__)

CONF_TWILIGHT = "twilight_attenuation"
TerraLight = terra_ns.class_("TerraLight", sensor.Sensor, cg.Component)
TerraLightNumber = terra_ns.class_("TerraLightNumber", number.Number, cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(number.NUMBER_SCHEMA
).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(TerraLight),
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
        cv.Optional("skycond_attenuation"): cv.declare_id(sensor.Sensor),
        cv.GenerateID(CONF_TWILIGHT): cv.declare_id(TerraLightNumber)
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    print("to_code", config)
    terra_var = cg.new_Pvariable(config[CONF_ID])
    number_var = cg.new_Pvariable(config[CONF_TWILIGHT])
    await cg.register_component(terra_var, config)
    await sensor.register_sensor(terra_var, config)
    await number.register_number(
            number_var,
            config,
            min_value=0,
            max_value=100,
            step=1,
        )

    cg.add(terra_var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(terra_var.set_attribute(config[CONF_ATTRIBUTE]))
