# Geckocontroller Software

## Home Assistant Setup
Using esphome, the Geckocontroller shows up as a device in your Home Assistant automatically

### Historic Weather
* To create varied simulated weather conditions based on real weather records from the natural habitat of the inhabitant
* Living on the opposite hemisphere, it is reasonable to use weather records from half a year ago
* There was no Home Assistant component which can provide historic weather data, so I wrote one: https://github.com/fraxinas/historic_weather
* save a historic weather table file running https://github.com/fraxinas/historic_weather_getter/blob/main/historic_weather_getter.py with this command:
* $ `./historic_weather_getter.py 2021 france/noumea > noumea_weather_2021.json`
* place the historic weather table file in the config directory (same as your `configuration.yaml`)
* add the following definition to your Home Assistant's `configuration.yaml` under `sensor:`:
```
  - platform: historic_weather
    location: france/noumea
    filename: noumea_weather_2021.json
    offset_days: 182
    offset_hours: 1
```
* `offset_hours` must match `day_shift_hours` in `gecko.yaml`, which shifts the simulated sun by the same amount, so that the weather stays in step with the day cycle
* without Home Assistant or the Historic Weather records (connection lost, or a feed `unavailable` for 10 min) a standard day/night cycle stands in: temperature 26 °C by day / 21 °C by night, humidity 65 % / 80 %, a mainly clear sky and a 5 s shower every morning (`fallback:` in the `geckoweather` block of `gecko.yaml`); the "Weather Fallback" sensor shows when it is active

## Calibration

### Rain Pump Reservoir

The rain pump is fed from a 1 l bottle and must not run dry. `reservoir_runtime` of the `geckorain` output in `gecko.yaml` is the pump runtime one filling lasts (`0s` disables the tracking). Measure it instead of guessing:

* fill the bottle and switch on "Reservoir Calibration": the pump runs continuously
* switch it off as soon as the pump draws air: 95 % of the measured time becomes the reservoir runtime, shown as "Reservoir Runtime" and kept in flash (until `reservoir_runtime` is edited in the YAML)
* "Reservoir Level" shows the remaining fill in %; at 0 % the pump is locked out and "Rain Automation" switches off
* after refilling, press "Reservoir refilled": the level returns to 100 % and the automation switches back on

### HX711 Load Cell

with 0g weight (only nest)

| RawVal | Diff |
| :----- | :--- |
| 456504 |  162 |
| 456860 | -194 |
| 456719 |  -53 |
| 456581 |   85 |
| 456666 |    - |

with 100g reference weight

| RawVal | Diff |
| :----- | :--- |
| 249512 |  488 |
| 250099 |  -99 |
| 250383 | -383 |
| 250006 |   -6 |
| 250000 |    - |

* Δ = 100 / (RawVal<sub>0</sub> - RawVal<sub>100</sub>) * Diff<sub>max</sub> =
* 100 / (456666 - 250000) * 488 = 0,236129793967077 ≈ 0,2
