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

