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

### Rain Pump Water

The rain pump sprays rain and the misting for humidity, fed from a 1 l bottle that must not run dry. Everything it does is reckoned in water, at the pump's `flow` through the spray nozzles (`geckorain` output in `gecko.yaml`): the water sprayed is in proportion to the pump's on-time. No pulse is shorter than `min_spray` (200 ms), no rain pulse longer than `max_spray` (3 s).

With a MistKing diaphragm misting pump MKP-125-50 with one Value T double misting assembly and one Value L misting assembly attached, the flow is 2.66 ml/s, and the pump draws 970 ml from one filling of the bottle (its intake sits a few cm above the bottom).

Flow and the usable reservoir volume are calibrated on the device and kept in flash ("Pump Flow", "Reservoir Volume"); the YAML values stand in until then. Calibrate with the spray nozzles attached (the flow through them is far lower than the pump's open flow):

* fill the bottle as usual, hold measuring cups under the nozzles and switch on "Reservoir Calibration": 5 s later the pump starts and runs continuously ("Reservoir Calibration Runtime"), everything else held off; with the nozzles in the terrarium the cups are held by hand for the run (about 6 min)
* press "Reservoir empty" as soon as the pump draws air: the pump stops and the runtime is kept
* enter the ml in the measuring cups (all together) as "Reservoir Calibration Result": that over the runtime becomes the flow, and the ml themselves the "Reservoir Volume" -- what the pump really draws from one filling
* switching "Reservoir Calibration" off instead aborts, nothing changes; after `calibration_limit` (10 min) the run ends by itself and the reservoir counts as empty
* refill the bottle and press "Reservoir refilled"

Rain: the weather's precipitation (mm in the hour) on the 60 × 40 cm floor, 15 % of it (`rain_share`), capped at `max_rain`. Heavier rain comes in bigger pulses and more often: at `max_rain` a pulse every `rain_interval` (30 s), lighter rain √(max_rain / mm) times further apart, each pulse the water of its interval, its length at the flow within `min_spray` .. `max_spray`. Misting sprays at most `max_misting` (ml per hour, at full humidify demand) in pulses of `min_spray`.

The reservoir (`reservoir_volume`) is tracked at the same flow; every pump run (rain, misting, "Let it rain") is deducted from it:

* "Reservoir Level" shows the remaining fill in %; once only `reserve` (10 %) is left, the pump is locked out and "Rain Automation" switches off. "Reservoir empty" does the same right away
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
