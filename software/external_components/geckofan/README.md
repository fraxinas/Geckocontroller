┌──────────────────────┐     ┌───────────────────────┐
│ Hygrometer           │     │                       │
│                      ├────►├────────┐  MultiFan    │
│ PID Controller       │     │        │              │
└──────────────────────┘     │        │              │
                             │        │              │
┌──────────────────────┐     │        │              │
│ Thermometer          │     │        │              │
│                      ├────►├──────┐ │              │
│ PID Controller       │     │      │ │              │
└──────────────────────┘     │      │ │              │
                             │      │ │              │
┌──────────────────────┐     │      ▼ ▼              │
│ Vicinity Sensor      ├────►├─┐  ┌──────────────┐   │
└──────────────────────┘     │ └─►│ Calculate    │   │
                             │    │ combined     │   │
┌──────────────────────┐     │ ┌─►│ Fan speed    │   │
│ Windspeed Sensor     ├────►├─┘  └──────┬───────┘   │
└──────────────────────┘     │           │           │
                             │  ┌────────▼─────────┐ │       ┌────────────────────┐
                             │  │                  │ │       │                    │
                             │  │  FanState        ├─┼──────►│ FloatOutput (ledc) │
                             │  │                  │ │       │                    │
                             │  └──────────────────┘ │       └────────────────────┘
                             │                       │
                             └───────────────────────┘

durch höhere lüfterdrehzahl kann
* die feuchtigkeit verringert werden
* die innentemperatur (thermometer) and die vicinity-temperatur angenähert werden (-> sie kann niemals darunter gesenkt oder darüber erhöht werden, temperaturerhöhung ist nur durch heiz- oder tageslichtlampe möglich, temperatursenkung in minimalem umfang über regen)
* die natürliche umgebungswindgeschwindigkeit simuliert werden

die gewichtung für den multifan soll so laufen, dass wenn der windspeed von home assistant mit 0 vorgegeben wird, dass der lüfter keinesfalls mehr als 10% output speed haben darf
wenn von home assistant ein großer windspeed kommt >5km/h muss er mindestens 10% betragen und darf nicht durch andere bedingungen verringert werden, bei >30 km/h mind. 90%, bei > 50 km/h 100%

## Ausnahme von der 10%-Deckelung bei Windstille (24.09.2026)

Die Obergrenze aus dem letzten Absatz gilt nicht mehr uneingeschränkt: liegt die gemessene
Innentemperatur mehr als `OVERHEAT_RELEASE_K` (1,5 K) über dem Sollwert des Thermostats, wird
die Windstille-Deckelung aufgehoben und der Lüfter darf bis 100%. Die Untergrenzen
(wind_floor) bleiben davon unberührt.

Grund: die Deckelung ist Realismus, kein Regelziel. Bei ruhigem Simulationswetter konnte der
Lüfter den Überschwinger am Morgen nicht abbauen, obwohl er die Leistung dazu hat.

Ebenfalls geändert: die Kombination der beiden PID-Anforderungen ist jetzt `max(hygro, thermo)`
statt des gewichteten Mittels. Ein Mittelwert verdünnt eine Vollanforderung gegen eine
Nullanforderung -- mit hygro-Gewicht 4 und thermo-Gewicht 1 kam volle Kühlanforderung als 20%
Lüfter heraus. Ein Maximum lässt sich nicht gewichten, deshalb ist die `weight:`-Option bei
`hygrostat:` und `thermostat:` ersatzlos entfallen; eine YAML, die sie noch setzt, wird von der
Konfigurationsprüfung abgelehnt.
