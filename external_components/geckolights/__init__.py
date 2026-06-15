import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output, globals
from esphome.components.pid import climate as pid_climate
from esphome.const import (
    CONF_ID,
    CONF_OUTPUT,
    CONF_OUTPUT_ID,
    CONF_RED,
    CONF_GREEN,
    CONF_BLUE,
)

CODEOWNERS = ["@fraxinas"]
AUTO_LOAD = ["light", "output"]

geckolights_ns = cg.esphome_ns.namespace("geckolights")
GeckoLights = geckolights_ns.class_("GeckoLights", cg.Component, output.FloatOutput)
GeckoBrightnessOutput = geckolights_ns.class_("GeckoBrightnessOutput", light.LightOutput)
GeckoRGBOutput = geckolights_ns.class_("GeckoRGBOutput", light.LightOutput)

CONF_HEAT_LAMP = "heat_lamp"
CONF_DAY_LIGHT = "day_light"
CONF_SCENIC_LIGHT = "scenic_light"
CONF_THERMOSTAT = "thermostat"
CONF_TWILIGHT_ATTENUATION = "twilight_attenuation"
CONF_SKYCOND_ATTENUATION = "skycond_attenuation"
CONF_IS_DAYTIME = "is_daytime"
CONF_CALIBRATION_TARGET = "calibration_target"
CONF_SUN_EFFECT_DELAY = "sun_effect_delay"
CONF_SUN_RISE_MAX = "sun_rise_max"

MONO_LIGHT_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(GeckoBrightnessOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    }
)

RGB_LIGHT_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(GeckoRGBOutput),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GeckoLights),
        cv.Required(CONF_HEAT_LAMP): MONO_LIGHT_SCHEMA,
        cv.Required(CONF_DAY_LIGHT): MONO_LIGHT_SCHEMA,
        cv.Required(CONF_SCENIC_LIGHT): RGB_LIGHT_SCHEMA,
        cv.Required(CONF_THERMOSTAT): cv.use_id(pid_climate.PIDClimate),
        cv.Required(CONF_TWILIGHT_ATTENUATION): cv.use_id(globals.GlobalsComponent),
        cv.Required(CONF_SKYCOND_ATTENUATION): cv.use_id(globals.GlobalsComponent),
        # `is_daytime` is declared as GlobalsComponent for ID validation even though its
        # runtime type is RestoringGlobalsComponent (restore_value: yes); the C++ setter
        # takes the concrete RestoringGlobalsComponent<bool>* that get_variable yields.
        cv.Required(CONF_IS_DAYTIME): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_CALIBRATION_TARGET): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_SUN_EFFECT_DELAY, default=180): cv.uint16_t,
        cv.Optional(CONF_SUN_RISE_MAX, default=0.75): cv.positive_float,
    }
).extend(cv.COMPONENT_SCHEMA)


async def _register_mono_light(parent_setter, conf):
    out = cg.new_Pvariable(conf[CONF_OUTPUT_ID])
    await light.register_light(out, conf)
    cg.add(out.set_output(await cg.get_variable(conf[CONF_OUTPUT])))
    cg.add(parent_setter(await cg.get_variable(conf[CONF_ID])))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await _register_mono_light(var.set_heat_lamp, config[CONF_HEAT_LAMP])
    await _register_mono_light(var.set_day_light, config[CONF_DAY_LIGHT])

    sc = config[CONF_SCENIC_LIGHT]
    sc_out = cg.new_Pvariable(sc[CONF_OUTPUT_ID])
    await light.register_light(sc_out, sc)
    cg.add(sc_out.set_red(await cg.get_variable(sc[CONF_RED])))
    cg.add(sc_out.set_green(await cg.get_variable(sc[CONF_GREEN])))
    cg.add(sc_out.set_blue(await cg.get_variable(sc[CONF_BLUE])))
    cg.add(var.set_scenic_light(await cg.get_variable(sc[CONF_ID])))

    cg.add(var.set_thermostat(await cg.get_variable(config[CONF_THERMOSTAT])))
    cg.add(var.set_twilight_attenuation(await cg.get_variable(config[CONF_TWILIGHT_ATTENUATION])))
    cg.add(var.set_skycond_attenuation(await cg.get_variable(config[CONF_SKYCOND_ATTENUATION])))
    cg.add(var.set_is_daytime(await cg.get_variable(config[CONF_IS_DAYTIME])))
    if CONF_CALIBRATION_TARGET in config:
        cg.add(var.set_calibration_target(await cg.get_variable(config[CONF_CALIBRATION_TARGET])))
    cg.add(var.set_sun_effect_delay(config[CONF_SUN_EFFECT_DELAY]))
    cg.add(var.set_sun_rise_max(config[CONF_SUN_RISE_MAX]))
