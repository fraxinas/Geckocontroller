# Geckocontroller Hardware

## Calculations

### TPS54202 Buck Converter

with
* V<sub>out</sub> = 3.3V
* V<sub>ref</sub> = 0.596V
* V<sub>in,max</sub> = 13V
* f<sub>sw</sub> = 500kHz
* I<sub>out</sub> = 0.6A

#### Feedback Resistors

* R<sub>FB1</sub> chosen 100k
* R<sub>FB2</sub> = R<sub>FB1</sub> * V<sub>ref</sub> / (V<sub>out</sub> - V<sub>ref</sub>) = 22041.42
* rounded to E24: R<sub>FB2</sub> = 22k
* V<sub>out</sub> = V<sub>ref</sub> * (1 + R<sub>FB1</sub> / R<sub>FB2</sub>) = 3.305V

Values are:
* `R101` = 100k
* `R102` = 22k

#### Output Inductor

* K<sub>ind</sub> = 0.25 (inductor ripple current relative to I<sub>out</sub>)
* L<sub>min</sub> = V<sub>out</sub> * (V<sub>in,max</sub> - V<sub>out</sub>) / (V<sub>in,max</sub> * K<sub>ind</sub> * I<sub>out</sub> * f<sub>sw</sub>) = 32.8µH

Value is:
* `L101` = 33µH

#### Output Capacitor

* ΔI<sub>out</sub> = 0.6A (load step)
* ΔV<sub>out</sub> = 0.15V (allowed output voltage deviation)
* C<sub>out</sub> > 2 * ΔI<sub>out</sub> / (f<sub>sw</sub> * ΔV<sub>out</sub>) = 16µF, must be low ESR (ceramic)

Value is:
* `C101` = 22µF

### Zero-Cross Detection

with
* V<sub>mains</sub> = 230V ±10% → 253V<sub>rms,max</sub>, 50Hz
* V<sub>peak</sub> = 230V * √2 = 325V (358V at +10%)
* V<sub>F,LED</sub> ≈ 1.4V (6N139), V<sub>F,bridge</sub> ≈ 2 * 1.0V

#### Series Resistors `R84`, `R85`

* one resistor in L and one in N, so each sees only half the mains voltage and a single shorted resistor still limits the current
* R<sub>total</sub> = 2 * 120k = 240k
* I<sub>F,peak</sub> = (V<sub>peak</sub> - V<sub>F,LED</sub> - V<sub>F,bridge</sub>) / R<sub>total</sub> = (325V - 3.4V) / 240k = 1.34mA
* I<sub>F,rms</sub> ≈ V<sub>mains</sub> / R<sub>total</sub> = 0.96mA
* P per resistor = (V<sub>rms</sub> / R<sub>total</sub>)² * 120k = 0.11W (0.13W at 253V)
* voltage per resistor = 179V<sub>peak</sub> at 253V, which is too close to the 200V limit of a standard 2512 resistor → high-voltage type with 3kV surge rating
* total zero-cross power loss = V<sub>mains</sub>² / R<sub>total</sub> = 0.22W

Values are:
* `R84` = `R85` = 120k 3kV 1W (2512)

#### Bridge Rectifier `D80`

* rectifies both half waves so the opto LED conducts on every half cycle
* current is only ~1.3mA peak, the part is chosen by its reverse voltage: 1000V ≫ 358V<sub>peak</sub> plus mains transients

Value is:
* `D80` = MB10S (1000V, TO-269AA / MBS)

#### Optocoupler `U81` and Output

* 6N139 CTR ≥ 400% at I<sub>F</sub> = 0.5mA
* required collector current: I<sub>C</sub> = (3.3V - 0.4V) / `R81` = 2.9V / 12k = 0.24mA → only CTR ≥ 48% needed at 0.5mA, plenty of margin
* guaranteed output low when I<sub>F</sub> ≥ 0.5mA, i.e. at |v(t)| ≥ 0.5mA * 240k + 3.4V = 123V
* upper bound for the zero-cross pulse width: t<sub>ZC</sub> = 2 * arcsin(123V / 325V) / (2π * 50Hz) = 2.5ms; the actual pulse is narrower because the typical CTR is much higher
* `R81` 12k pull-up: GPIO34 is input-only without internal pull-ups; 3.3V / 12k = 0.28mA when low
* `R82` 100k from the output transistor's base (pin 7) to GND: speeds up turn-off and suppresses leakage currents; the photocurrent needs 0.6V / 100k = 6µA before the output starts conducting
* `R83` 100R + `C81` 100nF: RC filter on the opto's V<sub>CC</sub>, f<sub>c</sub> = 1 / (2π * 100Ω * 100nF) = 15.9kHz

Values are:
* `U81` = 6N139
* `R81` = 12k
* `R82` = 100k
* `R83` = 100R
* `C81` = 100nF

### AC Dimmer TRIAC

#### Triac Driver Input `U82`, `R86`, `D81`, `R87`

* FOD4208: I<sub>FT,max</sub> = 2.0mA (guaranteed trigger), V<sub>F</sub> ≈ 1.2V
* I<sub>F</sub> = (3.3V - 1.2V) / `R86` = 2.1V / 220R = 9.5mA → ~4.8x I<sub>FT,max</sub>, margin for LED aging and temperature
* indicator `D81` (yellow, V<sub>F</sub> ≈ 2.0V): I = (3.3V - 2.0V) / `R87` = 1.3V / 470R = 2.8mA
* total GPIO16 load ≈ 12.3mA
* FOD4208 is the 800V version (V<sub>DRM</sub>), random phase (no internal zero-cross) as required for phase angle control; static dV/dt ≥ 10kV/µs

Values are:
* `U82` = FOD4208SD
* `R86` = 220R
* `R87` = 470R
* `D81` = LED yellow

#### Gate Resistors `R88`, `R89`

* `R88` limits the driver current if the triac fires at the mains peak: I = 358V / 470R = 0.76A, below the FOD4208's 3A non-repetitive surge current I<sub>TSM</sub>; the current flows only for a few µs until the triac has latched → anti-surge (AS) 2512 type for the pulse load
* `R89` from gate to MT1 drains driver leakage and dV/dt-induced currents, so the sensitive gate (E type) doesn't fire by itself; at V<sub>GT</sub> ≈ 1V it takes 1V / 330R = 3mA
* gate current comes from MT2 through the driver, so the gate polarity always follows MT2 → the triac only triggers in quadrants I and III (I<sub>GT</sub> ≤ 10mA, the slower quadrant IV at 25mA is never used)
* minimum mains voltage needed to fire: V<sub>min</sub> = (I<sub>GT</sub> + 3mA) * `R88` + V<sub>GT</sub> + V<sub>TM,driver</sub> = 13mA * 470R + 1V + 3V ≈ 10V → the triac can fire from arcsin(10V / 325V) = 1.8° (0.1ms) after the zero crossing

Values are:
* `R88` = 470R anti-surge (2512)
* `R89` = 330R (1206)

#### Triac `Q81`

* V<sub>DRM</sub> = 800V ≫ 358V<sub>peak</sub>, margin for mains transients
* I<sub>T(RMS)</sub> = 12A, the load is limited by cooling: dissipation is roughly 1W per A<sub>rms</sub> of load current, so the tab is cooled through the /L copper on both layers with thermal vias

Value is:
* `Q81` = BT138S-800E (DPAK)

#### Snubber `R80`, `C80`

* limits dV/dt across the triac and the driver when commutating inductive loads, τ = `R80` * `C80` = 39R * 10nF = 0.39µs
* off-state current through the snubber into the load: I = V<sub>mains</sub> * 2π * 50Hz * 10nF = 0.72mA<sub>rms</sub>
* `R80` limits the capacitor discharge into the triac when it fires at the mains peak: I<sub>peak</sub> = 358V / 39R = 9.2A
* energy per firing ½ * C * V² = 0.5 * 10nF * (325V)² = 0.53mJ, at 100 firings/s → ~0.05W (about double including the recharge every half cycle), 1W rating for surge headroom, 350V rated resistor
* `C80` must be a mains-rated X2 capacitor

Values are:
* `R80` = 39R 1W 350V
* `C80` = 10nF X2


### FAN PWM integrator switching
* usually PWM fans can't be reliably attenuated to 0 rpm and need to have their Vcc switched in order to turn them off completely
* so to save one extra GPIO output for switching the fan power, this circuit will integrate the PWM signal and simply disconnect the fan's 12V power if the PWM duty cycle is near 0

http://www.falstad.com/circuit/circuitjs.html?ctz=CQAgjCAMB0l3YBMBWEizWQTgMxYBwDsiOYkAbGACxIhWEirKSMCmAtGGAFADmdVfGkRUBQxMShRuAYxBZyIcqIXDRLRNETwduuIg5D2m-PmZIiiLIRz4wOchxyIosSDwBOIdoKWjnflJkcNwAZiA4DnRCDkK+keCYrpCI3F6Ipt5cLhlxQizk+PBp4ISKyqWKiCJB5DrcAO4RhREusd4BkNwAbmJqfZy0LCxULLRDrsiNfdX+LbPSTe2DLlS+K9Je7WBYbS3ZUszT7RXbQ8f7u83iNV1LLewBywd3aJkbz+dNudERUb5dcJgMqBEQFfwuDBUVDaaZgkBFNCjfqveG+H4A6aqBbAqq3aa4-rwhao5ELdokgkgiqqCqvWmiRF07gAJW8FWoonYHJBGjQzDgEWgOCFIuGsCmAGd2aIKtzRJygiBQgBDAA2ktY3AAJq1AkzRC5taxVQBXNUAF2m8sCPiEzIA9kjAiNIFgsOBxTprORkOQUFIcv5uE6wKg6XQ3R7kTBvWU-QGIJDUDgQxEXZH3ZHkvAfQmYWh06nQ+CpKMs7H4GUrHhoaQA5W4Hn-SnC2QItwgA
