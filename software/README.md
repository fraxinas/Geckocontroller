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
* add the following definitions to your Home Assistant's `configuration.yaml` under `sensor:`:
```
  - platform: historic_weather
    location: france/noumea
    filename: noumea_weather_2021.json
    offset_days: 182
    offset_hours: 0

  - platform: template
    sensors:
      temperature_noumea:
        value_template: "{{ states('sensor.temperature_in_france_noumea_182_days_ago') | default('24') }}"
        friendly_name: Temperature in Noumea half a year ago
      humidity_noumea:
        value_template: "{{ states('sensor.humidity_in_france_noumea_182_days_ago')| default('75') }}"
        friendly_name: Humidity in Noumea half a year ago
      rain_noumea:
        value_template: "{{ states('sensor.rain_in_france_noumea_182_days_ago')| default('0') }}"
        friendly_name: Rain in Noumea half a year ago
      sky_noumea:
        value_template: "{{ states('sensor.sky_in_france_noumea_182_days_ago')| default('0') }}"
        friendly_name: Sky condition in Noumea half a year ago
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
