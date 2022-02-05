# Geckocontroller Software

## Home Assistant Setup
Using esphome, the Geckocontroller shows up as a device in your Home Assistant automatically

### Slider for Daylight Attenuation
* twilight attenuation is a correction factor for the daylight, which is automatically in-/de-cremented by sunrise and suset effects
* it can also be manually set with a slider from Home Assistant using this setup:
* add the following definition to your Home Assistant's `configuration.yaml` in the root level:
```
input_number:
  twilight_attenuation_input:
    name: Twilight Attenuation
    initial: 0
    min: 0
    max: 100
    step: 1
    mode: slider
```
* then specify a corresponding automation in `automation.yaml`:
```
- id: 'your-unique-id'
  alias: Write twilight attenuation value to geckocontroller
  trigger:
  - entity_id: input_number.twilight_attenuation_input
    platform: state
  condition: []
  action:
  - service: esphome.geckocontrol_control_twilight_attenuation
    data_template:
      level: '{{ trigger.to_state.state | int }}'
  mode: single
```

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
    offset_hours: 0
```
* temperature and humidity controllers still work without Home Assistant / Historic Weather component and are then using standard values

## Calibration

### HX711 Load Cell

with 0g weight (only coconut)

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
