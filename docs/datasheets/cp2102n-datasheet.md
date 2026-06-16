

USBXpress™ Family
CP2102N Data Sheet
The  CP2102N  devices,  part  of  the  USBXpress  family,  are  de-
signed  to  quickly  add  USB  to  your  applications  by  eliminating
firmware complexity and reducing development time.
These highly-integrated USB-to-UART bridge controllers provide a simple solution for
updating RS-232 designs to USB using a minimum of components and PCB space.
CP2102N includes a USB 2.0 full-speed function controller, USB transceiver, oscillator,
and Universal Asynchronous Receiver/Transmitter (UART) in packages as small as 3
mm x 3 mm. No other external USB components are required for development. All cus-
tomization and configuration options can be selected using a simple GUI-based config-
urator.  By  eliminating  the  need  for  complex  firmware  and  driver  development,  the
CP2102N devices enable quick USB connectivity with minimal development effort.
CP2102N is ideal for a wide range of applications, including the following:
## KEY FEATURES
- No firmware development required
- Simple GUI-based configurator
- Integrated USB transceiver; no external
resistors required
- Integrated clock; no external crystal
required
- USB 2.0 full-speed compatible
- Data transfer rates up to 3 Mbaud
- USB Battery Charger Detection (USB BCS
## 1.2 Specification)
- Remote wakeup for waking a suspended
host
- Low operating current : 9.5 mA
- Royalty-free Virtual COM port drivers
- POS terminals
- USB dongles
- Gaming controllers
- Medical equipment
- Data loggers
External RS-232
transceiver or
UART circuitry
## External
circuitry for
USB suspend
states
## CP2102N
48 MHz
## Oscillator
USB Battery
## Charger
## Detect
## Remote
## Wakeup
USB Connector
External battery
charging
circuitry
## Voltage
## Regulator
USB Function
## Controller
512 byte
RX Buffer
960 byte
## Configuration
## Memory
## UART
## Hardware
## Handshaking
## RS485
## USB
## Transceiver
512 byte
TX Buffer
Feature is related to:
UARTUSBOverall System
silabs.com | Building a more connected world.Rev. 1.5

-  Feature List and Ordering Information
## 2102N
## –
## A
## –
## R
Tape and Reel (Optional)
Package Type — QFN20, QFN24, or QFN28
## Firmware Revision
## Hardware Revision
## 01
## CP
## G
## QFN20
Temperature Grade — –40 to +85 °C (G)
## Silicon Labs Xpress Product Line
USBXpress Family, USB-to-UART Bridge
Figure 1.1.  CP2102N Part Numbering
The CP2102N devices have the following features:
•Single-Chip USB-to-UART Data Transfer
- Integrated USB transceiver; no external resistors required
- Integrated clock; no external crystal required
- Internal 960-byte programmable ROM for vendor ID, prod-
uct ID, serial number, power descriptor, release number,
and product description strings
- On-chip power-on reset circuit
- On-chip voltage regulator — 3.3 V output
- Pin compatible with CP2101/2/9 (QFN28 package)
- Pin compatible with CP2104 (QFN24 package)
•USB Function Controller
- USB Specification 2.0 compliant; full-speed (12 Mbps)
- USB suspend states supported via SUSPEND pins
- USB Battery Charger Detection (USB BCS 1.2 Specifica-
tion)
- Remote wakeup for waking a suspended host
•Single power supply of 3.0 to 3.6 V or 3.0 to 5.25 V
•Universal Asynchronous Receiver/Transmitter (UART)
- All handshaking and modem interface signals
- Data formats supported
- Data bits — 5, 6, 7, and 8
- Stop bits — 1, 1.5, and 2
- Parity — odd, even, mark, space, no parity
- Baud rates: 300 baud to 3 Mbaud
- 512 byte receive buffer
- 512 byte transmit buffer
- Hardware or Xon/Xoff handshaking supported
•Virtual COM Port Device Drivers
- Works with existing COM port Applications
- Supported on Windows, Mac, and Linux
- Royalty-free distribution license
•Direct Driver Support
- Royalty-free distribution license
## Table 1.1.  Product Selection Guide
OrderingPart Number
GPIOs
## Battery Charger Detect
Separate VIO
and VDD Pins
## Pb-free
(RoHS Compliant)
Temperature RangePackage
CP2102N-A02-GQFN287Yes—Yes-40 to +85 °CQFN28
CP2102N-A02-GQFN244—YesYes-40 to +85 °CQFN24
CP2102N-A02-GQFN204——Yes-40 to +85 °CQFN20
## Note:
1.Devices with the same ordering part number may have different types of pin 1 indicators. However, all of these variants can use
the same landing diagram as long as the recommended landing diagram instructions are followed.

CP2102N Data Sheet
Feature List and Ordering Information
silabs.com | Building a more connected world.Rev. 1.5  |  2

Table of Contents
-  Feature List and Ordering Information......................2
## 2.  Typical Connection Diagrams.........................5
## 2.1  Power.................................5
## 2.2  Battery Charger Detect...........................7
## 2.3  USB.................................8
## 3.  Electrical Specifications..........................10
## 3.1  Electrical Characteristics..........................10
## 3.1.1  Recommended Operating Conditions.....................10
## 3.1.2  Power Consumption...........................10
3.1.3  Reset and Supply Monitor.........................11
## 3.1.4  Configuration Memory..........................11
## 3.1.5  Internal Oscillator............................11
## 3.1.6  5 V Voltage Regulator..........................12
## 3.1.7  GPIO................................13
3.1.8  USB Transceiver............................14
## 3.2  Thermal Conditions............................14
## 3.3  Absolute Maximum Ratings..........................15
## 3.4  Typical Performance Curves.........................16
## 4.  Functional Description...........................17
4.1  USB Function Controller and Transceiver.....................17
4.2  Universal Asynchronous Receiver/Transmitter (UART) Interface.............17
## 4.2.1  Baud Rate Generation..........................18
## 4.2.2  Sending Break Signaling.........................18
## 4.3  Additional Features............................19
4.3.1  General Purpose Input/Outputs (GPIO)....................19
## 4.3.2  Dynamic Suspend...........................19
## 4.3.3  Output Mode.............................19
4.3.4  Battery Charging (CHREN, CHR0, and CHR1)..................20
4.3.5  Remote Wakeup (WAKEUP)........................20
4.3.6  Clock Output (CLK)...........................20
4.3.7  Hardware Handshaking (RTS and CTS)....................21
## 4.3.8  Software Handshaking..........................22
## 4.3.9  Data Throughput Optimization.......................22
4.3.10  Transmit and Receive LED Toggles (TXT and RXT)...............23
4.3.11  Modem Control (DSR, DTR, DCD, RI)....................23
## 4.3.12  RS485 (RS485)............................24
## 4.3.13  Receiver Timeout...........................24
## 4.4  Drivers.................................24
4.4.1  Virtual COM Port (VCP) Drivers.......................25
4.4.2  USBXpress Drivers...........................25
4.4.3  Customization and Certification.......................25
silabs.com
| Building a more connected world.Rev. 1.5 |  3

## 4.5  Device Customization...........................26
## 5.  Pin Definitions..............................27
5.1  CP2102N QFN28 Pin Definitions........................27
5.2  CP2102N QFN24 Pin Definitions........................29
5.3  CP2102N QFN20 Pin Definitions........................31
-  QFN28 Package Specifications........................33
6.1   QFN28 Package Dimensions.........................33
6.2   QFN28 PCB Land Pattern..........................35
6.3  QFN28 Package Marking..........................36
-  QFN24 Package Specifications........................37
7.1  QFN24 Package Dimensions.........................37
7.2  PCB Land Pattern.............................39
## 7.3  Package Marking.............................40
-  QFN20 Package Specifications........................41
8.1   QFN20 Package Dimensions.........................41
8.2   QFN20 PCB Land Pattern..........................43
8.3  QFN20 Package Marking..........................44
## 9.  Relevant Application Notes.........................45
## 10.  Revision History.............................46
silabs.com
| Building a more connected world.Rev. 1.5 |  4

## 2.  Typical Connection Diagrams
## 2.1  Power
In all cases, a 1 kΩ pull-up on the RSTb pin is recommended. This pull-up should be tied to VIO on devices that have it. On devices
where VIO is connected to VDD or devices that do not have VIO, this pull-up should be tied to VDD. The RSTb pin will be driven low
during power-on and power failure reset events.
The figure below shows a typical connection diagram for the power pins of the CP2102N devices when the internal regulator is used
and USB is connected (bus-powered).
CP2102N Device
## Voltage
## Regulator
## VREGIN
## GND
4.7 μF and 0.1 μF bypass
capacitors required for
each power pin placed as
close to the pins as
possible.
## 3.3 V (out)
## VDD
USB 5 V (in)
RSTb
## VIO
Figure 2.1.  Connection Diagram with Voltage Regulator Used and USB Connected (Bus-Powered)
The figure below shows a typical connection diagram for the power pins of the CP2102N devices when the internal regulator is used
and USB is connected (self-powered).
CP2102N Device
## Voltage
## Regulator
## VREGIN
## GND
4.7 μF and 0.1 μF bypass
capacitors required for
each power pin placed as
close to the pins as
possible.
## 3.3 V (out)
## VDD
## 3.6-5.25 V (in)
RSTb
## VIO
Figure 2.2.  Connection Diagram with Voltage Regulator Used and USB Connected (Self-Powered)
CP2102N Data Sheet
## Typical Connection Diagrams
silabs.com | Building a more connected world.Rev. 1.5  |  5

The figure below shows a typical connection diagram for the power pins of the CP2102N devices when the internal 5 V-to-3.3 V regula-
tor is not used.
CP2102N Device
## Voltage
## Regulator
## 3.0-3.6 V (in)
## GND
4.7 μF and 0.1 μF bypass
capacitors required for
each power pin placed as
close to the pins as
possible.
## VREGIN
## VDD
RSTb
## VIO
1 kohm
Figure 2.3.  Connection Diagram with Voltage Regulator Not Used
CP2102N Data Sheet
## Typical Connection Diagrams
silabs.com | Building a more connected world.Rev. 1.5  |  6

## 2.2  Battery Charger Detect
The CP2102N Battery Charger Detect notifies an external battery charger the amount of current available from the USB interface.
The figure below shows an example connection diagram for external battery charging circuitry. If using an external battery charging IC,
consult the data sheet for more information about the specific recommended connection diagrams.
CP2102N Device
## D+
## GND
## VREGIN
## D-
## USB
## Connector
## VBUS
## D+
Signal GND
## D-
## VBUS
## CHR1 (1.5 A)
CHR0 (500 mA)
CHREN (100 mA)
## External
## Battery
Charging IC
## SETI
## CEN
SP0503BAHTG or
equivalent USB ESD
protection diodes
(Recommended)
22.1 kΩ
47.5 kΩ
## Figure 2.4.  Battery Charging Connection Diagram
CP2102N Data Sheet
## Typical Connection Diagrams
silabs.com | Building a more connected world.Rev. 1.5  |  7

## 2.3  USB
The figure below shows a typical connection bus-powered diagram for the USB pins of the CP2102N devices including ESD protection
diodes on the USB pins.
Note: There are two relevant restrictions on the VBUS pin voltage in self-powered and bus-powered configurations. The first is the ab-
solute maximum voltage on the VBUS pin, which is defined as VIO + 2.5 V in Table 3.10 Absolute Maximum Ratings on page 15. The
second is the Input High Voltage (VIH) for VBUS to detect when the device is connected to a bus, which is defined as VIO – 0.6 V in
Table 3.7 GPIO on page 13. A resistor divider (or functionally-equivalent circuit) on VBUS is required to meet these specifications and
ensure reliable device operation. In this case, the current limitation of the resistor divider prevents high VBUS pin leakage current, even
though the VIO + 2.5 V specification is not strictly met while the device is not powered.

CP2102N Device
## USB
## D+
## GND
## VREGIN
## D-
## USB
## Connector
## VBUS
## D+
Signal GND
## D-
SP0503BAHTG or
equivalent USB ESD
protection diodes
(Recommended)
## VBUS
22.1 kΩ
47.5 kΩ
Figure 2.5.  Bus-Powered Connection Diagram for USB Pins
CP2102N Data Sheet
## Typical Connection Diagrams
silabs.com | Building a more connected world.Rev. 1.5  |  8

The figure below shows a typical connection self-powered diagram for the USB pins of the CP2102N devices including ESD protection
diodes on the USB pins.
Note: There are two relevant restrictions on the VBUS pin voltage in self-powered and bus-powered configurations. The first is the ab-
solute maximum voltage on the VBUS pin, which is defined as VIO + 2.5 V in Table 3.10 Absolute Maximum Ratings on page 15. The
second is the Input High Voltage (VIH) for VBUS to detect when the device is connected to a bus, which is defined as VIO – 0.6 V in
Table 3.7 GPIO on page 13. A resistor divider (or functionally-equivalent circuit) on VBUS is required to meet these specifications and
ensure reliable device operation. In this case, the current limitation of the resistor divider prevents high VBUS pin leakage current, even
though the VIO + 2.5 V specification is not strictly met while the device is not powered.

CP2102N Device
## USB
## D+
## GND
## VBUS
## D-
## USB
## Connector
## VBUS
## D+
Signal GND
## D-
SP0503BAHTG or
equivalent USB ESD
protection diodes
(Recommended)
22.1 k
47.5 k
Figure 2.6.  Self-Powered Connection Diagram for USB Pins
CP2102N Data Sheet
## Typical Connection Diagrams
silabs.com | Building a more connected world.Rev. 1.5  |  9

## 3.  Electrical Specifications
## 3.1  Electrical Characteristics
All electrical parameters in all tables are specified under the conditions listed in Table 3.1 Recommended Operating Conditions on page
10, unless stated otherwise.
## 3.1.1  Recommended Operating Conditions
## Table 3.1.  Recommended Operating Conditions
ParameterSymbolTest ConditionMinTypMaxUnit
Operating Supply Voltage on VDD
## 1
## V
## DD
## 3.0—3.6V
Operating Supply Voltage on VIO
## 3
## V
## IO
## 1.71—V
## DD
## V
Operating Supply Voltage on VRE-
## GIN
## V
## REGIN
## 3.0—5.25V
Operating Ambient TemperatureT
## A
## -40—85°C
## Note:
1.Standard USB compliance tests require 3.0 V on VDD for compliant operation.
2.All voltages with respect to GND.
3.On devices without a VIO pin, V
## IO
## = V
## DD
## .
4.GPIO levels are undefined whenever VIO is less than 1 V.

## 3.1.2  Power Consumption
## Table 3.2.  Power Consumption
ParameterSymbolTest ConditionMinTypMaxUnit
## Normal Operation
## 1, 2
## I
## DD
115200 baud transmitting continu-
ous bidirectional data
—9.5—mA
3 Mbaud transmitting continuous
bidirectional data
—13.7—mA
USB Suspend
## 1, 2
## I
## DD
—195—μA
Held in Reset
## 1, 2
## I
## DD
—1.3—mA
USB Pull-up
## 3
## I
## PU
—200230μA
## Note:
1.Includes supply current from internal LDO regulator, supply monitor, and internal oscillators. These power consumption numbers
are only for the CP2102N and do not include an external RS232 transceiver or other external circuitry.
2.USB Pull-up current should be added for total supply current. Normal and suspended supply current is current flowing into VRE-
## GIN.
3.The USB Pull-up supply current values are calculated values based on USB specifications. USB Pull-up supply current is current
flowing from VDD to GND through USB pull-down/pull-up resistors on D+ and D-.

CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  10

3.1.3  Reset and Supply Monitor
Table 3.3.  Reset and Supply Monitor
ParameterSymbolTest ConditionMinTypMaxUnit
VDD Supply Monitor ThresholdV
## VDDM
## 1.952.052.15V
Power-On Reset (POR) ThresholdV
## POR
Rising Voltage on VDD —1.2—V
Falling Voltage on VDD0.75—1.36V
VDD Ramp Timet
## RMP
Time to V
## DD
## > 2.2 V10——μs
Reset Delay from PORt
## POR
Relative to V
## DD
## > V
## POR
## 31031ms
Reset Delay from non-POR sourcet
## RST
Time between release of reset
source and code execution
## —50—μs
RSTb Low Time to Generate Resett
## RSTL
## 15——μs
## Note:
1.The RSTb pin will be driven low during power-on and power failure reset events.

## 3.1.4  Configuration Memory
## Table 3.4.  Configuration Memory
ParameterSymbolTest ConditionMinTypMaxUnits
## V
## DD
## Voltage During Programming
## 1
## V
## PROG
## 3.0—3.6V
Endurance (Write/Erase Cycles)N
## WE
20k100k—Cycles
## Note:
1.The device can be safely programmed at any voltage above the supply monitor threshold (V
## VDDM
## ).
2.Data Retention Information is published in the Quarterly Quality and Reliability Report.

## 3.1.5  Internal Oscillator
## Table 3.5.  Internal Oscillator
ParameterSymbolTest ConditionMinTypMaxUnit
## Internal Oscillator Frequencyf
## OSC
Full Temperature and Supply
## Range
47.34848.7MHz
Power Supply SensitivityPSS
## OSC
## T
## A
## = 25 °C—0.02—%/V
Temperature SensitivityTS
## OSC
## V
## DD
= 3.0 V—45—ppm/°C
CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  11

## 3.1.6  5 V Voltage Regulator
Table 3.6.  5V Voltage Regulator
ParameterSymbolTest ConditionMinTypMaxUnit
## Input Voltage Range
## 1
## V
## REGIN
## 3.0—5.25V
Output Voltage on VDD
## 2
## V
## REGOUT
Output Current = 1 to 100 mA
Regulation range (VREGIN ≥ 4.1V)
## 3.13.33.6V
Output Current = 1 to 100 mA
Dropout range (VREGIN < 4.1V)
## —V
## REGIN
## –
## V
## DROPOUT
## —V
## Output Current
## 2
## I
## REGOUT
——100mA
Dropout VoltageV
## DROPOUT
Output Current = 100 mA——0.8V
## Note:
1.Input range to meet the Output Voltage on VDD specification. If the 5 V voltage regulator is not used, VREGIN should be tied to
## VDD.
2.Output current is total regulator output, including any current required by the device.

CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  12

## 3.1.7  GPIO
Table 3.7.  GPIO
ParameterSymbolTest ConditionMinTypMaxUnit
Output High Voltage (High Drive)V
## OH
## I
## OH
= -7 mA, V
## IO
## ≥ 3.0 VV
## IO
## - 0.7——V
## I
## OH
= -3.3 mA, 2.2 V ≤ V
## IO
## < 3.0 V
## I
## OH
= -1.8 mA, 1.71 V ≤ V
## IO
## < 2.2 V
## V
## IO
x 0.8——V
Output Low Voltage (High Drive)V
## OL
## I
## OL
= 13.5 mA, V
## IO
## ≥ 3.0 V——0.6V
## I
## OL
= 7 mA, 2.2 V ≤ V
## IO
## < 3.0 V
## I
## OL
= 3.6 mA, 1.71 V ≤ V
## IO
## < 2.2 V
## ——V
## IO
x 0.2V
Output High Voltage (Low Drive)V
## OH
## I
## OH
= -4.75 mA, V
## IO
## ≥ 3.0 VV
## IO
## - 0.7——V
## I
## OH
= -2.25 mA, 2.2 V ≤ V
## IO
## < 3.0 V
## I
## OH
= -1.2 mA, 1.71 V ≤ V
## IO
## < 2.2 V
## V
## IO
x 0.8——V
Output Low Voltage (Low Drive)V
## OL
## I
## OL
= 6.5 mA, V
## IO
## ≥ 3.0 V——0.6V
## I
## OL
= 3.5 mA, 2.2 V ≤ V
## IO
## < 3.0 V
## I
## OL
= 1.8 mA, 1.71 V ≤ V
## IO
## < 2.2 V
## ——V
## IO
x 0.2V
## Input High Voltage
(all GPIO pins including VBUS)
## V
## IH
## V
## IO
## - 0.6——V
## Input Low Voltage
(all GPIO including VBUS)
## V
## IL
## ——0.6V
Pin CapacitanceC
## IO
—7—pF
Weak Pull-Up Current
## (V
## IN
## = 0 V)
## I
## PU
## V
## DD
= 3.6-30-20-10μA
Input Leakage (Pullups off or Ana-
log)
## I
## LK
## GND < V
## IN
## < V
## IO
-1.1—1.1μA
Input Leakage Current with V
## IN
above V
## IO
## I
## LK
## V
## IO
## < V
## IN
## < V
## IO
+2.0 V05150μA
RS485 Setup Time before Start
## Bit
## 1
t
## RS485S
## 0—64.02ms
RS485 Hold Time after Stop Bit
## 1
t
## RS485H
## 0—64.02ms
TX Toggle Ratef
## TXTOGGLE
—20—Hz
RX Toggle Ratef
## RXTOGGLE
—20—Hz
## Note:
1.Programmable from 0 ms to 64 ms in 1 μs steps. The programmed time is the guaranteed minimum, and the actual time may be
up to 20 μs longer.

CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  13

3.1.8  USB Transceiver
Table 3.8.  USB Transceiver
ParameterSymbolTest ConditionMinTypMaxUnit
## Transmitter
Output High VoltageV
## OH
## V
## DD
## ≥ 3.0V2.8——V
Output Low VoltageV
## OL
## V
## DD
## ≥ 3.0V——0.8V
Output Crossover PointV
## CRS
## 1.3—2.0V
Output ImpedanceZ
## DRV
## Driving High
## Driving Low
## 28
## 28
## 36
## 36
## 44
## 44
## Ω
Pull-up ResistanceR
## PU
Full Speed (D+ Pull-up)1.4251.51.575kΩ
Output Rise TimeT
## R
## Full Speed4—20ns
Output Fall TimeT
## F
## Full Speed4—20ns
## Receiver
## Differential Input
## Sensitivity
## V
## DI
## | (D+) - (D-) |0.2——V
## Differential Input Common Mode
## Range
## V
## CM
## 0.8—2.5V
Input Leakage CurrentI
## L
Pullups Disabled—<1.0—μA
Refer to the USB Specification for timing diagrams and symbol definitions.
## 3.2  Thermal Conditions
## Table 3.9.  Thermal Conditions
ParameterSymbolTest ConditionMinTypMaxUnit
Thermal Resistance (Junction to
## Ambient)
θ
## JA
QFN20 Packages─60─°C/W
QFN24 Packages─30─°C/W
QFN28 Packages─26─°C/W
Thermal Resistance (Junction to
## Case)
θ
## JC
QFN20 Packages─32.9─°C/W
QFN24 Packages─24.2─°C/W
QFN28 Packages─18.8─°C/W
## Thermal Characterization Parame-
ter (Junction to Top)
## Ψ
## JT
QFN20 Packages─0.88─°C/W
QFN24 Packages─0.3─°C/W
QFN28 Packages─0.3─°C/W
## Note:
1.Thermal resistance assumes a multi-layer PCB with any exposed pad soldered to a PCB pad.

CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  14

## 3.3  Absolute Maximum Ratings
Stresses above those listed in 3.3 Absolute Maximum Ratings may cause permanent damage to the device. This is a stress rating only
and functional operation of the devices at those or any other conditions above those indicated in the operation listings of this specifica-
tion is not implied. Exposure to maximum rating conditions for extended periods may affect device reliability. For more information on
the available quality and reliability data, see the Quality and Reliability Monitor Report at http://www.silabs.com/support/quality/pages/
default.aspx.
## Table 3.10.  Absolute Maximum Ratings
ParameterSymbolTest ConditionMinMaxUnit
Ambient Temperature Under BiasT
## BIAS
## -55125°C
Storage TemperatureT
## STG
## -65150°C
Voltage on VDDV
## DD
## GND-0.34.2V
Voltage on VIO
## 2
## V
## IO
## GND-0.34.2V
Voltage on VREGINV
## REGIN
## GND-0.35.8V
Voltage on D+ or D-V
## USBD
## GND-0.3V
## DD
## +0.3V
Voltage on UART pins, GPIO, VBUS,
RSTb, or any other non-power, non-
USB pin
## V
## IN
## V
## IO
## > 3.3 VGND-0.35.8V
## V
## IO
## < 3.3 VGND-0.3V
## IO
## +2.5V
Total Current Sunk into Supply PinI
## VDD
─400mA
Total Current Sourced out of Ground
## Pin
## I
## GND
400─mA
Current Sourced or Sunk by any UART
pins, GPIO, VBUS, RSTb, or any other
non-power, non-USB pin
## I
## IO
-100100mA
Operating Junction TemperatureT
## J
## -40105°C
## Note:
1.Exposure to maximum rating conditions for extended periods may affect device reliability.
2.On devices without a VIO pin, V
## IO
## = V
## DD

CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  15

## 3.4  Typical Performance Curves
## Figure 3.1.  Typical V
## OH
## Curves
## Figure 3.2.  Typical V
## OL
## Curves
CP2102N Data Sheet
## Electrical Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  16

## 4.  Functional Description
4.1  USB Function Controller and Transceiver
The Universal Serial Bus function controller in the CP2102N is a USB 2.0 compliant full-speed device with integrated transceiver and
on-chip matching and pull-up resistors. The USB function controller manages all data transfers between the USB and the UART as well
as command requests generated by the USB host controller and commands for controlling the function of the UART.
The USB Suspend and Resume signals are supported for power management of both the CP2102N device as well as external circuitry.
The CP2102N will enter Suspend mode when Suspend signaling is detected on the bus. On entering Suspend mode, the CP2102N
asserts the SUSPEND and SUSPENDb signals. SUSPEND and SUSPENDb are also asserted after a CP2102N reset until device con-
figuration during USB Enumeration is complete.
The CP2102N exits Suspend mode when any of the following occur:
1.Resume signaling is detected or generated.
2.A USB Reset signal is detected.
3.A device reset occurs.
4.USB Remote Wakeup functionality is enabled and the WAKEUP pin is grounded.
On exit of Suspend mode, the SUSPEND and SUSPENDb signals are de-asserted. Both SUSPEND and SUSPENDb temporarily float
high during a CP2102N reset. If this behavior is undesirable, a strong pull-down (10 kΩ) can be used to ensure SUSPENDb remains
low during reset.
4.2  Universal Asynchronous Receiver/Transmitter (UART) Interface
The CP2102N UART interface consists of the TX (transmit) and RX (receive) data signals as well as the RTS, CTS, DSR, DTR, DCD,
and RI control signals. The UART supports RTS/CTS, DSR/DTR, and Xon/Xoff handshaking.
The UART is programmable to support a variety of data formats and baud rates. If the Virtual COM Port drivers are used, the data
format and baud rate are set during COM port configuration on the PC. If the USBXpress drivers are used, the CP2102N is configured
through the USBXpress API. The data formats and baud rates available are listed in the table below.
Table 4.1.  Data Formats and Baud Rates
ParameterAvailable Values
Data Bits5, 6, 7, and 8
## Stop Bits
## 1, 1.5
## 1
, and 2
Party Typesnone, even, odd, mark, space
## Baud Rates300, 600, 1200, 1800, 2400, 4000, 4800, 7200, 9600, 14400,
## 16000, 19200, 28800, 38400, 51200, 56000, 57600, 64000,
## 76800, 115200, 128000, 153600, 230400, 250000, 256000,
## 460800, 500000, 576000, 921600, 1000000, 1200000, 1500000,
## 2000000, 3000000
## Note:
1.5-bit only.

CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  17

## 4.2.1  Baud Rate Generation
The baud rate generator is very flexible, allowing the user to request any baud rate in the range from 300 baud to 3 Mbaud. If the baud
rate cannot be directly generated from the 48 MHz oscillator, the device will choose the closest possible option. The actual baud rate is
dictated by the following equations.
## Clock Divider=
48 MHz
2×Prescale×Requested Baud Rate
## Actual Baud Rate=
48 MHz
2×Prescale×Clock Divider
In both cases, the Prescale value is 4 if the Requested Baud Rate is ≤ 365 baud and 1 if the Requested Baud Rate value is > 365
baud.
Most baud rates can be generated with an error of less than 1.0%. A general rule of thumb for the majority of UART applications is to
limit the baud rate error on both the transmitter and the receiver to no more than ±2%. The Clock Divider value is rounded to the near-
est integer, which may produce an error source. Another error source will be the 48 MHz oscillator, which is accurate to ±0.25%. Know-
ing the actual and requested baud rates, the total baud rate error can be found using the equation below.
## Baud Rate Error (%)=100×
## (
## 1–
## Actual Baud Rate
## Requested Baud Rate
## )
## ±0.25%
## 4.2.2  Sending Break Signaling
The CP2102N supports break signaling with an external 10k Ohm resistor between TXD and ground. This resistor is sufficient for break
signaling across all baud rates.
When a Send Break command is received, the CP2102N halts adding new data to the transmitter FIFO and will wait 6 byte times for in-
flight data to complete transmission. It will not process other USB transactions such as RX data reception or GPIO commands while
waiting - transactions will be processed once break is initiated. During this time, if enabled, the RS-485 signal will begin asserting. If
RTS TX Control is enabled, RTS will also begin asserting. Once the 6 byte time has expired, the CP2102N places the TXD line in a
high-impedance state - ignoring flow control status - and the external resistor pulls down TXD to initiate a break.
While sending break the TXT LED toggle is active. USB transactions including RX data reception and GPIO commands function nor-
mally.
When a Stop Break command is received, the CP2102N removes TXD from the high impedance state. It is held for 1 byte time to allow
for stabilization. After that time has expired the transmitter resumes normal operations, and the RS485 and RTS (if RTS TX Control is
enabled) signals wait the specified hold time.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  18

## 4.3  Additional Features
4.3.1  General Purpose Input/Outputs (GPIO)
The CP2102N has up to 7 GPIO that can be controlled from the host. By default and during reset, these pins are set to open-drain with
a weak pull-up enabled and the port latch set to 1. The pins can be made push-pull to drive external circuitry like LEDs. In addition, the
state of these pins can be configured during standard operation, during Suspend, and immediately following reset.
Note: All pins temporarily float high during a device reset. If this behavior is undesirable, a strong pull-down (10 kΩ) can be used to
ensure the pin remains low during reset.

The GPIO pins may also have alternate functions which are listed in the table below.
Table 4.2.  GPIO Pin Alternate Functions
GPIO PinQFN28 PackageQFN24 PackageQFN20 Package
## GPIO.0TXTTXT
## CLK
## 1
## GPIO.1RXTRXTRS485
## GPIO.2RS485RS485TXT
## GPIO.3WAKEUPWAKEUPRXT
GPIO.4No alternate functionNot availableNot available
GPIO.5No alternate functionNot availableNot available
GPIO.6No alternate functionNot availableNot available
## Note:
1.On QFN28 and QFN24 packages, the CLK signal is available on the same pin as RI.

By default, all of the GPIO pins are configured as a GPIO input. The speed of reading and writing the GPIO pins is subject to the timing
of the USB bus. GPIO pins configured as inputs or outputs are not recommended for real-time signaling.
More information regarding the configuration of these pins can be found in Xpress Configurator in Simplicity Studio and AN721: CP21xx
Device Customization Guide. Guidance on GPIO usage can be found in AN223: Runtime GPIO Control for CP210x.
## 4.3.2  Dynamic Suspend
By default, the latch values for all pins remains static during USB Suspend.
Alternatively, the dynamic suspend feature sets the pin latch to a predefined state when the CP2102N device moves from the config-
ured USB state to the suspend USB state (see chapter nine of USB 2.0 specification for more information on USB device states). When
the device exits the suspend USB state, the pin latch is restored to the previous value before entering the suspend state. Dynamic
Suspend is configured separately for the GPIO pins and UART/Modem Control pins.
## 4.3.3  Output Mode
Each pin has two options for the output mode: push-pull and open-drain.
By configuring for push-pull operation, a pin operates as a push-pull output. The output voltage is determined by pin’s latch value. This
type of output is most often used to connect directly to another device or drive external circuitry like an LED.
By configuring for open-drain operation, a pin operates as an open-drain output or input. The output voltage is determined by the pin's
latch value. If the pin latch value is 1, the pin is pulled up to VIO (or VDD if the device does not have a VIO pin) through an on-chip pull-
up resistor. Open-drain outputs are typically used when interfacing to logic at a higher voltage than the VIO pin. These pins can be
safely pulled to the higher, external voltage through an external pull-up resistor if VDD meets the 3.3 Absolute Maximum Ratings re-
quirements.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  19

4.3.4  Battery Charging (CHREN, CHR0, and CHR1)
When battery charging is enabled, the D+/D- signals will detect the type of current source attached and set the CHREN, CHR0, and
CHR1 pins appropriately. CHREN enables 100 mA source current, CHR0 enables 500 mA source current, and CHR1 enables 1.5 A
source current.
The charging system may draw up to the limit specified by CHREN, CHR0, and CHR1. If the system also is operational while charging,
the current set points for the ISET resistors should be decreased based on how much the system could be using during battery charge.
When configuring a device to enable battery charging, the GPIO associated with the battery charging pins must also be configured cor-
rectly in Xpress Configurator as shown in the following table.
Table 4.3.  Configuring GPIO for Battery Charging
Charge Detect ModePinsState
Up to 100 mACHRENPush-Pull/High
CHR1Open Drain/Low
CHR0Open Drain/Low
Up to 500 mACHRENPush-Pull/High
CHR1Open Drain/Low
CHR0Push-Pull/High
Above 500 mACHRENPush-Pull/High
CHR1Push-Pull/High
CHR0Push-Pull/High
Note: Battery charging pins (CHREN, CHR1, CHR0) are disabled in Suspend only when using a Standard Data Port. If attached to a
Dedicated Charging Port or Charging Downstream Port, the battery charging pins are left on while in Suspend.

4.3.5  Remote Wakeup (WAKEUP)
The WAKEUP pin is an optional active low remote wakeup input. When the wakeup pin toggles from inactive to active (i.e. grounded)
and the CP2102N is in USB suspend, the CP2102N will begin the wakeup sequence.
Host software must enable USB remote wakeup for the device. In Windows, this is under Device Manager. To set this, right-click on the
device, select [Properties]>[Power Management] and enable the [Allow this device to wake the computer] feature.
4.3.6  Clock Output (CLK)
An optional clock output is available on CP2102N devices.
## F
## CLK
## =
48 MHz
## 2×N
The valid values for N are 1 to 256.
Note: The clock output stops and is no longer present on the pin when the CP2102N device is in USB Suspend. This occurs when the
device is connected to USB and the host controller suspends the device (either through a feature like Selective Suspend or when the
host PC is in Hibernate or Sleep modes) or when the CP2102N is disconnected from the host in self-powered mode.

CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  20

4.3.7  Hardware Handshaking (RTS and CTS)
To utilize the functionality of the RTS and CTS pins of the CP2102N, the device must be configured to use hardware flow control on the
USB host.
RTS, or Ready To Send, is an active-low output from the CP2102N and indicates to the external UART device that the CP2102N’s
UART RX FIFO has not reached the FLOW OFF watermark level of 448 bytes and is ready to accept more data. When the amount of
data in the RX FIFO reaches the watermark, the CP2102N pulls RTS high to indicate to the external UART device to stop sending data.
The CP2102N does not pull RTS low again until the UART RX FIFO is at the FLOW ON watermark level of 384 bytes (at least 128 free
bytes). This hysteresis allows for optimal operation. These RTS watermark levels are configurable using Xpress Configurator in Simplic-
ity Studio.
Note: RTS TX Control signaling is a special mode that asserts RTS while the CP2102N is transmitting. This mode is not available be-
low 300 baud. RTS hardware flow control works at all baud rates.

CTS, or Clear To Send, is an active-low input to the CP2102N and is used by the external UART device to indicate to the CP2102N
when the external UART device’s RX FIFO is getting full. The CP2102N will not send more than two bytes of data once CTS is pulled
high.
Hardware handshaking allows for optimal continuous transmission speeds at high baud rates (greater than 1 MBaud). The effective
throughput depends on USB bus loading and host USB stack efficiency. The typical maximum continuous bidirectional data transfer is
> 450 kbytes/s at 3 Mbaud.
## RS232
## System
## CP2102N
## TX
## RX
## TX
## RX
## RTS
## CTS
## RTS
## CTS
Figure 4.1.  Using Hardware Flow Control with the CP2102N
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  21

## 4.3.8  Software Handshaking
The CP2102N also supports software handshaking using the XON and XOFF event characters. The characters used for XON/XOFF is
set by the host software.
If the CP2102N receives an XOFF request, it will stop transmission, even if the CP2102N receiver needs to transmit an XOFF over
UART. This can potentially allow an overflow to occur or a deadlock condition if both the CP2102N and the connected UART device
transmit XOFF at the same time. The XOFF_CONTINUE setting allows the CP2102N transmitter to send XOFF/XON requests even if it
has received an XOFF request from the connected UART device. Once the connected UART device transmits XON, normal transmis-
sion from the CP2102N resumes.
Software handshaking uses the same watermark levels as hardware handshaking and can be configured dynamically by host software.
Watermark levels greater than 512 are converted to an XON limit of 192 bytes and an XOFF limit of 64 bytes. If the XON limit crosses
over the XOFF limit, the XON limit will automatically be modified to not cross over the XOFF limit. An XOFF limit of 0 is converted to 64
to guarantee buffer space is available until the UART end device stops transmission. When setting the XON and XOFF limits, it's rec-
ommended to use values where the XON limit added to the XOFF limit is less than 512 bytes, like 192/192 or 128/128. CP2102N
testing shows that the XON limit set to 192 and XOFF limit set to 192 provides optimal software flow control behavior.
0xF0
## RS232
## System
## CP2102N
TXRX0x400x230x510x640x87
## CP2102N
receives XON
## CP2102N
receives XOFF
## Figure 4.2.  Software Flow Control Timing Diagram
## 4.3.9  Data Throughput Optimization
Effective throughput depends on several factors:
- CP2102N placement on the physical USB device tree
- USB bus load from other devices
- Host OS USB stack efficiency
- CP2102N configuration options
Handshaking is required at high baud rates (greater than 1 MBaud) to avoid receiver overrun. A request to stop transmission is only
initiated once the RX FIFO has reached the FLOW OFF watermark level. Once the USB bus lowers the RX FIFO level below the FLOW
ON watermark, a request to continue transmission is sent.
Hardware  handshaking  allows  for  optimal  continuous  transmission  speeds  at  high  baud  rates.  Using  a  Windows  host  PC,  the
CP2102N's typical maximum continuous bidirectional throughput is > 450 kbytes/s at 3 Mbaud (> 70% efficiency).
Software handshaking using XON/XOFF transmission requires more overhead. Using a Windows host PC, the CP2102N's typical maxi-
mum continuous bidirectional throughput is > 330 kbytes/s at 3 Mbaud (> 55% efficiency).
For these performance numbers, the CP2102N is placed on a USB hub connected to the Windows host PC with a third party UART
adapter. The only significant USB traffic is generated by the USB to UART devices. The Windows host PC is running automated tests
with minimal CPU load.
Certain conditions will reduce the maximum throughput at high baud rates (> 1Mbaud):
- Using DSR, DTR, or DCD handshaking signals lowers maximum performance. Use hardware CTS/RTS only for peak performance.
- Embedded events or error character insertion requires free space in the UART RX FIFO to post events to the host. At high baud
rates with continuous data reception, this space may not be available. Limit maximum baud rates with continuous data reception to 1
MBaud when using embedded events or error character insertion to guarantee reception of events or the error character.
- Transmitting an immediate character momentarily causes lower bidirectional throughput as the character forces a bypass of the cur-
rent transmit FIFO. Once the character has been transmitted, the typical bidirectional throughput is restored.
Using Remote Wakeup, Charge Enable, Clock Out, or the GPIOs will not impact UART throughput.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  22

4.3.10  Transmit and Receive LED Toggles (TXT and RXT)
The TX and RX LED toggle pins will toggle on and off at a fixed rate specified in Table 3.7 GPIO on page 13 whenever a byte is trans-
mitted or received by the CP2102N. These pins are logic high whenever a device is not transmitting or receiving data and can directly
drive basic LEDs within the device specification limits.
## CP2102N
## TXT
## RXT
## VIO
Figure 4.3.  Transmit and Receive Toggle
4.3.11  Modem Control (DSR, DTR, DCD, RI)
The modem control pins are enabled when requested on the host. If the Virtual COM Port drivers are used, the modem control pins are
enabled during COM port configuration on the PC. If the USBXpress drivers are used, the CP2102N is configured through the USBX-
press API. The behavior of the modem control pins may vary between operating systems.
## Table 4.4.  Modem Control Signals
Modem Control SignalDescription
DSRInput to the CP2102N. Data Set Ready control input (active low).
DTROutput from the CP2102N. Data Terminal Ready control output (active low).
Note that this pin may toggle when opening a COM port on some operating systems.
DCDInput to the CP2102N. Data Carrier Detect control input (active low).
RIInput to the CP2102N. Ring Indicator control input (active low).
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  23

## 4.3.12  RS485 (RS485)
The RS485 pin is an optional control pin that can be connected to the DE and RE inputs of the transceiver. When configured for RS485
mode, the pin is asserted during UART data transmission. The RS485 pin is active-high by default and is also configurable for active-
low mode using Xpress Configurator.
The RS485 pin setup and hold times are programmable using Xpress Configurator to enable maximum flexibility.
Note: Note The RS485 pin is not available at baud rates below 300 baud.

## RS485
## Transceiver
## CP2102N
## R
## D
## DE
## RE
## TX
## RX
## RS485
Figure 4.4.  Using the CP2102N with a RS485 Transceiver
StartD0D1D2D3...DnStop
## TX
## RS485
## RS485
## Setup
## (t
## RS485S
## )
## RS485
## Hold
## (t
## RS485H
## )
Figure 4.5.  RS485 Output Timing Diagram for a Single-Byte Transfer
## 4.3.13  Receiver Timeout
The CP2102N supports a new custom vendor command to configure the internal buffer receive timeout. During normal operation, when
data is received the receive buffer waits up to 2 ms or 128 character times, whichever is fewer, before transferring data to host. This
timer is reset each time new data is received. For some usage models, this response time causes unwanted extra latency between
receiving a byte at the UART and the byte being available on the host. The Set Receiver Max Timeout custom vendor command allows
applications to set the timeout from .001 ms to 2 ms. Small values will cause the receiver to inefficiently use the 512 byte Receive
Buffer and should not be used at high data rates (greater than 230400).
## 4.4  Drivers
There are two sets of device drivers available for the CP2102N devices: the Virtual COM Port (VCP) drivers and the USBXpress Direct
Access drivers. Only one set of drivers is necessary to interface with the device.
The latest drivers are available at www.silabs.com/interface-software.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  24

4.4.1  Virtual COM Port (VCP) Drivers
The CP2102N Virtual COM Port (VCP) device drivers allow a CP2102N-based USB device to appear to the PC's application software
as a COM port. Application software running on the PC accesses the CP2102N-based device as it would access a standard hardware
COM port. However, actual data transfer between the PC and the CP2102N device is performed over the USB interface. Therefore,
existing COM port applications may be used to transfer data via the USB to the CP2102N-based device without modifying the applica-
tion. See AN197: Serial Communications Guide for the CP210x for Example Code for Interfacing to a CP2102N using the Virtual COM
drivers.
Note: Because the CP2102N uses a USB-based communication interface, timing will not be controllable or guaranteed as it is with a
standard COM port. Full-speed USB operates on 1 ms frames, and the host schedules packets for each USB device where it can in the
1 ms frame. It is recommended to use large data transfers when reading and writing from the host to send data as quickly as possible.

4.4.2  USBXpress Drivers
The Silicon Labs USBXpress drivers provide an alternate solution for interfacing with CP2102N devices. No serial port protocol exper-
tise is required. Instead, a simple, high-level application program interface (API) is used to provide simpler CP210x connectivity and
functionality. The USBXpress for CP210x Development Kit includes Windows device drivers, Windows device driver installer and unin-
stallers, and a host interface function library (host API) provided in the form of a Windows Dynamic Link Library (DLL). The USBXpress
driver set is recommended for new products that also include new PC software. The USBXpress interface is described in AN169:
USBXpress® Programmer's Guide.
4.4.3  Customization and Certification
In addition to customizing the device as described in 4.5 Device Customization, the drivers can be also be customized. See AN220:
USB Driver Customization for more information on generating customized VCP and USBXpress drivers.
The default drivers that are shipped with the CP2102N are Microsoft WHQL (Windows Hardware Quality Labs) certified. The certifica-
tion means that the drivers have been tested by Microsoft and their latest operating systems will allow the drivers to be installed without
any warnings or errors. Some installations of Windows will prevent unsigned drivers from being installed at all. The customized drivers
that are generated using the AN220 software are not automatically certified. They must first go through the Microsoft Driver Reseller
Submission process. See AN807: Recertifying a Customized Windows HCK Driver Package for more information and contact Silicon
Labs support for assistance with this process.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  25

## 4.5  Device Customization
The CP2102N includes an internal electrically erasable programmable read-only memory (EEPROM). This memory may be used to
customize the USB Vendor ID (VID), Product ID (PID), Product Description String, Power Descriptor, Device Release Number and De-
vice Serial Number as desired for OEM applications. If the EEPROM is not programmed with OEM data, the default configuration data
shown in the table below is used.
Table 4.5.  Default USB Configuration Data
NameDescriptionDefault Value
Vendor ID (VID)The Vendor ID is a four digit hexadecimal number that is
unique to a particular vendor. 0x10C4, for example, is the
Silicon Labs Vendor ID.
0x10C4
Product ID (PID)The Product ID is a four digit hexadecimal number that
identifies the vendor's device. 0xEA60, for example, is the
default Product ID for Silicon Labs' CP210x USB to UART
Bridge devices.
0xEA60
Power ModeThis setting determines whether the device is Bus-Pow-
ered, i.e. it is powered by the host, or Self-Powered, i.e. it
is powered from a supply on the device.
0x80 (Bus-Powered)
Max PowerThis describes the maximum amount of power that the de-
vice will draw from the host in mA multiplied by 2. For ex-
ample, 0x32 equates to 100 mA.
## 0x32
Release VersionThe Release Version is a binary-coded-decimal value that
is assigned by the device manufacturer.
## 0x0100
Serial StringThe Serial String is an optional string that is used by the
host to distinguish between multiple devices with the same
VID and PID combination. It is limited to 63 characters.
128-bit unique ID assigned by Silicon Labs
Product StringThe Product String is an optional string that describes the
product. It is limited to 126 characters.
"CP2102N USB to UART Bridge Controller"
While customization of the USB configuration data is optional, it is recommended to customize the VID/PID combination. A unique
VID/PID  combination  will  prevent  the  driver  from  conflicting  with  any  other  USB  driver.  A  vendor  ID  can  be  obtained  from http://
www.usb.org/ or Silicon Labs can provide a free PID for the OEM product that can be used with the Silicon Laboratories VID (http://
www.silabs.com/products/mcu/Pages/request-PID.aspx).
If the OEM application supports multiple CP2102N-based devices attached to the same PC, each CP2102N must have a unique serial
number. By default, the CP2102N uses a unique 128 bit identifier as the serial number. Alternatively, sequential serial numbers can be
pre-programmed by Silicon Labs using settings provided by Xpress Configurator and delivered as a custom CP2102N part number.
These serial numbers can be unique per custom part number, or multiple part numbers can share the same group of sequential serial
numbers. For more details, see Xpress Configurator in Simplicity Studio.
The internal programmable ROM is programmed via the USB. This allows the OEM's USB configuration data and serial number to be
written to the CP2102N on-board ROM during the manufacturing and testing process. A simple GUI-based or command-line customiza-
tion utility for programming the internal programmable ROM is available from Silicon Labs as a part of Simplicity Studio or available
separately on the Silicon Labs website (www.silabs.com/interface-software).
The device parameters can be locked to prevent future modification on the CP2102N.
CP2102N Data Sheet
## Functional Description
silabs.com | Building a more connected world.Rev. 1.5  |  26

## 5.  Pin Definitions
5.1  CP2102N QFN28 Pin Definitions
28 pin QFN
(Top View)
## 28272625
## 1
## 2
## 3
## 4
## 89
## 1011
## 21
## 20
## 19
## 18
## DCD
## RI / CLK
## GND
## D+
## VBUS
RSTb
## NC
SUSPENDb
## GPIO.5
## GPIO.6
## GPIO.0 / TXT
## GPIO.1 / RXT
## DTRDSRTXDRXD
## GND
## 24
## 2322
## CTS
## GPIO.4
## 121314
## SUSPEND
## CHREN
## CHR1
## 5
## 6
## 7
## 17
## 16
## 15
## D-
## VDD
## VREGIN
## GPIO.2 / RS485
## GPIO.3 / WAKEUP
## CHR0
## RTS
28 pin QFN
(Top View)
Figure 5.1.   CP2102N QFN28 Pinout
Table 5.1.  Pin Definitions for CP2102N QFN28
## Pin
## Number
Pin NameDescription
1DCDDigital Input. Data Carrier Detect control input (active low).
## 2RI /
## CLK
Digital Input. Ring Indicator control input (active low).
Digital Output. Clock output.
3GNDGround
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  27

## Pin
## Number
Pin NameDescription
4D+USB Data Positive
5D-USB Data Negative
6VDDSupply Power Input /
5V Regulator Output
7VREGIN5V Regulator Input
8VBUSDigital Input. VBUS Sense Input. This pin should be connected to the VBUS signal of a USB
network. A 5 V signal on this pin indicates a USB network connection.
9RSTbActive-low Reset
10NCNo Connect (leave this pin floating).
11SUSPENDbDigital Output. This pin is driven low when the device enters the USB suspend state.
12SUSPENDDigital Output. This pin is driven high when the device enters the USB suspend state.
13CHRENDigital Output. Enable charging circuit (100 mA).
14CHR1Digital Output. Enable highest current (1.5 A).
15CHR0Digital Output. Enable higher current (500 mA).
## 16GPIO.3 /
## WAKEUP
Digital Input/Output. General Purpose I/O.
Digital Input. Remote USB wakeup interrupt input.
## 17GPIO.2 /
## RS485
Digital Input/Output. General Purpose I/O.
Digital Output. RS485 control signal.
## 18GPIO.1 /
## RXT
Digital Input/Output. General Purpose I/O.
Digital Output. Receive LED toggle.
## 19GPIO.0 /
## TXT
Digital Input/Output. General Purpose I/O.
Digital Output. Transmit LED toggle.
20GPIO.6Digital Input/Output. General Purpose I/O.
21GPIO.5Digital Input/Output. General Purpose I/O.
22GPIO.4Digital Input/Output. General Purpose I/O.
23CTSDigital Input. Clear To Send control input (active low).
24RTSDigital Output. Ready To Send control output (active low).
25RXDDigital Input. Asynchronous data input (UART Receive).
26TXDDigital Output. Asynchronous data output (UART Transmit).
27DSRDigital Input. Data Set Ready control input (active low).
28DTRDigital Output. Data Terminal Ready control output (active low).
CenterGNDGround
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  28

5.2  CP2102N QFN24 Pin Definitions
## 242322212019
## 1
## 2
## 3
## 4
## 5
## 6
## 789
## 101112
## 18
## 17
## 16
## 15
## 14
## 13
24 pin QFN
(Top View)
## RI / CLK
## GND
## D+
## D-
## VIO
## VDD
## VREGIN
## VBUS
RSTb
## NC
## GPIO.3 / WAKEUP
## GPIO.2 / RS485
## CTS
## SUSPEND
## NC
SUSPENDb
## GPIO.1 / RXT
## DCDDTRDSRTXDRXDRTS
## GND
## GPIO.0 / TXT
Figure 5.2.   CP2102N QFN24 Pinout
Table 5.2.  Pin Definitions for CP2102N QFN24
## Pin
## Number
Pin NameDescription
## 1RI /
## CLK
Digital Input. Ring Indicator control input (active low).
Digital Output. Clock output.
2GNDGround
3D+USB Data Positive
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  29

## Pin
## Number
Pin NameDescription
4D-USB Data Negative
5VIOI/O Supply Power Input
6VDDSupply Power Input /
5V Regulator Output
7VREGIN5V Regulator Input
8VBUSDigital Input. VBUS Sense Input. This pin should be connected to the VBUS signal of a USB
network. A 5 V signal on this pin indicates a USB network connection.
9RSTbActive-low Reset
10NCNo Connect (leave this pin floating).
## 11GPIO.3 /
## WAKEUP
Digital Input/Output. General Purpose I/O.
Digital Input. Remote USB wakeup interrupt input.
## 12GPIO.2 /
## RS485
Digital Input/Output. General Purpose I/O.
Digital Output. RS485 control signal.
## 13GPIO.1 /
## RXT
Digital Input/Output. General Purpose I/O.
Digital Output. Receive LED toggle.
## 14GPIO.0 /
## TXT
Digital Input/Output. General Purpose I/O.
Digital Output. Transmit LED toggle.
15SUSPENDbDigital Output. This pin is driven low when the device enters the USB suspend state.
16NCNo Connect (leave this pin floating).
17SUSPENDDigital Output. This pin is driven high when the device enters the USB suspend state.
18CTSDigital Input. Clear To Send control input (active low).
19RTSDigital Output. Ready To Send control output (active low).
20RXDDigital Input. Asynchronous data input (UART Receive).
21TXDDigital Output. Asynchronous data output (UART Transmit).
22DSRDigital Input. Data Set Ready control input (active low).
23DTRDigital Output. Data Terminal Ready control output (active low).
24DCDDigital Input. Data Carrier Detect control input (active low).
CenterGNDGround
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  30

5.3  CP2102N QFN20 Pin Definitions
20 pin QFN
(Top View)
## 20191817
## 2
## 3
## 4
## 5
## 789
## 10
## 15
## 14
## 13
## 12
## GPIO.1 / RS485
## GPIO.0 / CLK
## GND
## D+
## D-
## VDD
## VREGIN
## VBUS
RSTb
## NC
## RTS
## CTS
## SUSPEND
## WAKEUP
## GND
SUSPENDb
## GPIO.2 / TXTGPIO.3 / RXTTXDRXD
## GND
## 1
## 611
## 16
Figure 5.3.   CP2102N QFN20 Pinout
Table 5.3.  Pin Definitions for CP2102N QFN20
## Pin
## Number
Pin NameDescription
## 1GPIO.1 /
## RS485
Digital Input/Output. General Purpose I/O.
Digital Output. RS485 control signal.
## 2GPIO.0 /
## CLK
Digital Input/Output. General Purpose I/O.
Digital Output. Clock output.
3GNDGround
4D+USB Data Positive
5D-USB Data Negative
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  31

## Pin
## Number
Pin NameDescription
6VDDSupply Power Input /
5V Regulator Output
7VREGIN5V Regulator Input
8VBUSDigital Input. VBUS Sense Input. This pin should be connected to the VBUS signal of a USB
network. A 5 V signal on this pin indicates a USB network connection.
9RSTbActive-low Reset
10NCNo Connect (leave this pin floating).
11SUSPENDbDigital Output. This pin is driven low when the device enters the USB suspend state.
12GNDGround
13WAKEUPDigital Input. Remote USB wakeup interrupt input.
14SUSPENDDigital Output. This pin is driven high when the device enters the USB suspend state.
15CTSDigital Input. Clear To Send control input (active low).
16RTSDigital Output. Ready To Send control output (active low).
17RXDDigital Input. Asynchronous data input (UART Receive).
18TXDDigital Output. Asynchronous data output (UART Transmit).
## 19GPIO.3 /
## RXT
Digital Input/Output. General Purpose I/O.
Digital Output. Receive LED toggle.
## 20GPIO.2 /
## TXT
Digital Input/Output. General Purpose I/O.
Digital Output. Transmit LED toggle.
CenterGNDGround
CP2102N Data Sheet
## Pin Definitions
silabs.com | Building a more connected world.Rev. 1.5  |  32

-  QFN28 Package Specifications
6.1   QFN28 Package Dimensions
Figure 6.1.   QFN28 Package Drawing
Table 6.1.   QFN28 Package Dimensions
DimensionMinTypMax
## A0.700.750.80
## A10.00—0.05
## A30.20 REF
b0.200.250.30
## D5.00 BSC
## D23.153.253.35
e0.50 BSC
## E5.00 BSC
## E23.153.253.35
## L0.450.550.65
aaa0.10
bbb0.10
ddd0.05
CP2102N Data Sheet
QFN28 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  33

DimensionMinTypMax
eee0.08
## Z0.44
## Y0.18
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.Dimensioning and Tolerancing per ANSI Y14.5M-1994.
3.This drawing conforms to JEDEC outline MO-220 except for custom features D2, E2, L, Z, and Y which are toleranced per suppli-
er designation.
4.Recommended card reflow profile is per the JEDEC/IPC J-STD-020 specification for Small Body Components.

CP2102N Data Sheet
QFN28 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  34

6.2   QFN28 PCB Land Pattern
## X1
## X2
## Y2
## Y1
## C2
## C1
## E
## C0.35
Figure 6.2.   QFN28 PCB Land Pattern Drawing
Table 6.2.   QFN28 PCB Land Pattern Dimensions
DimensionMinMax
## C14.80
## C24.80
## E0.50
## X10.30
## X23.35
## Y10.95
## Y23.35
CP2102N Data Sheet
QFN28 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  35

DimensionMinMax
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.This Land Pattern Design is based on the IPC-7351 guidelines.
3.All metal pads are to be non-solder mask defined (NSMD). Clearance between the solder mask and the metal pad is to be 60 μm
minimum, all the way around the pad.
4.A stainless steel, laser-cut and electro-polished stencil with trapezoidal walls should be used to assure good solder paste release.
5.The stencil thickness should be 0.125 mm (5 mils).
6.The ratio of stencil aperture to land pad size should be 1:1 for all perimeter pads.
7.A 2 x 2 array of 1.2 mm square openings on a 1.5 mm pitch should be used for the center pad.
8.A No-Clean, Type-3 solder paste is recommended.
9.The recommended card reflow profile is per the JEDEC/IPC J-STD-020 specification for Small Body Components.

6.3  QFN28 Package Marking
## PPPPPPPP
## TTTTTT
## YYWW #
Figure 6.3.  QFN28 Package Marking
The package marking consists of:
- PPPPPPPP – The part number designation.
- TTTTTT – A trace or manufacturing code.
- YY – The last two digits of the assembly year.
- WW – The two-digit workweek when the device was assembled.
- # – Indicates the hardware revision.
Note: Firmware revision is not part of the package marking.

CP2102N Data Sheet
QFN28 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  36

-  QFN24 Package Specifications
7.1  QFN24 Package Dimensions
Figure 7.1.   QFN24 Package Drawing
Table 7.1.   QFN24 Package Dimensions
DimensionMinTypMax
## A0.700.750.80
## A10.00—0.05
b0.180.250.30
## D4.00 BSC
## D22.352.452.55
e0.50 BSC
## E4.00 BSC
## E22.352.452.55
## L0.300.400.50
aaa——0.10
bbb——0.10
ccc——0.08
ddd——0.10
CP2102N Data Sheet
QFN24 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  37

DimensionMinTypMax
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.Dimensioning and Tolerancing per ANSI Y14.5M-1994.
3.This drawing conforms to JEDEC Solid State Outline MO-220.
4.Recommended card reflow profile is per the JEDEC/IPC J-STD-020C specification for Small Body Components.

CP2102N Data Sheet
QFN24 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  38

7.2  PCB Land Pattern
## C0.25
## C1
## E
## X1
## X2
## Y2
## Y1
## C2
Figure 7.2.  PCB Land Pattern Drawing
Table 7.2.  PCB Land Pattern Dimensions
DimensionMinMax
## C13.90
## C23.90
## E0.50
## X10.30
## X22.55
## Y10.85
## Y22.55
CP2102N Data Sheet
QFN24 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  39

DimensionMinMax
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.This Land Pattern Design is based on the IPC-7351 guidelines.
3.All metal pads are to be non-solder mask defined (NSMD). Clearance between the solder mask and the metal pad is to be 60 μm
minimum, all the way around the pad.
4.A stainless steel, laser-cut and electro-polished stencil with trapezoidal walls should be used to assure good solder paste release.
5.The stencil thickness should be 0.125 mm (5 mils).
6.The ratio of stencil aperture to land pad size should be 1:1 for all perimeter pads.
7.A 2 x 2 array of 0.9 mm square openings on a 1.2 mm pitch should be used for the center pad.
8.A No-Clean, Type-3 solder paste is recommended.
9.The recommended card reflow profile is per the JEDEC/IPC J-STD-020 specification for Small Body Components.

## 7.3  Package Marking
## PPPPPPPP
## TTTTTT
## YYWW  #
## Figure 7.3.  Package Marking
The package marking consists of:
- PPPPPPPP – The part number designation.
- TTTTTT – A trace or manufacturing code.
- YY – The last two digits of the assembly year.
- WW – The two-digit workweek when the device was assembled.
- # – Indicates the hardware revision.
Note: Firmware revision is not part of the package marking.

CP2102N Data Sheet
QFN24 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  40

-  QFN20 Package Specifications
8.1   QFN20 Package Dimensions

Figure 8.1.   QFN20 Package Drawing
Table 8.1.   QFN20 Package Dimensions
DimensionMinTypMax
## A0.700.750.80
## A10.000.020.05
## A30.20 REF
b0.180.250.30
c0.250.300.35
## D3.00 BSC
## D21.61.701.80
e0.50 BSC
## E3.00 BSC
CP2102N Data Sheet
QFN20 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  41

DimensionMinTypMax
## E21.601.701.80
f2.50 BSC
## L0.300.400.50
## K0.25 REF
## R0.090.1250.15
aaa0.15
bbb0.10
ccc0.10
ddd0.05
eee0.08
fff0.10
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.Dimensioning and Tolerancing per ANSI Y14.5M-1994.
3.The drawing complies with JEDEC MO-220.
4.Recommended card reflow profile is per the JEDEC/IPC J-STD-020 specification for Small Body Components.

CP2102N Data Sheet
QFN20 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  42

8.2   QFN20 PCB Land Pattern
Figure 8.2.   QFN20 PCB Land Pattern Drawing
Table 8.2.   QFN20 PCB Land Pattern Dimensions
DimensionMinMax
## C13.10
## C23.10
## C32.50
## C42.50
## E0.50
## X10.30
## X20.250.35
## X31.80
## Y10.90
## Y20.250.35
## Y31.80
CP2102N Data Sheet
QFN20 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  43

DimensionMinMax
## Note:
1.All dimensions shown are in millimeters (mm) unless otherwise noted.
2.Dimensioning and Tolerancing is per the ANSI Y14.5M-1994 specification.
3.This Land Pattern Design is based on the IPC-7351 guidelines.
4.All metal pads are to be non-solder mask defined (NSMD). Clearance between the solder mask and the metal pad is to be 60 μm
minimum, all the way around the pad.
5.A stainless steel, laser-cut and electro-polished stencil with trapezoidal walls should be used to assure good solder paste release.
6.The stencil thickness should be 0.125 mm (5 mils).
7.The ratio of stencil aperture to land pad size should be 1:1 for the perimeter pads.
8.A 2 x 2 array of 0.75 mm openings on a 0.95 mm pitch should be used for the center pad to assure proper paste volume.
9.A No-Clean, Type-3 solder paste is recommended.
10.The recommended card reflow profile is per the JEDEC/IPC J-STD-020 specification for Small Body Components.

8.3  QFN20 Package Marking
## PPPP
## PPPPPP
## TTTTTT
## YYWW  #
Figure 8.3.  QFN20 Package Marking
The package marking consists of:
- PPPPPPPP – The part number designation.
- TTTTTT – A trace or manufacturing code.
- Y – The last digit of the assembly year.
- WW – The two-digit workweek when the device was assembled.
- # – Indicates the hardware revision.
Note: Firmware revision is not part of the package marking.

CP2102N Data Sheet
QFN20 Package Specifications
silabs.com | Building a more connected world.Rev. 1.5  |  44

## 9.  Relevant Application Notes
The following Application Notes are applicable to the CP2102N devices:
•AN721: CP210x Device Customization Guide  —  This  application  note  guides  developers  through  the  configuration  process  of
USBXpress devices using Simplicity Studio [Xpress Configurator].
•AN220: USB Driver Customization — This document and accompanying software enable the customization of the CP210x Virtual
COM Port (VCP) and USBXpress drivers.
•AN197: Serial Communications Guide for CP210x — This document describes recommendations for communicating with USBX-
press CP210x devices using the Virtual COM Port (VCP) driver.
•AN976: Migrating from a CP2102 to a CP2102N — This document guides developers on how to migrate existing systems using the
CP2102 to the CP2102N.
•AN169: USBXpress Programmer’s Guide — This application note provides recommendations and examples for developing using
the USBXpress direct-access driver.
•AN807: Recertifying a Customized Windows HCK Driver Package — This document describes the WHQL certification process re-
quired for customized drivers.
•AN223: Runtime GPIO Control for CP210x — This document describes how to toggle GPIO pins from the USB host.
Application Notes can be accessed on the Silicon Labs website (www.silabs.com/interface-appnotes) or in Simplicity Studio using the
[Application Notes] tile.
CP2102N Data Sheet
## Relevant Application Notes
silabs.com | Building a more connected world.Rev. 1.5  |  45

## 10.  Revision History
## Revision 1.5
## November, 2020
- Updated Figure 2.6 Self-Powered Connection Diagram for USB Pins on page 9 to add back VBUS voltage divider that was acciden-
tally removed in previous revision.
## Revision 1.4
## October, 2020
- Updated Figure 2.4 Battery Charging Connection Diagram on page 7, Figure 2.5 Bus-Powered Connection Diagram for USB Pins on
page 8, and Figure 2.6 Self-Powered Connection Diagram for USB Pins on page 9 to reflect new SP0503BAHTG protection device
RoHS-compliant part number.
- Added firmware note to the package marking sections.
## Revision 1.3
## March, 2019
- Updated Table 1.1 Product Selection Guide on page 2 to reflect revison change to CP2102N-A02 devices.
- Added section 4.2.2 Sending Break Signaling describing how to send line breaks
- Added section 4.3.13 Receiver Timeout describing the new custom vendor command to configure the internal buffer receive timeout
- Changed note in 4.3.12 RS485 (RS485) to indicate that in CP2102N-A02, the RS485 pin is not available at baud rates below 300
baud.
- Changed information in 4.3.8 Software Handshaking to indicate that in CP2102N-A02, watermark levels greater than 512 are con-
verted to an XON limit of 192 and an XOFF limit of 64 bytes.
- Updated PCB land pattern diagram in 8.2  QFN20 PCB Land Pattern.
## Revision 1.2
## November, 2017
- Added a note to Table 1.1 Product Selection Guide on page 2 to clarify the multiple types of pin 1 indicators for each package.
- Added a description of the RSTb pin behavior during reset to 2.1 Power and as a note on 3.1.3 Reset and Supply Monitor.
- Updated the note regarding the resistor divider on VBUS, added the resistor divider to Figure 2.5 Bus-Powered Connection Diagram
for USB Pins on page 8, and duplicated the note to this area in 2.3 USB.
- Added the resistor divider on VBUS to Figure 2.4 Battery Charging Connection Diagram on page 7.
- Updated Thermal Resistance to Thermal Resistance (Junction to Ambient) and added Thermal Resistance (Junction to Case) and
Thermal Characterization Parameter (Junction to Top) for all packages to 3.2 Thermal Conditions.
- Updated 4.3.4 Battery Charging (CHREN, CHR0, and CHR1) to include information about configuring GPIO for battery charging.
- Updated 4.3.7  Hardware  Handshaking  (RTS  and  CTS)  and 4.3.12  RS485  (RS485)  with  a  note  that  RTS  control  signaling  and
RS485 features are not supported below 1200 baud.
- Added Z and Y dimensions and updated Note 3 in Table 6.1  QFN28 Package Dimensions on page 33.
- Updated revision history format and moved table of contents.
## Revision 1.1
## August, 2016
- Updated the minimum Operating Supply Voltage on VDD to 3.0 V in 1. Feature List and Ordering Information, 3.1.1 Recommended
Operating Conditions, 3.1.4 Configuration Memory, and Figure 2.3 Connection Diagram with Voltage Regulator Not Used on page 6.
- Updated 4.3.6 Clock Output (CLK) to specify that the clock is not present when the device is in USB Suspend.
- Updated QFN24 bottom pad label to GND instead of VSS.
- Adjusted D, E, and aaa in QFN28 Package Dimensions.
- Adjusted D, E, and L in QFN24 Package Dimensions.
CP2102N Data Sheet
## Revision History
silabs.com | Building a more connected world.Rev. 1.5  |  46

## Revision 1.0
## May, 2016
- Initial release.
CP2102N Data Sheet
## Revision History
silabs.com | Building a more connected world.Rev. 1.5  |  47

IoT Portfolio
www.silabs.com/IoT
## SW/HW
w
ww.silabs.com/simplicity
## Quality
w
ww.silabs.com/quality
## Support & Community
www.sil
abs.com/community
## Simplicity Studio
One-click access to MCU and wireless
tools, documentation, software, source
code libraries & more. Available for
Windows, Mac and Linux!
## Silicon Laboratories Inc.
## 400 West Cesar Chavez
## Austin, T X 78701
## USA
http://www.silabs.com
## Disclaimer
Silicon Labs intends to provide customers with the latest, accurate, and in-depth documentation of all peripherals and modules available for system and software implementers using or
intending to use the Silicon Labs products. Characterization data, available modules and peripherals, memory sizes and memory addresses refer to each specific device, and “Typical”
parameters provided can and do vary in different applications. Application examples described herein are for illustrative purposes only. Silicon Labs reserves the right to make changes
without further notice to the product information, specifications, and descriptions herein, and does not give warranties as to the accuracy or completeness of the included information.
Without prior notification, Silicon Labs may update product firmware during the manufacturing process for security or reliability reasons.  Such changes will not alter the specifications or
the performance of the product.  Silicon Labs shall have no liability for the consequences of use of the information supplied in this document. This document does not imply or expressly
grant any license to design or fabricate any integrated circuits. The products are not designed or authorized to be used within any FDA Class III devices, applications for which FDA
premarket approval is required, or Life Support Systems without the specific written consent of Silicon Labs. A “Life Support System” is any product or system intended to support or
sustain life and/or health, which, if it fails, can be reasonably expected to result in significant personal injury or death. Silicon Labs products are not designed or authorized for military
applications. Silicon Labs products shall under no circumstances be used in weapons of mass destruction including (but not limited to) nuclear, biological or chemical weapons, or
missiles capable of delivering such weapons. Silicon Labs disclaims all express and implied warranties and shall not be responsible or liable for any injuries or damages related to use of
a Silicon Labs product in such unauthorized applications.
## Trademark Information
Silicon Laboratories Inc.®, Silicon Laboratories®, Silicon Labs®, SiLabs® and the Silicon Labs logo®, Bluegiga®, Bluegiga Logo®, ClockBuilder®, CMEMS®, DSPLL®, EFM®,
EFM32®, EFR, Ember®, Energy Micro, Energy Micro logo and combinations thereof, “the world’s most energy friendly microcontrollers”, Ember®, EZLink®, EZRadio®, EZRadioPRO®,
Gecko®, Gecko OS, Gecko OS Studio, ISOmodem®, Precision32®, ProSLIC®, Simplicity Studio®, SiPHY®, Telegesis, the Telegesis Logo®, USBXpress®, Zentri, the Zentri logo and
Zentri DMS,  Z-Wave®, and others are trademarks or registered trademarks of Silicon Labs.  ARM, CORTEX, Cortex-M3 and THUMB are trademarks or registered trademarks of ARM
Holdings. Keil is a registered trademark of ARM Limited. Wi-Fi is a registered trademark of the Wi-Fi Alliance.  All other products or brand names mentioned herein are trademarks of
their respective holders.