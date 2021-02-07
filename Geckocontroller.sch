EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Triac_Thyristor:BTA16-600B Q?
U 1 1 601EE7A7
P 9900 2950
F 0 "Q?" H 10029 2904 50  0000 L CNN
F 1 "BTA16-600B" H 10029 2995 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-220-3_Vertical" H 10100 2875 50  0001 L CIN
F 3 "https://www.st.com/resource/en/datasheet/bta16.pdf" H 9900 2950 50  0001 L CNN
	1    9900 2950
	1    0    0    1   
$EndComp
$Comp
L Relay:FINDER-44.52 K?
U 1 1 601F3BE4
P 9150 4950
F 0 "K?" H 8520 4904 50  0000 R CNN
F 1 "FINDER-44.52" H 8520 4995 50  0000 R CNN
F 2 "Relay_THT:Relay_DPDT_Finder_40.52" H 10500 4920 50  0001 C CNN
F 3 "http://gfinder.findernet.com/assets/Series/359/S44EN.pdf" H 9150 4950 50  0001 C CNN
	1    9150 4950
	1    0    0    1   
$EndComp
$Comp
L Transistor_BJT:BC547 Q?
U 1 1 601F75AF
P 7450 4950
F 0 "Q?" H 7641 4996 50  0000 L CNN
F 1 "BC547" H 7641 4905 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 7650 4875 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BC550-D.pdf" H 7450 4950 50  0001 L CNN
	1    7450 4950
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 601F9153
P 7950 4650
F 0 "R?" V 7743 4650 50  0000 C CNN
F 1 "R" V 7834 4650 50  0000 C CNN
F 2 "" V 7880 4650 50  0001 C CNN
F 3 "~" H 7950 4650 50  0001 C CNN
	1    7950 4650
	0    -1   1    0   
$EndComp
$Comp
L Device:R R?
U 1 1 60221A24
P 9350 2050
F 0 "R?" H 9420 2096 50  0000 L CNN
F 1 "470R" H 9420 2005 50  0000 L CNN
F 2 "" V 9280 2050 50  0001 C CNN
F 3 "~" H 9350 2050 50  0001 C CNN
	1    9350 2050
	0    1    -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 60226BE8
P 7550 3150
F 0 "#PWR?" H 7550 2900 50  0001 C CNN
F 1 "GND" H 7555 2977 50  0000 C CNN
F 2 "" H 7550 3150 50  0001 C CNN
F 3 "" H 7550 3150 50  0001 C CNN
	1    7550 3150
	-1   0    0    -1  
$EndComp
$Comp
L Connector:Screw_Terminal_01x02 J?
U 1 1 6023B43D
P 10800 2650
F 0 "J?" H 10718 2325 50  0001 C CNN
F 1 "Dimmer" H 10880 2596 50  0000 L CNN
F 2 "" H 10800 2650 50  0001 C CNN
F 3 "~" H 10800 2650 50  0001 C CNN
	1    10800 2650
	1    0    0    -1  
$EndComp
$Comp
L Connector:Screw_Terminal_01x02 J?
U 1 1 6023C73E
P 10750 5300
F 0 "J?" H 10668 4975 50  0001 C CNN
F 1 "HeatBed" H 10830 5246 50  0000 L CNN
F 2 "" H 10750 5300 50  0001 C CNN
F 3 "~" H 10750 5300 50  0001 C CNN
	1    10750 5300
	1    0    0    -1  
$EndComp
$Comp
L Diode:1N4148 D?
U 1 1 601FBBFD
P 8300 4950
F 0 "D?" V 8254 5030 50  0000 L CNN
F 1 "1N4148" V 8345 5030 50  0000 L CNN
F 2 "Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal" H 8300 4775 50  0001 C CNN
F 3 "https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf" H 8300 4950 50  0001 C CNN
	1    8300 4950
	0    -1   1    0   
$EndComp
$Comp
L Device:R R?
U 1 1 6037F9CF
P 6900 3050
F 0 "R?" V 6693 3050 50  0000 C CNN
F 1 "R" V 6784 3050 50  0000 C CNN
F 2 "" V 6830 3050 50  0001 C CNN
F 3 "~" H 6900 3050 50  0001 C CNN
	1    6900 3050
	0    -1   1    0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 602F8D6E
P 8100 1650
F 0 "#PWR?" H 8100 1400 50  0001 C CNN
F 1 "GND" H 8105 1477 50  0000 C CNN
F 2 "" H 8100 1650 50  0001 C CNN
F 3 "" H 8100 1650 50  0001 C CNN
	1    8100 1650
	-1   0    0    -1  
$EndComp
$Comp
L Device:D_Bridge_+-AA D?
U 1 1 601EDA3B
P 9900 2350
F 0 "D?" V 9946 2006 50  0000 R CNN
F 1 "D_Bridge_+-AA" V 9855 2006 50  0000 R CNN
F 2 "" H 9900 2350 50  0001 C CNN
F 3 "~" H 9900 2350 50  0001 C CNN
	1    9900 2350
	0    1    -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 60442826
P 7550 5350
F 0 "#PWR?" H 7550 5100 50  0001 C CNN
F 1 "GND" H 7555 5177 50  0000 C CNN
F 2 "" H 7550 5350 50  0001 C CNN
F 3 "" H 7550 5350 50  0001 C CNN
	1    7550 5350
	-1   0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 6046FC6C
P 3850 1650
F 0 "#PWR?" H 3850 1400 50  0001 C CNN
F 1 "GND" H 3855 1477 50  0000 C CNN
F 2 "" H 3850 1650 50  0001 C CNN
F 3 "" H 3850 1650 50  0001 C CNN
	1    3850 1650
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 604719AB
P 1800 4000
F 0 "#PWR?" H 1800 3750 50  0001 C CNN
F 1 "GND" H 1805 3827 50  0000 C CNN
F 2 "" H 1800 4000 50  0001 C CNN
F 3 "" H 1800 4000 50  0001 C CNN
	1    1800 4000
	1    0    0    -1  
$EndComp
$Comp
L Isolator:6N138 U?
U 1 1 601EC117
P 8750 2350
F 0 "U?" V 8704 2680 50  0000 L CNN
F 1 "6N138" V 8795 2680 50  0000 L CNN
F 2 "" H 9040 2050 50  0001 C CNN
F 3 "http://www.onsemi.com/pub/Collateral/HCPL2731-D.pdf" H 9040 2050 50  0001 C CNN
	1    8750 2350
	-1   0    0    -1  
$EndComp
$Comp
L Relay_SolidState:MOC3021M U?
U 1 1 60238DEB
P 8750 2950
F 0 "U?" H 8750 2633 50  0000 C CNN
F 1 "MOC3021M" H 8750 2724 50  0000 C CNN
F 2 "" H 8550 2750 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/MOC3023M-D.PDF" H 8750 2950 50  0001 L CNN
	1    8750 2950
	1    0    0    1   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 6058DFD9
P 1800 750
F 0 "#PWR?" H 1800 600 50  0001 C CNN
F 1 "+3.3V" H 1815 923 50  0000 C CNN
F 2 "" H 1800 750 50  0001 C CNN
F 3 "" H 1800 750 50  0001 C CNN
	1    1800 750 
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 60664BE2
P 6900 4950
F 0 "R?" V 6693 4950 50  0000 C CNN
F 1 "R" V 6784 4950 50  0000 C CNN
F 2 "" V 6830 4950 50  0001 C CNN
F 3 "~" H 6900 4950 50  0001 C CNN
	1    6900 4950
	0    1    1    0   
$EndComp
$Comp
L Relay:FINDER-44.52 K?
U 1 1 606CDA6E
P 9150 4000
F 0 "K?" H 8520 3954 50  0000 R CNN
F 1 "FINDER-44.52" H 8520 4045 50  0000 R CNN
F 2 "Relay_THT:Relay_DPDT_Finder_40.52" H 10500 3970 50  0001 C CNN
F 3 "http://gfinder.findernet.com/assets/Series/359/S44EN.pdf" H 9150 4000 50  0001 C CNN
	1    9150 4000
	1    0    0    1   
$EndComp
$Comp
L Transistor_BJT:BC547 Q?
U 1 1 606CDE48
P 7450 4000
F 0 "Q?" H 7641 4046 50  0000 L CNN
F 1 "BC547" H 7641 3955 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 7650 3925 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BC550-D.pdf" H 7450 4000 50  0001 L CNN
	1    7450 4000
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 606CDE52
P 7950 3700
F 0 "R?" V 7743 3700 50  0000 C CNN
F 1 "R" V 7834 3700 50  0000 C CNN
F 2 "" V 7880 3700 50  0001 C CNN
F 3 "~" H 7950 3700 50  0001 C CNN
	1    7950 3700
	0    -1   1    0   
$EndComp
$Comp
L Connector:Screw_Terminal_01x02 J?
U 1 1 606CDE5C
P 10750 4350
F 0 "J?" H 10668 4025 50  0001 C CNN
F 1 "Pump" H 10830 4296 50  0000 L CNN
F 2 "" H 10750 4350 50  0001 C CNN
F 3 "~" H 10750 4350 50  0001 C CNN
	1    10750 4350
	1    0    0    -1  
$EndComp
$Comp
L Diode:1N4148 D?
U 1 1 606CDE66
P 8300 4000
F 0 "D?" V 8254 4080 50  0000 L CNN
F 1 "1N4148" V 8345 4080 50  0000 L CNN
F 2 "Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal" H 8300 3825 50  0001 C CNN
F 3 "https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf" H 8300 4000 50  0001 C CNN
	1    8300 4000
	0    -1   1    0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 606CDE70
P 7550 4400
F 0 "#PWR?" H 7550 4150 50  0001 C CNN
F 1 "GND" H 7555 4227 50  0000 C CNN
F 2 "" H 7550 4400 50  0001 C CNN
F 3 "" H 7550 4400 50  0001 C CNN
	1    7550 4400
	-1   0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 606CDE8C
P 6900 4000
F 0 "R?" V 6693 4000 50  0000 C CNN
F 1 "R" V 6784 4000 50  0000 C CNN
F 2 "" V 6830 4000 50  0001 C CNN
F 3 "~" H 6900 4000 50  0001 C CNN
	1    6900 4000
	0    1    1    0   
$EndComp
$Comp
L Relay:FINDER-44.52 K?
U 1 1 606EDE42
P 9150 5900
F 0 "K?" H 8520 5854 50  0000 R CNN
F 1 "FINDER-44.52" H 8520 5945 50  0000 R CNN
F 2 "Relay_THT:Relay_DPDT_Finder_40.52" H 10500 5870 50  0001 C CNN
F 3 "http://gfinder.findernet.com/assets/Series/359/S44EN.pdf" H 9150 5900 50  0001 C CNN
	1    9150 5900
	1    0    0    1   
$EndComp
$Comp
L Transistor_BJT:BC547 Q?
U 1 1 606EE318
P 7450 5900
F 0 "Q?" H 7641 5946 50  0000 L CNN
F 1 "BC547" H 7641 5855 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 7650 5825 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BC550-D.pdf" H 7450 5900 50  0001 L CNN
	1    7450 5900
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 606EE322
P 7950 5600
F 0 "R?" V 7743 5600 50  0000 C CNN
F 1 "R" V 7834 5600 50  0000 C CNN
F 2 "" V 7880 5600 50  0001 C CNN
F 3 "~" H 7950 5600 50  0001 C CNN
	1    7950 5600
	0    -1   1    0   
$EndComp
$Comp
L Connector:Screw_Terminal_01x02 J?
U 1 1 606EE32C
P 10750 6250
F 0 "J?" H 10668 5925 50  0001 C CNN
F 1 "HeatLamp" H 10830 6196 50  0000 L CNN
F 2 "" H 10750 6250 50  0001 C CNN
F 3 "~" H 10750 6250 50  0001 C CNN
	1    10750 6250
	1    0    0    -1  
$EndComp
$Comp
L Diode:1N4148 D?
U 1 1 606EE336
P 8300 5900
F 0 "D?" V 8254 5980 50  0000 L CNN
F 1 "1N4148" V 8345 5980 50  0000 L CNN
F 2 "Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal" H 8300 5725 50  0001 C CNN
F 3 "https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf" H 8300 5900 50  0001 C CNN
	1    8300 5900
	0    -1   1    0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 606EE340
P 7550 6300
F 0 "#PWR?" H 7550 6050 50  0001 C CNN
F 1 "GND" H 7555 6127 50  0000 C CNN
F 2 "" H 7550 6300 50  0001 C CNN
F 3 "" H 7550 6300 50  0001 C CNN
	1    7550 6300
	-1   0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 606EE358
P 6900 5900
F 0 "R?" V 6693 5900 50  0000 C CNN
F 1 "R" V 6784 5900 50  0000 C CNN
F 2 "" V 6830 5900 50  0001 C CNN
F 3 "~" H 6900 5900 50  0001 C CNN
	1    6900 5900
	0    1    1    0   
$EndComp
$Comp
L Device:R R?
U 1 1 602281A5
P 7550 2500
F 0 "R?" H 7480 2454 50  0000 R CNN
F 1 "10k" H 7480 2545 50  0000 R CNN
F 2 "" V 7480 2500 50  0001 C CNN
F 3 "~" H 7550 2500 50  0001 C CNN
	1    7550 2500
	1    0    0    1   
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 6078DFBD
P 7200 3600
F 0 "#PWR?" H 7200 3450 50  0001 C CNN
F 1 "+12V" H 7215 3773 50  0000 C CNN
F 2 "" H 7200 3600 50  0001 C CNN
F 3 "" H 7200 3600 50  0001 C CNN
	1    7200 3600
	1    0    0    -1  
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 607AAEF6
P 7200 4550
F 0 "#PWR?" H 7200 4400 50  0001 C CNN
F 1 "+12V" H 7215 4723 50  0000 C CNN
F 2 "" H 7200 4550 50  0001 C CNN
F 3 "" H 7200 4550 50  0001 C CNN
	1    7200 4550
	1    0    0    -1  
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 607B9570
P 7200 5500
F 0 "#PWR?" H 7200 5350 50  0001 C CNN
F 1 "+12V" H 7215 5673 50  0000 C CNN
F 2 "" H 7200 5500 50  0001 C CNN
F 3 "" H 7200 5500 50  0001 C CNN
	1    7200 5500
	1    0    0    -1  
$EndComp
$Comp
L Regulator_Switching:LM2575-3.3BU U?
U 1 1 607F11A0
P 5600 1050
F 0 "U?" H 5600 1417 50  0000 C CNN
F 1 "LM2575-3.3BU" H 5600 1326 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:TO-263-5_TabPin3" H 5600 800 50  0001 L CIN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/lm2575.pdf" H 5600 1050 50  0001 C CNN
	1    5600 1050
	1    0    0    -1  
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 607FAD6E
P 8400 750
F 0 "#PWR?" H 8400 600 50  0001 C CNN
F 1 "+12V" H 8415 923 50  0000 C CNN
F 2 "" H 8400 750 50  0001 C CNN
F 3 "" H 8400 750 50  0001 C CNN
	1    8400 750 
	1    0    0    -1  
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 608000F2
P 4450 750
F 0 "#PWR?" H 4450 600 50  0001 C CNN
F 1 "+12V" H 4465 923 50  0000 C CNN
F 2 "" H 4450 750 50  0001 C CNN
F 3 "" H 4450 750 50  0001 C CNN
	1    4450 750 
	1    0    0    -1  
$EndComp
$Comp
L Device:CP C?
U 1 1 608020FB
P 4450 1200
F 0 "C?" H 4568 1246 50  0000 L CNN
F 1 "100µF" H 4568 1155 50  0000 L CNN
F 2 "" H 4488 1050 50  0001 C CNN
F 3 "~" H 4450 1200 50  0001 C CNN
	1    4450 1200
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 60822F51
P 4450 1650
F 0 "#PWR?" H 4450 1400 50  0001 C CNN
F 1 "GND" H 4455 1477 50  0000 C CNN
F 2 "" H 4450 1650 50  0001 C CNN
F 3 "" H 4450 1650 50  0001 C CNN
	1    4450 1650
	1    0    0    -1  
$EndComp
$Comp
L Diode:1N5819 D?
U 1 1 6083446E
P 6350 1350
F 0 "D?" V 6304 1430 50  0000 L CNN
F 1 "1N5819" V 6395 1430 50  0000 L CNN
F 2 "Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal" H 6350 1175 50  0001 C CNN
F 3 "http://www.vishay.com/docs/88525/1n5817.pdf" H 6350 1350 50  0001 C CNN
	1    6350 1350
	0    1    1    0   
$EndComp
$Comp
L pspice:INDUCTOR L?
U 1 1 60838692
P 6800 1150
F 0 "L?" H 6800 1365 50  0000 C CNN
F 1 "330µH" H 6800 1274 50  0000 C CNN
F 2 "" H 6800 1150 50  0001 C CNN
F 3 "~" H 6800 1150 50  0001 C CNN
	1    6800 1150
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 60860521
P 7250 750
F 0 "#PWR?" H 7250 600 50  0001 C CNN
F 1 "+3.3V" H 7265 923 50  0000 C CNN
F 2 "" H 7250 750 50  0001 C CNN
F 3 "" H 7250 750 50  0001 C CNN
	1    7250 750 
	1    0    0    -1  
$EndComp
$Comp
L Device:CP C?
U 1 1 6084BB4A
P 7250 1350
F 0 "C?" H 7368 1396 50  0000 L CNN
F 1 "330µF" H 7368 1305 50  0000 L CNN
F 2 "" H 7288 1200 50  0001 C CNN
F 3 "~" H 7250 1350 50  0001 C CNN
	1    7250 1350
	1    0    0    -1  
$EndComp
$Comp
L Converter_ACDC:IRM-05-12 PS?
U 1 1 60671E36
P 8850 1100
F 0 "PS?" H 8850 733 50  0000 C CNN
F 1 "IRM-05-12" H 8850 824 50  0000 C CNN
F 2 "Converter_ACDC:Converter_ACDC_MeanWell_IRM-05-xx_THT" H 8850 750 50  0001 C CNN
F 3 "https://www.meanwell.com/Upload/PDF/IRM-05/IRM-05-SPEC.PDF" H 8850 700 50  0001 C CNN
	1    8850 1100
	-1   0    0    1   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 609B3599
P 8400 2000
F 0 "#PWR?" H 8400 1850 50  0001 C CNN
F 1 "+3.3V" H 8415 2173 50  0000 C CNN
F 2 "" H 8400 2000 50  0001 C CNN
F 3 "" H 8400 2000 50  0001 C CNN
	1    8400 2000
	1    0    0    -1  
$EndComp
$Comp
L Transistor_FET:IRLML0030 Q?
U 1 1 60AAF2AE
P 2100 4550
F 0 "Q?" H 2305 4504 50  0000 L CNN
F 1 "IRLML0030" H 2305 4595 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 2300 4475 50  0001 L CIN
F 3 "https://www.infineon.com/dgdl/irlml0030pbf.pdf?fileId=5546d462533600a401535664773825df" H 2100 4550 50  0001 L CNN
	1    2100 4550
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x04_Male J?
U 1 1 60AB12DD
P 950 4750
F 0 "J?" H 1058 5031 50  0001 C CNN
F 1 "RGB LED" H 1058 4939 50  0000 C CNN
F 2 "" H 950 4750 50  0001 C CNN
F 3 "~" H 950 4750 50  0001 C CNN
	1    950  4750
	1    0    0    -1  
$EndComp
$Comp
L Transistor_FET:IRLML0030 Q?
U 1 1 60B1168B
P 2300 4850
F 0 "Q?" H 2505 4804 50  0000 L CNN
F 1 "IRLML0030" H 2505 4895 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 2500 4775 50  0001 L CIN
F 3 "https://www.infineon.com/dgdl/irlml0030pbf.pdf?fileId=5546d462533600a401535664773825df" H 2300 4850 50  0001 L CNN
	1    2300 4850
	-1   0    0    1   
$EndComp
$Comp
L Transistor_FET:IRLML0030 Q?
U 1 1 60B166CB
P 2500 5150
F 0 "Q?" H 2705 5104 50  0000 L CNN
F 1 "IRLML0030" H 2705 5195 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 2700 5075 50  0001 L CIN
F 3 "https://www.infineon.com/dgdl/irlml0030pbf.pdf?fileId=5546d462533600a401535664773825df" H 2500 5150 50  0001 L CNN
	1    2500 5150
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 60B61D3B
P 3050 5250
F 0 "#PWR?" H 3050 5000 50  0001 C CNN
F 1 "GND" H 3055 5077 50  0000 C CNN
F 2 "" H 3050 5250 50  0001 C CNN
F 3 "" H 3050 5250 50  0001 C CNN
	1    3050 5250
	1    0    0    -1  
$EndComp
$Comp
L power:+12V #PWR?
U 1 1 60B94268
P 1450 4500
F 0 "#PWR?" H 1450 4350 50  0001 C CNN
F 1 "+12V" H 1465 4673 50  0000 C CNN
F 2 "" H 1450 4500 50  0001 C CNN
F 3 "" H 1450 4500 50  0001 C CNN
	1    1450 4500
	1    0    0    -1  
$EndComp
Text Notes 800  5450 0    50   ~ 0
RGB LED Driver
Text Notes 10250 600  0    50   ~ 0
Power Supply
$Comp
L power:GND #PWR?
U 1 1 60E13A39
P 7250 1650
F 0 "#PWR?" H 7250 1400 50  0001 C CNN
F 1 "GND" H 7255 1477 50  0000 C CNN
F 2 "" H 7250 1650 50  0001 C CNN
F 3 "" H 7250 1650 50  0001 C CNN
	1    7250 1650
	1    0    0    -1  
$EndComp
$Comp
L Connector:Screw_Terminal_01x03 J?
U 1 1 60EB9D0C
P 950 6050
F 0 "J?" H 868 5725 50  0001 C CNN
F 1 "DS18B20 II" H 868 5817 50  0000 C CNN
F 2 "" H 950 6050 50  0001 C CNN
F 3 "~" H 950 6050 50  0001 C CNN
	1    950  6050
	-1   0    0    1   
$EndComp
$Comp
L Connector:Screw_Terminal_01x03 J?
U 1 1 60EBD567
P 950 6500
F 0 "J?" H 868 6175 50  0001 C CNN
F 1 "DS18B20 I" H 868 6267 50  0000 C CNN
F 2 "" H 950 6500 50  0001 C CNN
F 3 "~" H 950 6500 50  0001 C CNN
	1    950  6500
	-1   0    0    1   
$EndComp
$Comp
L Device:CP C?
U 1 1 60460845
P 2100 900
F 0 "C?" H 2218 946 50  0000 L CNN
F 1 "100µF" H 2218 855 50  0000 L CNN
F 2 "" H 2138 750 50  0001 C CNN
F 3 "~" H 2100 900 50  0001 C CNN
	1    2100 900 
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 610336C7
P 1400 6700
F 0 "#PWR?" H 1400 6450 50  0001 C CNN
F 1 "GND" H 1405 6527 50  0000 C CNN
F 2 "" H 1400 6700 50  0001 C CNN
F 3 "" H 1400 6700 50  0001 C CNN
	1    1400 6700
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 60EBEE15
P 1800 5850
F 0 "R?" V 1593 5850 50  0000 C CNN
F 1 "4k7" V 1684 5850 50  0000 C CNN
F 2 "" V 1730 5850 50  0001 C CNN
F 3 "~" H 1800 5850 50  0001 C CNN
	1    1800 5850
	0    1    1    0   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 6131E5C2
P 2150 5800
F 0 "#PWR?" H 2150 5650 50  0001 C CNN
F 1 "+3.3V" H 2165 5973 50  0000 C CNN
F 2 "" H 2150 5800 50  0001 C CNN
F 3 "" H 2150 5800 50  0001 C CNN
	1    2150 5800
	1    0    0    -1  
$EndComp
Text Notes 900  4975 2    59   ~ 0
+12V\n-G\n-R\n-B
Text Notes 850  6000 2    50   ~ 0
DQ
Text Notes 850  6100 2    50   ~ 0
VDD
Text Notes 850  6200 2    50   ~ 0
GND
Text Notes 850  6450 2    50   ~ 0
DQ
Text Notes 850  6550 2    50   ~ 0
VDD
Text Notes 850  6650 2    50   ~ 0
GND
Text Notes 10800 975  0    50   ~ 0
L
Text Notes 10800 1300 0    50   ~ 0
N
Text Notes 2100 6900 0    50   ~ 0
Digital Thermometer Input
$Comp
L Connector:Conn_01x04_Male J?
U 1 1 61878125
P 950 7300
F 0 "J?" H 1058 7581 50  0001 C CNN
F 1 "BME280" H 1058 7489 50  0000 C CNN
F 2 "" H 950 7300 50  0001 C CNN
F 3 "~" H 950 7300 50  0001 C CNN
	1    950  7300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 618DBADA
P 2400 7450
F 0 "#PWR?" H 2400 7300 50  0001 C CNN
F 1 "+3.3V" H 2415 7623 50  0000 C CNN
F 2 "" H 2400 7450 50  0001 C CNN
F 3 "" H 2400 7450 50  0001 C CNN
	1    2400 7450
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 619104AB
P 1400 7550
F 0 "#PWR?" H 1400 7300 50  0001 C CNN
F 1 "GND" H 1405 7377 50  0000 C CNN
F 2 "" H 1400 7550 50  0001 C CNN
F 3 "" H 1400 7550 50  0001 C CNN
	1    1400 7550
	1    0    0    -1  
$EndComp
Text Notes 1800 7700 0    50   ~ 0
BME280 Thermo/Hygo/Baro Input
Text Notes 900  7525 2    59   ~ 0
SCK\nSDI\nGND\nVCC
$Comp
L Connector:Screw_Terminal_01x02 J?
U 1 1 6020C5BA
P 10800 1050
F 0 "J?" H 10718 725 50  0001 C CNN
F 1 "AC Input" H 10880 996 50  0000 L CNN
F 2 "" H 10800 1050 50  0001 C CNN
F 3 "~" H 10800 1050 50  0001 C CNN
	1    10800 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C?
U 1 1 61FC96F5
P 3900 6500
F 0 "C?" H 4015 6546 50  0000 L CNN
F 1 "10µF" H 4015 6455 50  0000 L CNN
F 2 "" H 3938 6350 50  0001 C CNN
F 3 "~" H 3900 6500 50  0001 C CNN
	1    3900 6500
	1    0    0    -1  
$EndComp
$Comp
L Device:C C?
U 1 1 61FCA716
P 4300 6500
F 0 "C?" H 4415 6546 50  0000 L CNN
F 1 "100nF" H 4415 6455 50  0000 L CNN
F 2 "" H 4338 6350 50  0001 C CNN
F 3 "~" H 4300 6500 50  0001 C CNN
	1    4300 6500
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 620281CF
P 3900 4250
F 0 "#PWR?" H 3900 4100 50  0001 C CNN
F 1 "+3.3V" H 3915 4423 50  0000 C CNN
F 2 "" H 3900 4250 50  0001 C CNN
F 3 "" H 3900 4250 50  0001 C CNN
	1    3900 4250
	1    0    0    -1  
$EndComp
$Comp
L Transistor_BJT:S8050 Q?
U 1 1 62083541
P 4300 3450
F 0 "Q?" V 4628 3450 50  0000 C CNN
F 1 "S8050" V 4537 3450 50  0000 C CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 4500 3375 50  0001 L CIN
F 3 "http://www.unisonic.com.tw/datasheet/S8050.pdf" H 4300 3450 50  0001 L CNN
	1    4300 3450
	0    -1   -1   0   
$EndComp
$Comp
L Transistor_BJT:S8050 Q?
U 1 1 62084874
P 5100 3450
F 0 "Q?" V 5428 3450 50  0000 C CNN
F 1 "S8050" V 5337 3450 50  0000 C CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 5300 3375 50  0001 L CIN
F 3 "http://www.unisonic.com.tw/datasheet/S8050.pdf" H 5100 3450 50  0001 L CNN
	1    5100 3450
	0    1    -1   0   
$EndComp
$Comp
L Device:R R?
U 1 1 62198FF5
P 4300 3900
F 0 "R?" H 4370 3946 50  0000 L CNN
F 1 "12k" H 4370 3855 50  0000 L CNN
F 2 "" V 4230 3900 50  0001 C CNN
F 3 "~" H 4300 3900 50  0001 C CNN
	1    4300 3900
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 6219F547
P 5100 3900
F 0 "R?" H 5170 3946 50  0000 L CNN
F 1 "12k" H 5170 3855 50  0000 L CNN
F 2 "" V 5030 3900 50  0001 C CNN
F 3 "~" H 5100 3900 50  0001 C CNN
	1    5100 3900
	1    0    0    -1  
$EndComp
Wire Wire Line
	10200 1050 10200 1000
Wire Wire Line
	10600 1150 10350 1150
Wire Wire Line
	8300 5250 8300 5100
Wire Wire Line
	8750 5250 8300 5250
Wire Wire Line
	8300 4650 8300 4800
Wire Wire Line
	8750 4650 8300 4650
Wire Wire Line
	10600 1050 10450 1050
Connection ~ 10450 1050
Wire Wire Line
	10450 1050 10200 1050
Wire Wire Line
	8450 3050 7050 3050
Wire Wire Line
	10200 1000 9250 1000
Wire Wire Line
	10350 2350 10200 2350
Wire Wire Line
	9900 2050 9500 2050
Wire Wire Line
	9600 2350 9600 1900
Wire Wire Line
	9600 1900 10450 1900
Wire Wire Line
	9900 2650 9050 2650
Wire Wire Line
	9050 2450 9050 2650
Wire Wire Line
	9150 4550 9150 4650
Wire Wire Line
	8300 4650 8100 4650
Connection ~ 8300 4650
Wire Wire Line
	7550 5350 7550 5150
Wire Wire Line
	9050 2250 9050 2050
Wire Wire Line
	9050 2050 9200 2050
Wire Wire Line
	8400 2150 8450 2150
Wire Wire Line
	7550 2350 7550 2250
Wire Wire Line
	7550 2250 8450 2250
Wire Wire Line
	9050 2850 9750 2850
Wire Wire Line
	9050 3050 9600 3050
Wire Wire Line
	9150 4550 10350 4550
Wire Wire Line
	9550 4650 10450 4650
Wire Wire Line
	7550 4750 7550 4650
Wire Wire Line
	7550 4650 7800 4650
Wire Wire Line
	9600 3050 9600 3150
Wire Wire Line
	7050 4950 7250 4950
Wire Wire Line
	8300 4300 8300 4150
Wire Wire Line
	8750 4300 8300 4300
Wire Wire Line
	8300 3700 8300 3850
Wire Wire Line
	8750 3700 8300 3700
Wire Wire Line
	9150 3600 9150 3700
Wire Wire Line
	8300 3700 8100 3700
Connection ~ 8300 3700
Wire Wire Line
	7550 4400 7550 4200
Wire Wire Line
	9150 3600 10350 3600
Wire Wire Line
	9550 3700 10450 3700
Wire Wire Line
	7550 3800 7550 3700
Wire Wire Line
	7550 3700 7800 3700
Wire Wire Line
	7050 4000 7250 4000
Wire Wire Line
	6750 4000 6350 4000
Wire Wire Line
	10350 3600 10350 4550
Wire Wire Line
	10450 3700 10450 4650
Wire Wire Line
	8300 6200 8300 6050
Wire Wire Line
	8750 6200 8300 6200
Wire Wire Line
	8300 5600 8300 5750
Wire Wire Line
	8750 5600 8300 5600
Wire Wire Line
	9150 5500 9150 5600
Wire Wire Line
	8300 5600 8100 5600
Connection ~ 8300 5600
Wire Wire Line
	7550 6300 7550 6100
Wire Wire Line
	9550 5600 10450 5600
Wire Wire Line
	7550 5700 7550 5600
Wire Wire Line
	7550 5600 7800 5600
Wire Wire Line
	7050 5900 7250 5900
Wire Wire Line
	10450 4650 10450 5600
Connection ~ 10450 4650
Wire Notes Line width 39 rgb(255, 0, 0)
	8800 500  8800 6450
Wire Wire Line
	7550 2850 7550 3150
Wire Wire Line
	7550 2850 8450 2850
Connection ~ 7550 2850
Wire Wire Line
	7550 3700 7200 3700
Wire Wire Line
	7200 3700 7200 3600
Connection ~ 7550 3700
Wire Wire Line
	7550 4650 7200 4650
Wire Wire Line
	7200 4650 7200 4550
Connection ~ 7550 4650
Wire Wire Line
	7550 5600 7200 5600
Wire Wire Line
	7200 5600 7200 5500
Connection ~ 7550 5600
Wire Wire Line
	9150 5500 10350 5500
Wire Wire Line
	10350 5500 10350 4550
Connection ~ 10350 4550
Wire Wire Line
	6250 4950 6750 4950
Wire Wire Line
	6150 5900 6750 5900
Wire Wire Line
	4450 750  4450 950 
Wire Wire Line
	4450 1350 4450 1550
Wire Wire Line
	5100 950  4450 950 
Connection ~ 4450 950 
Wire Wire Line
	4450 950  4450 1050
Wire Wire Line
	5100 1150 5100 1550
Wire Wire Line
	5100 1550 4450 1550
Connection ~ 4450 1550
Wire Wire Line
	4450 1550 4450 1650
Wire Wire Line
	5100 1550 5600 1550
Wire Wire Line
	5600 1550 5600 1350
Connection ~ 5100 1550
Wire Wire Line
	6100 1150 6350 1150
Wire Wire Line
	6350 1150 6350 1200
Connection ~ 6350 1150
Wire Wire Line
	6350 1150 6550 1150
Wire Wire Line
	6350 1500 6350 1550
Wire Wire Line
	6350 1550 5600 1550
Connection ~ 5600 1550
Wire Wire Line
	7250 1500 7250 1550
Wire Wire Line
	7250 1550 6350 1550
Connection ~ 6350 1550
Wire Wire Line
	7250 1200 7250 1150
Wire Wire Line
	7250 950  6100 950 
Wire Wire Line
	7250 750  7250 950 
Connection ~ 7250 950 
Connection ~ 7250 1150
Wire Wire Line
	7250 1150 7250 950 
Wire Wire Line
	7050 1150 7250 1150
Connection ~ 1800 900 
Wire Wire Line
	1800 900  1950 900 
Wire Wire Line
	8400 750  8400 1200
Wire Wire Line
	8100 1000 8450 1000
Wire Wire Line
	8100 1000 8100 1650
Wire Wire Line
	8450 1200 8400 1200
Wire Wire Line
	8400 2150 8400 2000
Wire Wire Line
	10350 1150 10200 1150
Wire Wire Line
	10200 1150 10200 1200
Wire Wire Line
	10200 1200 9250 1200
Connection ~ 10350 1150
Wire Wire Line
	9650 4300 9650 4350
Wire Wire Line
	9650 4350 10550 4350
Wire Wire Line
	9250 4300 9250 4450
Wire Wire Line
	9250 4450 10550 4450
Wire Wire Line
	9650 5250 9650 5300
Wire Wire Line
	9650 5300 10550 5300
Wire Wire Line
	9250 5250 9250 5400
Wire Wire Line
	9250 5400 10550 5400
Wire Wire Line
	9650 6200 9650 6250
Wire Wire Line
	9650 6250 10550 6250
Wire Wire Line
	9250 6200 9250 6350
Wire Wire Line
	9250 6350 10550 6350
Wire Wire Line
	1150 4750 2000 4750
Wire Wire Line
	1700 4850 1700 5050
Wire Wire Line
	1700 5050 2200 5050
Wire Wire Line
	1150 4850 1700 4850
Wire Wire Line
	1150 4950 1600 4950
Wire Wire Line
	1600 4950 1600 5350
Wire Wire Line
	1600 5350 2400 5350
Wire Wire Line
	2000 4350 3050 4350
Wire Wire Line
	3050 4350 3050 4550
Wire Wire Line
	3050 4650 2200 4650
Wire Wire Line
	2400 4950 3050 4950
Wire Wire Line
	3050 4950 3050 4850
Connection ~ 3050 4650
Wire Wire Line
	3050 4950 3050 5150
Connection ~ 3050 4950
Wire Wire Line
	1150 4650 1450 4650
Wire Wire Line
	1450 4650 1450 4500
Wire Notes Line
	3150 4250 3150 5500
Wire Notes Line
	3150 5500 650  5500
Wire Notes Line
	10900 450  10900 1750
Wire Notes Line
	10900 1750 4200 1750
Wire Notes Line
	4200 500  10900 500 
Wire Notes Line
	4200 500  4200 1750
Connection ~ 10350 2350
Wire Wire Line
	10450 1050 10450 1900
Wire Wire Line
	7550 2650 7550 2850
Wire Wire Line
	10350 1150 10350 2350
Connection ~ 10450 1900
Wire Wire Line
	9900 2750 10600 2750
Wire Wire Line
	10450 1900 10450 2650
Wire Wire Line
	10600 2650 10450 2650
Connection ~ 10450 2650
Connection ~ 10350 3150
Wire Wire Line
	10350 2350 10350 3150
Wire Wire Line
	9900 2750 9900 2800
Wire Wire Line
	9600 3150 9900 3150
Wire Wire Line
	9900 3100 9900 3150
Connection ~ 9900 3150
Wire Wire Line
	9900 3150 10350 3150
Wire Wire Line
	10350 3150 10350 3600
Connection ~ 10350 3600
Wire Wire Line
	10450 2650 10450 3700
Connection ~ 10450 3700
Wire Wire Line
	7250 1650 7250 1550
Connection ~ 7250 1550
Wire Notes Line
	650  4250 3150 4250
Wire Wire Line
	1150 6600 1400 6600
Wire Wire Line
	1150 6150 1400 6150
Wire Wire Line
	1800 750  1800 900 
Wire Wire Line
	1800 1050 1800 900 
Wire Wire Line
	1800 3850 1800 4000
Wire Wire Line
	3850 1650 3850 1450
Wire Wire Line
	6750 3050 6450 3050
Wire Wire Line
	6450 2250 2400 2250
Wire Wire Line
	6350 2350 2400 2350
Wire Wire Line
	6250 2450 6250 4950
Wire Wire Line
	6150 2550 2400 2550
Wire Wire Line
	6150 2550 6150 5900
Wire Wire Line
	1400 6700 1400 6600
Wire Wire Line
	2300 4550 2500 4550
Wire Wire Line
	2500 3250 2400 3250
Wire Wire Line
	2500 4850 2600 4850
Wire Wire Line
	2600 3150 2400 3150
Wire Wire Line
	3250 2950 2400 2950
Wire Wire Line
	1150 6400 1500 6400
Wire Notes Line
	3150 5550 3150 6950
Wire Notes Line
	3150 6950 650  6950
Wire Notes Line
	650  5550 3150 5550
Wire Wire Line
	1400 6150 1400 6600
Connection ~ 1400 6600
Wire Wire Line
	7550 2250 6550 2250
Wire Wire Line
	6550 2250 6550 2150
Connection ~ 7550 2250
Wire Wire Line
	6550 2150 2400 2150
Wire Wire Line
	6450 2250 6450 3050
Wire Wire Line
	6350 2350 6350 4000
Wire Wire Line
	2500 4550 2500 3250
Wire Wire Line
	2600 4850 2600 3150
Wire Wire Line
	2700 5150 2700 3050
Wire Wire Line
	3250 5950 3250 2950
Wire Notes Line
	650  5500 650  4250
Wire Wire Line
	1150 7200 3550 7200
Wire Wire Line
	3550 2750 2400 2750
Wire Wire Line
	1150 7300 3650 7300
Wire Wire Line
	3650 2650 2400 2650
Wire Notes Line
	650  7000 3150 7000
Wire Notes Line
	3150 7000 3150 7750
Wire Notes Line
	3150 7750 650  7750
Wire Wire Line
	3550 7200 3550 2750
Wire Wire Line
	3650 7300 3650 2650
Wire Wire Line
	1150 7400 1400 7400
Wire Wire Line
	1400 7400 1400 7550
Wire Wire Line
	1150 7500 2400 7500
Wire Wire Line
	2400 7450 2400 7500
Wire Notes Line
	6700 1850 10900 1850
Wire Notes Line
	10900 3350 6700 3350
Wire Notes Line
	6700 6500 10900 6500
Wire Notes Line
	10900 1850 10900 6500
Wire Notes Line
	6700 1850 6700 6500
Wire Wire Line
	4300 5800 3900 5800
Wire Wire Line
	3900 5050 4000 5050
Connection ~ 3900 5050
Wire Wire Line
	3900 6350 3900 5800
Connection ~ 3900 5800
Wire Wire Line
	4300 6350 4300 5800
Wire Wire Line
	4300 3750 4300 3650
Wire Wire Line
	5100 3750 5100 3650
Wire Wire Line
	5100 4250 4900 4250
Connection ~ 4900 4250
Wire Wire Line
	4500 3350 4900 4250
Wire Wire Line
	4900 3350 4500 4250
Connection ~ 4500 4250
Wire Wire Line
	4500 4250 4300 4250
$Comp
L Connector:USB_C_Receptacle_USB2.0 J?
U 1 1 6236779C
P 5100 7250
F 0 "J?" V 5253 7979 50  0000 L CNN
F 1 "USB_C_Receptacle_USB2.0" V 5162 7979 50  0000 L CNN
F 2 "" H 5250 7250 50  0001 C CNN
F 3 "https://www.usb.org/sites/default/files/documents/usb_type-c.zip" H 5250 7250 50  0001 C CNN
	1    5100 7250
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5000 6650 5000 6550
Wire Wire Line
	5100 6650 5100 6550
Wire Wire Line
	6000 7250 6000 7550
Wire Wire Line
	6000 7250 6350 7250
Wire Wire Line
	6350 7250 6350 7450
Connection ~ 6000 7250
$Comp
L power:GND #PWR?
U 1 1 6240A3D8
P 6350 7450
F 0 "#PWR?" H 6350 7200 50  0001 C CNN
F 1 "GND" H 6355 7277 50  0000 C CNN
F 2 "" H 6350 7450 50  0001 C CNN
F 3 "" H 6350 7450 50  0001 C CNN
	1    6350 7450
	-1   0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 6240AE6E
P 3900 7450
F 0 "#PWR?" H 3900 7200 50  0001 C CNN
F 1 "GND" H 3905 7277 50  0000 C CNN
F 2 "" H 3900 7450 50  0001 C CNN
F 3 "" H 3900 7450 50  0001 C CNN
	1    3900 7450
	-1   0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 6240B8A4
P 4300 7450
F 0 "#PWR?" H 4300 7200 50  0001 C CNN
F 1 "GND" H 4305 7277 50  0000 C CNN
F 2 "" H 4300 7450 50  0001 C CNN
F 3 "" H 4300 7450 50  0001 C CNN
	1    4300 7450
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3900 6650 3900 7450
Wire Wire Line
	4300 6650 4300 7450
Wire Wire Line
	4700 6650 4700 6550
Wire Wire Line
	4800 6650 4800 6550
$Comp
L power:GND #PWR?
U 1 1 62555CD0
P 4700 6200
F 0 "#PWR?" H 4700 5950 50  0001 C CNN
F 1 "GND" H 4705 6027 50  0000 C CNN
F 2 "" H 4700 6200 50  0001 C CNN
F 3 "" H 4700 6200 50  0001 C CNN
	1    4700 6200
	1    0    0    1   
$EndComp
Wire Wire Line
	4700 6250 4700 6200
$Comp
L power:GND #PWR?
U 1 1 625742F5
P 4800 6200
F 0 "#PWR?" H 4800 5950 50  0001 C CNN
F 1 "GND" H 4805 6027 50  0000 C CNN
F 2 "" H 4800 6200 50  0001 C CNN
F 3 "" H 4800 6200 50  0001 C CNN
	1    4800 6200
	1    0    0    1   
$EndComp
Wire Wire Line
	4800 6250 4800 6200
Wire Wire Line
	5000 6550 5100 6550
Wire Wire Line
	5200 6550 5300 6550
Wire Wire Line
	5200 6550 5200 6650
Wire Wire Line
	5300 6550 5300 6650
Text GLabel 4700 4400 1    50   Input ~ 0
TXD
Text GLabel 4600 4400 1    50   Input ~ 0
RXD
Wire Wire Line
	4700 4450 4700 4400
Wire Wire Line
	4600 4450 4600 4400
Text GLabel 3000 1350 2    50   Input ~ 0
TXD
$Comp
L Device:R R?
U 1 1 625EE269
P 2700 1350
F 0 "R?" V 2493 1350 50  0000 C CNN
F 1 "470R" V 2584 1350 50  0000 C CNN
F 2 "" V 2630 1350 50  0001 C CNN
F 3 "~" H 2700 1350 50  0001 C CNN
	1    2700 1350
	0    1    1    0   
$EndComp
Wire Wire Line
	6250 2450 2400 2450
Wire Wire Line
	2700 3050 2400 3050
$Comp
L RF_Module:ESP32-WROOM-32 U?
U 1 1 601F033C
P 1800 2450
F 0 "U?" H 1800 4031 50  0000 C CNN
F 1 "ESP32-WROOM-32" H 1800 3940 50  0000 C CNN
F 2 "RF_Module:ESP32-WROOM-32" H 1800 950 50  0001 C CNN
F 3 "https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf" H 1500 2500 50  0001 C CNN
	1    1800 2450
	1    0    0    -1  
$EndComp
Wire Wire Line
	2400 1350 2550 1350
Wire Wire Line
	2850 1350 3000 1350
Text GLabel 3000 1550 2    50   Input ~ 0
RXD
Wire Wire Line
	3000 1550 2400 1550
Wire Wire Line
	1000 1200 1000 1250
$Comp
L Device:R R?
U 1 1 6048AF6F
P 1000 1050
F 0 "R?" V 793 1050 50  0000 C CNN
F 1 "10k" V 884 1050 50  0000 C CNN
F 2 "" V 930 1050 50  0001 C CNN
F 3 "~" H 1000 1050 50  0001 C CNN
	1    1000 1050
	1    0    0    1   
$EndComp
Wire Wire Line
	1000 900  1800 900 
Wire Wire Line
	1200 1250 1000 1250
Connection ~ 1000 1250
Wire Wire Line
	1000 1250 1000 1350
$Comp
L Switch:SW_Push SW?
U 1 1 62694816
P 1000 1850
F 0 "SW?" V 1046 1802 50  0000 R CNN
F 1 "SW_Push" V 955 1802 50  0000 R CNN
F 2 "" H 1000 2050 50  0001 C CNN
F 3 "~" H 1000 2050 50  0001 C CNN
	1    1000 1850
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 6269554E
P 1000 2150
F 0 "#PWR?" H 1000 1900 50  0001 C CNN
F 1 "GND" H 1005 1977 50  0000 C CNN
F 2 "" H 1000 2150 50  0001 C CNN
F 3 "" H 1000 2150 50  0001 C CNN
	1    1000 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	1000 2150 1000 2050
Wire Wire Line
	1000 1250 700  1250
$Comp
L Device:C C?
U 1 1 626BEF05
P 700 1850
F 0 "C?" H 815 1896 50  0000 L CNN
F 1 "10µF" H 815 1805 50  0000 L CNN
F 2 "" H 738 1700 50  0001 C CNN
F 3 "~" H 700 1850 50  0001 C CNN
	1    700  1850
	1    0    0    -1  
$EndComp
Wire Wire Line
	700  2000 700  2050
Wire Wire Line
	700  2050 1000 2050
Connection ~ 1000 2050
Wire Wire Line
	700  1250 700  1700
$Comp
L Device:R R?
U 1 1 6270437D
P 1000 1500
F 0 "R?" H 930 1454 50  0000 R CNN
F 1 "470R" H 930 1545 50  0000 R CNN
F 2 "" V 930 1500 50  0001 C CNN
F 3 "~" H 1000 1500 50  0001 C CNN
	1    1000 1500
	-1   0    0    1   
$EndComp
Text GLabel 700  1050 1    50   Input ~ 0
EN
Text GLabel 5350 3350 2    50   Input ~ 0
EN
Wire Wire Line
	4050 3350 4100 3350
Wire Wire Line
	700  1050 700  1250
Connection ~ 700  1250
Text GLabel 4050 3350 0    50   Input ~ 0
IO0
Wire Wire Line
	5300 3350 5350 3350
Text GLabel 3000 1250 2    50   Input ~ 0
IO0
Wire Wire Line
	3000 1250 2400 1250
$Comp
L Device:LED D?
U 1 1 6277F21E
P 3300 1450
F 0 "D?" H 3293 1195 50  0000 C CNN
F 1 "LED" H 3293 1286 50  0000 C CNN
F 2 "" H 3300 1450 50  0001 C CNN
F 3 "~" H 3300 1450 50  0001 C CNN
	1    3300 1450
	-1   0    0    1   
$EndComp
$Comp
L Device:R R?
U 1 1 62783C89
P 2700 1450
F 0 "R?" V 2907 1450 50  0000 C CNN
F 1 "470R" V 2816 1450 50  0000 C CNN
F 2 "" V 2630 1450 50  0001 C CNN
F 3 "~" H 2700 1450 50  0001 C CNN
	1    2700 1450
	0    -1   -1   0   
$EndComp
Wire Wire Line
	3450 1450 3850 1450
Connection ~ 3850 1450
Wire Wire Line
	3150 1450 2850 1450
Wire Wire Line
	2550 1450 2400 1450
Wire Wire Line
	2250 900  3850 900 
Wire Wire Line
	3850 900  3850 1450
Wire Notes Line
	6100 3000 6100 7750
Wire Notes Line
	6100 7750 3750 7750
Wire Notes Line
	3750 7750 3750 3000
Wire Notes Line
	3750 3000 6100 3000
Text Notes 5550 3100 0    50   ~ 0
USB Interface
$Comp
L Interface_USB:CP2102N-A01-GQFN24 U?
U 1 1 61EF3DD6
P 4900 5050
F 0 "U?" V 4946 4106 50  0000 R CNN
F 1 "CP2102" V 4855 4106 50  0000 R CNN
F 2 "Package_DFN_QFN:QFN-24-1EP_4x4mm_P0.5mm_EP2.6x2.6mm" H 5050 4100 50  0001 L CNN
F 3 "https://www.silabs.com/documents/public/data-sheets/cp2102.pdf" H 4350 6300 50  0001 C CNN
	1    4900 5050
	0    -1   -1   0   
$EndComp
$Comp
L Device:R R?
U 1 1 62519D63
P 4800 6400
F 0 "R?" H 4870 6446 50  0000 L CNN
F 1 "5k1" H 4870 6355 50  0000 L CNN
F 2 "" V 4730 6400 50  0001 C CNN
F 3 "~" H 4800 6400 50  0001 C CNN
	1    4800 6400
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 6245B412
P 4700 6400
F 0 "R?" H 4770 6446 50  0000 L CNN
F 1 "5k1" H 4770 6355 50  0000 L CNN
F 2 "" V 4630 6400 50  0001 C CNN
F 3 "~" H 4700 6400 50  0001 C CNN
	1    4700 6400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5300 6550 5300 5650
Connection ~ 5300 6550
Wire Wire Line
	5100 6550 5100 6250
Wire Wire Line
	5100 6250 5400 6250
Wire Wire Line
	5400 6250 5400 5650
Connection ~ 5100 6550
Wire Wire Line
	5200 5650 5200 5900
Wire Wire Line
	5200 5900 4500 5900
Wire Wire Line
	4500 5900 4500 6650
Connection ~ 3900 5150
Wire Wire Line
	3900 5150 3900 5050
Wire Wire Line
	3900 5150 3900 5800
Wire Wire Line
	3900 5150 4000 5150
Wire Wire Line
	3900 4250 3900 5050
Wire Wire Line
	4900 5650 4900 5800
Wire Wire Line
	4900 5800 4300 5800
Connection ~ 4300 5800
Wire Wire Line
	5800 4950 5800 5050
$Comp
L power:GND #PWR?
U 1 1 6297F5C0
P 5800 6000
F 0 "#PWR?" H 5800 5750 50  0001 C CNN
F 1 "GND" H 5805 5827 50  0000 C CNN
F 2 "" H 5800 6000 50  0001 C CNN
F 3 "" H 5800 6000 50  0001 C CNN
	1    5800 6000
	-1   0    0    -1  
$EndComp
Wire Wire Line
	5800 6000 5800 5050
Connection ~ 5800 5050
Wire Wire Line
	4500 4450 4500 4250
Wire Wire Line
	4900 4250 4900 4450
Wire Wire Line
	5100 4250 5100 4050
Wire Wire Line
	4300 4250 4300 4050
$Comp
L Device:R R?
U 1 1 62D5F57C
P 2900 4550
F 0 "R?" V 2693 4550 50  0000 C CNN
F 1 "68k" V 2784 4550 50  0000 C CNN
F 2 "" V 2830 4550 50  0001 C CNN
F 3 "~" H 2900 4550 50  0001 C CNN
	1    2900 4550
	0    1    1    0   
$EndComp
Connection ~ 3050 4550
Wire Wire Line
	3050 4550 3050 4650
$Comp
L Device:R R?
U 1 1 62D62FB1
P 2900 4850
F 0 "R?" V 2693 4850 50  0000 C CNN
F 1 "68k" V 2784 4850 50  0000 C CNN
F 2 "" V 2830 4850 50  0001 C CNN
F 3 "~" H 2900 4850 50  0001 C CNN
	1    2900 4850
	0    1    1    0   
$EndComp
Connection ~ 3050 4850
Wire Wire Line
	3050 4850 3050 4650
$Comp
L Device:R R?
U 1 1 62D634D3
P 2900 5150
F 0 "R?" V 2693 5150 50  0000 C CNN
F 1 "68k" V 2784 5150 50  0000 C CNN
F 2 "" V 2830 5150 50  0001 C CNN
F 3 "~" H 2900 5150 50  0001 C CNN
	1    2900 5150
	0    1    1    0   
$EndComp
Connection ~ 3050 5150
Wire Wire Line
	3050 5150 3050 5250
Wire Wire Line
	2750 4550 2500 4550
Connection ~ 2500 4550
Wire Wire Line
	2750 4850 2600 4850
Connection ~ 2600 4850
Wire Wire Line
	2750 5150 2700 5150
Connection ~ 2700 5150
Wire Wire Line
	1150 5950 1500 5950
Wire Wire Line
	1150 6050 2150 6050
Wire Wire Line
	2150 6050 2150 5850
Wire Wire Line
	1650 5850 1500 5850
Wire Wire Line
	1500 5850 1500 5950
Connection ~ 1500 5950
Wire Wire Line
	1500 5950 2350 5950
Wire Wire Line
	1950 5850 2150 5850
Connection ~ 2150 5850
Wire Wire Line
	2150 5850 2150 5800
Wire Wire Line
	2350 6400 2350 5950
Connection ~ 2350 5950
Wire Wire Line
	2350 5950 3250 5950
Wire Wire Line
	1150 6500 2150 6500
Wire Wire Line
	2150 6500 2150 6300
Connection ~ 2150 6050
$Comp
L Device:R R?
U 1 1 62F00A2B
P 1800 6300
F 0 "R?" V 1593 6300 50  0000 C CNN
F 1 "4k7" V 1684 6300 50  0000 C CNN
F 2 "" V 1730 6300 50  0001 C CNN
F 3 "~" H 1800 6300 50  0001 C CNN
	1    1800 6300
	0    1    1    0   
$EndComp
Wire Wire Line
	1650 6300 1500 6300
Wire Wire Line
	1500 6300 1500 6400
Connection ~ 1500 6400
Wire Wire Line
	1500 6400 2350 6400
Wire Wire Line
	1950 6300 2150 6300
Connection ~ 2150 6300
Wire Wire Line
	2150 6300 2150 6050
Wire Notes Line
	650  5550 650  7750
$EndSCHEMATC
