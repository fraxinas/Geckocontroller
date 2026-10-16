import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, globals
from esphome.components.pid import climate as pid_climate
from esphome.const import CONF_ID

CODEOWNERS = ["@fraxinas"]

geckopid_ns = cg.esphome_ns.namespace("geckopid")
GeckoPid = geckopid_ns.class_("GeckoPid", cg.Component)

CONF_THERMOSTAT = "thermostat"
CONF_HYGROSTAT = "hygrostat"
CONF_VICINITY = "vicinity"
CONF_CALIBRATION_TARGET = "calibration_target"
CONF_IS_DAYTIME = "is_daytime"
CONF_NOISEBAND = "noiseband"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GeckoPid),
        cv.Required(CONF_THERMOSTAT): cv.use_id(pid_climate.PIDClimate),
        cv.Required(CONF_HYGROSTAT): cv.use_id(pid_climate.PIDClimate),
        cv.Required(CONF_VICINITY): cv.use_id(sensor.Sensor),
        cv.Required(CONF_CALIBRATION_TARGET): cv.use_id(globals.GlobalsComponent),
        # `is_daytime` is declared as GlobalsComponent for ID validation even though its runtime
        # type is RestoringGlobalsComponent (restore_value: yes); the C++ setter takes the
        # concrete RestoringGlobalsComponent<bool>* that get_variable yields.
        cv.Required(CONF_IS_DAYTIME): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_NOISEBAND, default=0.5): cv.positive_float,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_thermostat(await cg.get_variable(config[CONF_THERMOSTAT])))
    cg.add(var.set_hygrostat(await cg.get_variable(config[CONF_HYGROSTAT])))
    cg.add(var.set_vicinity_sensor(await cg.get_variable(config[CONF_VICINITY])))
    cg.add(var.set_calibration_target(await cg.get_variable(config[CONF_CALIBRATION_TARGET])))
    cg.add(var.set_is_daytime(await cg.get_variable(config[CONF_IS_DAYTIME])))
    cg.add(var.set_noiseband(config[CONF_NOISEBAND]))
