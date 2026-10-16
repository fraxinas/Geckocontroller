import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output, globals, sensor, sun
from esphome.components import time as time_
from esphome.components.pid import climate as pid_climate
from ..geckoweather import GeckoWeather
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
# geckolights.h includes geckoweather.h unconditionally (the resolved-weather struct it subscribes
# to), so the weather component must be configured even though the `weather:` key itself is
# optional. Declaring it here turns a missing block into a config error instead of a C++ one.
DEPENDENCIES = ["geckoweather"]

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
CONF_ENABLED = "enabled"
CONF_MOON = "moon"
CONF_WEATHER = "weather"
CONF_SUN_EFFECT_DELAY = "sun_effect_delay"
CONF_ILLUMINATION = "illumination"
CONF_SUN = "sun"
CONF_GROWLIGHT = "growlight"
CONF_DIMMABLE = "dimmable"
CONF_PERIOD = "period"
CONF_SPLIT = "split"
CONF_FULL_ELEVATION = "full_elevation"
CONF_SENSOR = "sensor"
CONF_QUIET_HOURS = "quiet_hours"
CONF_TIME = "time"
CONF_START = "start"
CONF_FACTOR = "factor"


CONF_SUN_RISE_MAX = "sun_rise_max"

# The visible-illumination controller: weather illumination x sun elevation, rendered on the
# growlight strip with the scenic RGB as a white fill. See update_illumination_ in the .cpp.
#
# `dimmable` says what kind of output `growlight` is. false (the default) = a switched output
# (gpio/relay), which the component slow-PWMs over `period`. true = a dimmable FloatOutput (ledc,
# slow_pwm, ...), driven straight to the duty as a level with no chopping -- point `growlight` at
# such an output and flip this, nothing else changes. Setting it on an output that is only binary
# is caught as a C++ type error on the set_growlight_float call.
ILLUMINATION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SUN): cv.use_id(sun.Sun),
        cv.Required(CONF_GROWLIGHT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_DIMMABLE, default=False): cv.boolean,
        cv.Optional(CONF_PERIOD, default="5min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SPLIT, default="50%"): cv.percentage,
        cv.Optional(CONF_FULL_ELEVATION, default=45.0): cv.float_range(min=1.0, max=90.0),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
)

# Sleeping hours. From `start` on the LOCAL wall clock (this is about the people in the room, not
# the simulated Noumea weather feed) until the night ends at sunrise, the scenic moonlight is scaled
# by `factor` so the terrarium does not glow into a dark room.
QUIET_HOURS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TIME): cv.use_id(time_.RealTimeClock),
        cv.Optional(CONF_START, default="23:30:00"): cv.time_of_day,
        cv.Optional(CONF_FACTOR, default="50%"): cv.percentage,
    }
)

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
        cv.Optional(CONF_ENABLED): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_MOON): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_WEATHER): cv.use_id(GeckoWeather),
        cv.Optional(CONF_SUN_EFFECT_DELAY, default=180): cv.uint16_t,
        cv.Optional(CONF_SUN_RISE_MAX, default=0.75): cv.positive_float,
        cv.Optional(CONF_ILLUMINATION): ILLUMINATION_SCHEMA,
        cv.Optional(CONF_QUIET_HOURS): QUIET_HOURS_SCHEMA,
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
    if CONF_ENABLED in config:
        cg.add(var.set_enabled(await cg.get_variable(config[CONF_ENABLED])))
    if CONF_MOON in config:
        cg.add(var.set_moon_sensor(await cg.get_variable(config[CONF_MOON])))
    if CONF_WEATHER in config:
        cg.add(var.set_weather(await cg.get_variable(config[CONF_WEATHER])))
    cg.add(var.set_sun_effect_delay(config[CONF_SUN_EFFECT_DELAY]))
    cg.add(var.set_sun_rise_max(config[CONF_SUN_RISE_MAX]))
    if CONF_QUIET_HOURS in config:
        q = config[CONF_QUIET_HOURS]
        cg.add(var.set_quiet_time(await cg.get_variable(q[CONF_TIME])))
        cg.add(var.set_quiet_start(q[CONF_START]["hour"] * 60 + q[CONF_START]["minute"]))
        cg.add(var.set_quiet_factor(q[CONF_FACTOR]))
    if CONF_ILLUMINATION in config:
        il = config[CONF_ILLUMINATION]
        cg.add(var.set_sun(await cg.get_variable(il[CONF_SUN])))
        grow = await cg.get_variable(il[CONF_GROWLIGHT])
        if il[CONF_DIMMABLE]:
            cg.add(var.set_growlight_float(grow))
        else:
            cg.add(var.set_growlight_binary(grow))
        cg.add(var.set_illumination_period(il[CONF_PERIOD]))
        cg.add(var.set_illumination_split(il[CONF_SPLIT]))
        cg.add(var.set_illumination_full_elevation(il[CONF_FULL_ELEVATION]))
        if CONF_SENSOR in il:
            cg.add(var.set_illumination_sensor(await cg.get_variable(il[CONF_SENSOR])))
