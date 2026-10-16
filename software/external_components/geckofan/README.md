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
