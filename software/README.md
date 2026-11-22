# Geckocontroller Software

## Home Assistant Setup
Using esphome, the Geckocontroller shows up as a device in your Home Assistant automatically

### Historic Weather
* To create varied simulated weather conditions based on real weather records from the natural habitat of the inhabitant
* Living on the opposite hemisphere, it is reasonable to use weather records from half a year ago
* There was no Home Assistant component which can provide historic weather data, so I wrote one: https://codeberg.org/Fraxinas/historic_weather
* add the following definition to your Home Assistant's `configuration.yaml` under `sensor:`:
```
  - platform: historic_weather
    location: new_caledonia/ile_des_pins
    filename: ile_des_pins_weather_2021.json
    offset_days: 182
    offset_hours: 1
```
* the component will automatically download and cache a week ahead of data from Open-Meteo.com's historic weather API
* the controller uses these sensors of it, each named `sensor.<name>_in_new_caledonia_ile_des_pins_182_days_ago`:
  * `temperature`, `humidity`: the thermostat and hygrostat setpoints
  * `sky_brightness` (%): how bright the sky is, see below
  * `precipitation` (mm in the hour): sprayed as real water, 15 % of it on the 60 × 40 cm floor (see Rain Pump Water below)
  * `weather_code` (WMO): passing clouds, lightning and the console's sky
  * `windspeed`: the upper bound of the Multifan
  * `moon`: the scenic moonlight at night

#### Sky Brightness
* `sky_brightness` says how bright the sky is compared with a clear sky at the same moment: 100 % is a clear sky, an overcast day ~20–50 %
* it is worked out by the historic_weather component itself, so the controller needs no radiation sensors:
  * by day from the clearness index, the global radiation at the ground (`shortwave_radiation`) against the radiation at the top of the atmosphere (`terrestrial_radiation`), relative to a clear sky's clearness (`kt_clear`, default 0.75)
  * by night from the cloud cover (`night_cloud_dimming`, default 0.7: full cover → 30 %)
  * blended between the two around sunrise and sunset (`twilight_radiation`, default 150 W/m²)
* it holds no sun: the height of the sun over Île des Pins cancels out in the ratio. The controller applies its own simulated sun on top, so the growlight runs at sky brightness × sun elevation factor × passing clouds; at night the sky brightness dims the moonlight
* while it is missing (or under "Weather Override (test)") the typical brightness of the weather code in force stands in (`sky_brightness` in the `geckoweather` matrix of `gecko.yaml`)
* `offset_hours` must match `day_shift_hours` in `gecko.yaml`, which shifts the simulated sun by the same amount, so that the weather stays in step with the day cycle
* without Home Assistant or the Historic Weather records (connection lost, or a feed `unavailable` for 10 min) a standard day/night cycle stands in: temperature 26 °C by day / 21 °C by night, humidity 65 % / 80 %, a mainly clear sky (sky brightness and precipitation fall back to the typical values of the weather code in force) and, with neither precipitation nor weather code, a 5 s shower every morning (`fallback:` in the `geckoweather` block of `gecko.yaml`); the "Weather Fallback" sensor shows when it is active

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
