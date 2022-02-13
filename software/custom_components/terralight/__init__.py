import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, sensor
from esphome.components.pid import climate
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
multi_ns = cg.esphome_ns.namespace('terralight')

DEPENDENCIES = ["sensor", "api"]

CODEOWNERS = ["@fraxinas"]
_LOGGER = logging.getLogger(__name__)

TerraLight = multi_ns.class_("TerraLight", cg.Component, sensor.Sensor)

CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(TerraLight),
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
        cv.Optional("skycond_attenuation"): cv.declare_id(sensor.Sensor)
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))
