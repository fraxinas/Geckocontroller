# Geckocontroller Software

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
