# PID Auto-Calibration Design

Obtain `kp/ki/kd` for each climate-control process via Home Assistant, persist them, and
apply the right set per direction at runtime.

## Processes & actuators

| Process | Climate | PID dir | Actuator | Production transform bypassed while tuning |
|---|---|---|---|---|
| Heating | `thermostat_air` | heat (+) | `geckolights` | daylight-attenuation / day-night |
| Cooling | `thermostat_air` | cool (−) | `geckofan` | weighted hygro/thermo fan formula |
| Humidification | `hygrostat` | heat (+) | `geckorain` (rain pump) | weather/humidity arbitration + slow-PWM |
| Dehumidification | `hygrostat` | cool (−) | `geckofan` | weighted fan formula |

`hygrostat.heat_output` is **rewired from `fogger_humidifier` to the rain pump**; the fogger is
now used only by the "Sleepy hollow" fog button.

## Why ESPHome's stock autotune isn't enough alone
- `climate.pid.autotune` auto-applies the result to the live controller ([pid_climate.cpp](custom_components/) line ~142) but **does not persist** it, and a PID climate stores only **one** `kp/ki/kd` shared by heat+cool.
- The actuators here transform/ignore the raw PID output, so a relay test won't reach them cleanly. → a **calibration bypass** is required.

## Components

### `calibration_target` global (int, restore: no, initial 0)
`0 none · 1 heat · 2 cool · 3 humidify · 4 dehumidify`. Written by `geckopid`, read by the
three actuators to switch into direct pass-through for the targeted direction.

### Actuator calibration bypass
- **geckofan** (`MultiFan`): when `calibration_target ∈ {cool, dehumidify}`, `write_state(s)` drives `output_->set_level(clamp(s,0,1))` directly and `update_from_pid_` skips the weighted formula.
- **geckolights** (`GeckoLights`): when `calibration_target == heat`, `write_state(s)` drives the daylight (daytime) / heat lamp (night) brightness = `s` directly, no attenuation.
- **geckorain** (`RainPumpOutput`): when `calibration_target == humidify`, the arbitration below is bypassed and the relay output drives the slow-PWM directly.

### `geckorain` humidification arbitration (production, non-calibration)
The rain pump serves both historic-weather rain simulation and humidity control. Inputs:
`weather_precipitation` (mm in the hour), `hygrostat` target humidity `H*`, current humidity `humidity_combined`.
- **If `weather_precipitation > 0`** (scheduled rain): spray real water — the precipitation on the terrarium floor (`floor_area`, 1 mm on 1 m² = 1 l), `rain_share` of it, capped at `max_rain` — but **cap at `H* + 20`**: stop adding water once humidity is 20 pts above target. Heavier rain comes in bigger pulses and more often: one every `rain_interval` at `max_rain`, √(max_rain / mm) times further apart for lighter rain, each pulse the water of its interval.
- **If `weather_precipitation == 0`** and humidity is low: let humidity drift down to **`H* − 10`** before acting, then humidify gently from the hygrostat humidify demand: at most `max_misting` ml an hour at full demand, in short pulses of `min_spray`.
- All water is reckoned at the pump's calibrated flow through the nozzles, in proportion to its on-time; see `software/README.md`.

### `geckopid` component (new)
Owns calibration and the four persisted parameter sets.
- **Stored data** (ESPHome `preferences`/NVS, one blob): `{kp, ki, kd, vicinity_at_tune}` × {heat, cool, humidify, dehumidify}.
- **Boot**: load from flash, apply heat & humidify sets to the climates (cool/dehumidify applied on direction change).
- **Runtime swap** (`loop`): watch `thermostat_air.action` and `hygrostat.action`; on a direction change call `set_kp/ki/kd` with the matching stored set (HEATING→heat / COOLING→cool for the thermostat; HEATING→humidify / COOLING→dehumidify for the hygrostat).
- **Calibrate**(`target`): set `calibration_target`; ensure the climate is in an oscillation-capable mode; `start_autotune` with the relay restricted to one direction (`positive_output=0` for cool/dehumidify, `negative_output=0` for heat/humidify); **detect completion** by polling `get_kp()` (auto-applied exactly once at completion); then capture `get_kp/ki/kd`, snapshot `temperature_vicinity`, save to flash, clear `calibration_target`, disarm. A max-duration guard aborts on failure. Humidify/dehumidify use a long timeout (slow plant) — autotuned as-is.

### Home Assistant controls (template, in `gecko.yaml`)
- **select** "Calibration Target" → `Heating | Cooling | Humidification | Dehumidification` (maps to 1–4).
- **switch** "PID Calibration" → `on` calls `gecko_pid->start_calibration(selected)`; auto-resets to `off` when the run completes/aborts.
- Optional: read-only template sensors exposing the four stored sets + their `vicinity_at_tune`.

## Verification
`esphome config gecko.yaml` then `esphome compile gecko.yaml`. Functional: arm each target, watch the
`geckopid` logs report start → completion → saved `kp/ki/kd` + vicinity; reboot and confirm the values restore.
