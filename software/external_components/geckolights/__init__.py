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
CONF_SKYCOND_ATTENUATION = "skycond_attenuation"
CONF_IS_DAYTIME = "is_daytime"
CONF_CALIBRATION_TARGET = "calibration_target"
CONF_ENABLED = "enabled"
CONF_MOON = "moon"
CONF_WEATHER = "weather"
CONF_SUN_EFFECT_DELAY = "sun_effect_delay"
CONF_SUNRISE_ELEVATION = "sunrise_elevation"
CONF_SUNSET_ELEVATION = "sunset_elevation"
CONF_DAY_END_ELEVATION = "day_end_elevation"
CONF_ILLUMINATION = "illumination"
CONF_SUN = "sun"
CONF_GROWLIGHT = "growlight"
CONF_GROWLIGHT_LIGHT = "growlight_light"
CONF_PERIOD = "period"
CONF_SPLIT = "split"
CONF_FULL_ELEVATION = "full_elevation"
CONF_SENSOR = "sensor"
CONF_QUIET_HOURS = "quiet_hours"
CONF_TIME = "time"
CONF_START = "start"
CONF_FACTOR = "factor"



# The visible-illumination controller: weather illumination x sun elevation, rendered on the
# growlight strip with the scenic RGB as a white fill. See update_illumination_ in the .cpp.
#
# The growlight is given as exactly one of:
#   `growlight`       a switched output (gpio/relay), which the component slow-PWMs over `period`.
#   `growlight_light` a dimmable light entity, driven straight to the duty as a brightness with no
#                     chopping. Going through the light (rather than its output) keeps its state
#                     in HA true and runs its own on_state/on_turn_off automations (relay power and
#                     the like); the component only issues a light call when the level moved.
ILLUMINATION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_SUN): cv.use_id(sun.Sun),
            cv.Optional(CONF_GROWLIGHT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_GROWLIGHT_LIGHT): cv.use_id(light.LightState),
            cv.Optional(CONF_PERIOD, default="5min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SPLIT, default="50%"): cv.percentage,
            cv.Optional(CONF_FULL_ELEVATION, default=45.0): cv.float_range(min=1.0, max=90.0),
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    ),
    cv.has_exactly_one_key(CONF_GROWLIGHT, CONF_GROWLIGHT_LIGHT),
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
        # The sun-driven day, from the illumination `sun` (degrees): the "Sunrise Color" sweep
        # starts as the rising sun passes `sunrise_elevation`, and the day -- the daylight lamp
        # heating -- begins once it has played. "Sunset Color" starts as the setting sun passes
        # `sunset_elevation`, and the night begins below `day_end_elevation`.
        cv.Optional(CONF_SUNRISE_ELEVATION, default=5.0): cv.float_range(min=-18.0, max=40.0),
        cv.Optional(CONF_SUNSET_ELEVATION, default=5.0): cv.float_range(min=-18.0, max=40.0),
        cv.Optional(CONF_DAY_END_ELEVATION, default=0.0): cv.float_range(min=-18.0, max=40.0),
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
    cg.add(
        var.set_day_elevations(
            config[CONF_SUNRISE_ELEVATION], config[CONF_SUNSET_ELEVATION], config[CONF_DAY_END_ELEVATION]
        )
    )
    if CONF_QUIET_HOURS in config:
        q = config[CONF_QUIET_HOURS]
        cg.add(var.set_quiet_time(await cg.get_variable(q[CONF_TIME])))
        cg.add(var.set_quiet_start(q[CONF_START]["hour"] * 60 + q[CONF_START]["minute"]))
        cg.add(var.set_quiet_factor(q[CONF_FACTOR]))
    if CONF_ILLUMINATION in config:
        il = config[CONF_ILLUMINATION]
        cg.add(var.set_sun(await cg.get_variable(il[CONF_SUN])))
        if CONF_GROWLIGHT_LIGHT in il:
            cg.add(var.set_growlight_light(await cg.get_variable(il[CONF_GROWLIGHT_LIGHT])))
        else:
            cg.add(var.set_growlight_binary(await cg.get_variable(il[CONF_GROWLIGHT])))
        cg.add(var.set_illumination_period(il[CONF_PERIOD]))
        cg.add(var.set_illumination_split(il[CONF_SPLIT]))
        cg.add(var.set_illumination_full_elevation(il[CONF_FULL_ELEVATION]))
        if CONF_SENSOR in il:
            cg.add(var.set_illumination_sensor(await cg.get_variable(il[CONF_SENSOR])))
