# Geckocontroller Hardware

## Calculations

### ACT4088 Feedback Resistors

with
* V<sub>out</sub> = 3.3V
* R<sub>FB1</sub> chosen 47k

* R<sub>FB2</sub> = R<sub>FB1</sub> / ((V<sub>out</sub> / 0.81) - 1) = 15289.156626506

Values are:
* `R103` = 47k
* `R101` = 15k
* `R102` = 287R

### HX711 Reference Voltage Resistors

with
* AVDD should be 100mV below VCC, so 3.20V
* AVDD = V<sub>BG</sub> * ( R1 + R2 ) / R2
* R1 chosen 47k

Values are:
* `R61` = 47k
* `R62` = 30k1

### FAN PWM integrator switching
* usually PWM fans can't be reliably attenuated to 0 rpm and need to have their Vcc switched in order to turn them off completely
* so to save one extra GPIO output for switching the fan power, this circuit will integrate the PWM signal and simply disconnect the fan's 12V power if the PWM duty cycle is near 0

http://www.falstad.com/circuit/circuitjs.html?ctz=CQAgjCAMB0l3YBMBWEizWQTgMxYBwDsiOYkAbGACxIhWEirKSMCmAtGGAFADmdVfGkRUBQxMShRuAYxBZyIcqIXDRLRNETwduuIg5D2m-PmZIiiLIRz4wOchxyIosSDwBOIdoKWjnflJkcNwAZiA4DnRCDkK+keCYrpCI3F6Ipt5cLhlxQizk+PBp4ISKyqWKiCJB5DrcAO4RhREusd4BkNwAbmJqfZy0LCxULLRDrsiNfdX+LbPSTe2DLlS+K9Je7WBYbS3ZUszT7RXbQ8f7u83iNV1LLewBywd3aJkbz+dNudERUb5dcJgMqBEQFfwuDBUVDaaZgkBFNCjfqveG+H4A6aqBbAqq3aa4-rwhao5ELdokgkgiqqCqvWmiRF07gAJW8FWoonYHJBGjQzDgEWgOCFIuGsCmAGd2aIKtzRJygiBQgBDAA2ktY3AAJq1AkzRC5taxVQBXNUAF2m8sCPiEzIA9kjAiNIFgsOBxTprORkOQUFIcv5uE6wKg6XQ3R7kTBvWU-QGIJDUDgQxEXZH3ZHkvAfQmYWh06nQ+CpKMs7H4GUrHhoaQA5W4Hn-SnC2QItwgA
