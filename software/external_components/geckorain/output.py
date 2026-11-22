import zlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, sensor, globals
from esphome.components.pid import climate as pid_climate
from esphome.const import CONF_ID, CONF_PERIOD

from . import geckorain_ns

DEPENDENCIES = ["output"]

CONF_PUMP = "pump"
CONF_FLOW = "flow"
CONF_MIN_SPRAY = "min_spray"
CONF_MAX_SPRAY = "max_spray"
CONF_WEATHER = "weather"
CONF_FLOOR_AREA = "floor_area"
CONF_RAIN_SHARE = "rain_share"
CONF_MAX_RAIN = "max_rain"
CONF_RAIN_INTERVAL = "rain_interval"
CONF_MAX_MISTING = "max_misting"
CONF_HUMIDITY = "humidity"
CONF_HYGROSTAT = "hygrostat"
CONF_CALIBRATION_TARGET = "calibration_target"
CONF_ENABLED = "enabled"
CONF_RESERVOIR_VOLUME = "reservoir_volume"
CONF_RESERVE = "reserve"
CONF_RESERVOIR_LEVEL = "reservoir_level"
CONF_FLOW_SENSOR = "flow_sensor"
CONF_VOLUME_SENSOR = "volume_sensor"
CONF_CALIBRATION_SENSOR = "calibration_sensor"
CONF_CALIBRATION_LIMIT = "calibration_limit"

RainPumpOutput = geckorain_ns.class_("RainPumpOutput", output.FloatOutput, cg.Component)


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(RainPumpOutput),
        cv.Required(CONF_PUMP): cv.use_id(output.BinaryOutput),
        # Slow-PWM period of the humidify calibration, where the PID relay drives the pump directly.
        cv.Optional(CONF_PERIOD, default="180s"): cv.positive_time_period_milliseconds,
        # The pump's flow through the spray nozzles (ml/s), until calibrated on the device; the water
        # is in proportion to the on-time. No pulse shorter than `min_spray`, no rain pulse longer
        # than `max_spray`.
        cv.Optional(CONF_FLOW, default=2.66): cv.positive_float,
        cv.Optional(CONF_MIN_SPRAY, default="200ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_SPRAY, default="3s"): cv.positive_time_period_milliseconds,
        # The weather's precipitation in mm in the hour. Rain is this water on `floor_area` (m²),
        # 1 mm on 1 m² = 1 l, of which `rain_share` is sprayed; heavier than `max_rain` (mm in the
        # hour) counts as that. At max_rain a pulse every `rain_interval`, lighter rain
        # sqrt(max_rain / mm) times further apart, each pulse the water of its interval.
        cv.Optional(CONF_WEATHER): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_FLOOR_AREA, default=0.24): cv.positive_float,
        cv.Optional(CONF_RAIN_SHARE, default="15%"): cv.percentage,
        cv.Optional(CONF_MAX_RAIN, default=7.6): cv.positive_float,
        cv.Optional(CONF_RAIN_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        # Misting: at most this many ml an hour (at full humidify demand), in pulses of min_spray.
        cv.Optional(CONF_MAX_MISTING, default=120.0): cv.positive_float,
        cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HYGROSTAT): cv.use_id(pid_climate.PIDClimate),
        cv.Optional(CONF_CALIBRATION_TARGET): cv.use_id(globals.GlobalsComponent),
        cv.Optional(CONF_ENABLED): cv.use_id(globals.GlobalsComponent),
        # Litres the pump draws from one reservoir filling until calibrated; 0 disables reservoir
        # tracking. Once only `reserve` of it is left, the reservoir counts as empty.
        cv.Optional(CONF_RESERVOIR_VOLUME, default=0.0): cv.positive_float,
        cv.Optional(CONF_RESERVE, default="10%"): cv.percentage,
        cv.Optional(CONF_RESERVOIR_LEVEL): cv.use_id(sensor.Sensor),
        # The flow and reservoir volume in force (calibrated or from the YAML).
        cv.Optional(CONF_FLOW_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_VOLUME_SENSOR): cv.use_id(sensor.Sensor),
        # Flow calibration: the reservoir pumped empty continuously; its on-time so far, and past
        # the limit it ends by itself (dry-run guard).
        cv.Optional(CONF_CALIBRATION_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CALIBRATION_LIMIT, default="10min"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cg.add(var.set_pump(await cg.get_variable(config[CONF_PUMP])))
    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(var.set_flow(config[CONF_FLOW]))
    cg.add(var.set_min_spray(config[CONF_MIN_SPRAY]))
    cg.add(var.set_max_spray(config[CONF_MAX_SPRAY]))
    cg.add(var.set_floor_area(config[CONF_FLOOR_AREA]))
    cg.add(var.set_rain_share(config[CONF_RAIN_SHARE]))
    cg.add(var.set_max_rain(config[CONF_MAX_RAIN]))
    cg.add(var.set_rain_interval(config[CONF_RAIN_INTERVAL]))
    cg.add(var.set_max_misting(config[CONF_MAX_MISTING]))
    cg.add(var.set_reservoir_volume(config[CONF_RESERVOIR_VOLUME]))
    cg.add(var.set_reserve(config[CONF_RESERVE]))
    cg.add(var.set_calibration_limit(config[CONF_CALIBRATION_LIMIT]))
    # A stored calibration only holds for the flow and volume it replaced.
    model = (config[CONF_FLOW], config[CONF_RESERVOIR_VOLUME])
    cg.add(var.set_config_hash(zlib.crc32(repr(model).encode())))

    for key, setter in [
        (CONF_WEATHER, var.set_weather_sensor),
        (CONF_HUMIDITY, var.set_humidity_sensor),
        (CONF_HYGROSTAT, var.set_hygrostat),
        (CONF_CALIBRATION_TARGET, var.set_calibration_target),
        (CONF_ENABLED, var.set_enabled),
        (CONF_RESERVOIR_LEVEL, var.set_reservoir_level_sensor),
        (CONF_FLOW_SENSOR, var.set_flow_sensor),
        (CONF_VOLUME_SENSOR, var.set_volume_sensor),
        (CONF_CALIBRATION_SENSOR, var.set_calibration_sensor),
    ]:
        if key in config:
            cg.add(setter(await cg.get_variable(config[key])))
