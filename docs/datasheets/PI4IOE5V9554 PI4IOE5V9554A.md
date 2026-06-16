







## 8-bit I
## 2
C-Bus and SMBus I/O Port with Interrupt

## PI4IOE5V9554/9554A

## Features
 Operation Power Supply Voltage from 2.3V to 5.5 V
##  8-bit I
## 2
C-bus GPIO with Interrupt and Reset
 5V Tolerant I/Os
##  Polarity Inversion Register
 Active LOW Interrupt Output
##  Low Current Consumption
 0Hz to 400KHz Clock Frequency
 Noise Filter on SCL/SDA Inputs
##  Power-on Reset
 ESD Protection (4KV HBM and 1KV CDM)
 Totally Lead-Free & Fully RoHS Compliant (Notes 1 & 2)
 Halogen and Antimony Free. “Green” Device (Note 3)
 For automotive applications requiring specific change
control (i.e. parts qualified to AEC-Q100/101/200, PPAP
capable, and manufactured in IATF 16949 certified
facilities), please contact us
or your local Diodes
representative.
https://www.diodes.com/quality/product-definitions/
 Offered in Three Different Packages:
 TSSOP-16 and TQFN 3x3-16





## Description
The PI4IOE5V9554  and  PI4IOE5V9554A  provide  8  bits  of
general  purpose  parallel input/output (GPIO)  expansion  for
## I
## 2
C-bus/SMBus   applications.   The   device includes features
such  as  higher  driving  capability,  5V  tolerance,  lower  power
supply, individual I/O configuration, and smaller packaging. It
provides a simple solution when additional I/O is required for
ACPI power switches, sensors, push buttons, LEDs, fans, etc.

The PI4IOE5V9554/PI4IOE5V9554A consists   of   an   8-bit
register  to  configure  the  I/Os  as  either  inputs  or  outputs  and
an  8-bit  polarity  register  to  change  the  polarity  of  the  input
port register data. The data for each input or output is kept in
the   corresponding   input   port  or output   port register.   All
registers can be read by the system master.

The PI4IOE5V9554/PI4IOE5V9554A open-drain   interrupt
output is activated and indicates to the system when any input
state  has  changed.  The  power-on  reset  sets  the  registers  to
their default values and initializes the device state machine.

Three  hardware  pins  (A0,  A1,  A2)  vary  the  fixed  I
## 2
## C-bus
address  and  allow  up  to  eight  devices  to  share  the  same  I
## 2
## C-
bus/SMBus.    The    PI4IOE5V9554A    is    identical    to    the
PI4IOE5V9554 except  the  fixed  I
## 2
C-bus  address  is  different,
allowing  up  to  sixteen  of  these  devices  (eight  of  each)  on  the
same I
## 2
C-bus/SMBus.
## Pin Configuration

Figure 1: TSSOP-16 ( Top View )


Figure 2: TQFN 3x3-16 ( Top View )

## TQFN
## Notes:
- No purposely added lead. Fully EU Directive 2002/95/EC (RoHS), 2011/65/EU (RoHS 2) & 2015/863/EU (RoHS 3) compliant.
- See https://www.diodes.com/quality/lead-free/ for more information about Diodes Incorporated’s definitions of Halogen- and Antimony-free, "Green" and Lead-free.
- Halogen- and Antimony-free "Green” products are defined as those which contain <900ppm bromine, <900ppm chlorine (<1500ppm total Br + Cl) and <1000ppm
antimony compounds.

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 1

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Pin Description
## Table 1: Pin Description
## Pin
## Name Type Description
## TSSOP16 TQFN16
## 1 15 A0

## I Address Input 0
## 2 16 A1 I Address Input 1
## 3 1 A2 I Address Input 2
4 2 IO0 I/O Input/Output 0
5 3 IO1 I/O Input/Output 1
6 4 IO2 I/O Input/Output 2
7 5 IO3 I/O Input/Output 3
8 6 GND G Supply Ground
9 7 IO4 I/O Input/Output 4
10 8 IO5 I/O Input/Output 5
11 9 IO6 I/O Input/Output 6
12 10 IO7 I/O Input/Output 7
## 13 11
## INT

O Interrupt Output (Open Drain)
14 12 SCL I Serial Clock Line
15 13 SDA I Serial Data Line
16 14 VCC P Supply Voltage
## * I = Input; O = Output; P = Power; G = Ground

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 2

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Maximum Ratings

Power Supply ............................................................................... -0.5V to +6.0V
Voltage on I/O pin .............................................................. GND-0.5V to +6.0V
Input Current ............................................................................................. ±20mA
Output Current on an I/O pin  .................................................................. ±50mA
Supply Current ........................................................................................ ±160mA
Ground Supply Current ............................................................................ 200mA
Total Power Dissipation .......................................................................... 400mW
Operation Temperature .................................................................. -40°C   ~ 85°C
Storage Temperature  ................................................................... -65°C   ~ 150°C
Maximum Junction Temperature ,Tj(max)  ............................................. 125°C

## Static Characteristics
VCC = 2.3V to 5.5V; GND = 0 V; Tamb= -40°C to +85°C; unless otherwise specified.
## Table 2: Static Characteristics
## Symbol Parameter Conditions Min. Typ. Max. Unit
## Power Supply
## V
## CC
## Supply Voltage — 2.3 — 5.5 V
## I
## CC
## Supply Current
Operating mode
## VCC = 5.5V
No load
fSCL = 400kHz
— 40 60 μA
Operating mode
## VCC = 2.3V
No load
fSCL = 400kHz
— 10 20 μA
## I
stb
## Standby Current
Standby mode
## VCC = 5.5V
No load
## VI = GND
fSCL = 0kHz
I/O = inputs
— 500 700 μA
Standby mode
## VCC = 5.5V
No load
## VI = VCC
fSCL = 0kHz
I/O = inputs
— 0.25 1 μA
## V
## POR

## Power-on Reset Voltage
## [1]

## — — 1.16 1.41 V
Input SCL, Input/Output SDA
## V
## IL
Low Level Input Voltage — -0.5 — +0.3VCC V
## V
## IH
High Level Input Voltage — 0.7VCC — 5.5 V
## I
## OL
## Low Level Output Current V
## OL
=0.4V; VCC=2.3V 3 6 — mA
## I
## L
## Leakage Current V
## I
= VCC or GND -1 — 1 μA
## C
i
## Input Capacitance V
## I
= GND — 6 10 pF

## Note:
Stresses greater than those listed under MAXIMUM
RATINGS  may  cause  permanent  damage  to  the
device.  This  is  a  stress  rating  only  and  functional
operation   of   the   device   at   these   or   any   other
conditions  above  those  indicated  in  the  operational
sections   of   this   specification   is   not   implied.
Exposure  to  absolute  maximum  rating  conditions
for extended periods may affect reliability.

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 3

## © Diodes Incorporated









## PI4IOE5V9554/9554A


## Symbol Parameter Conditions Min. Typ. Max. Unit
I/Os
VIL  Low Level Input Voltage — -0.5
## —
## +0.81 V
VIH High Level Input Voltage — +1.8
## —
## 5.5 V
## I
## OL
## Low Level Output Current
## VCC = 2.3V; V
## OL
## = 0.5V
## [2]
## 8 10
## —
mA
## VCC = 2.3V; V
## OL
## = 0.7V
## [2]
## 10 13
## —
mA
## VCC = 3.0V; V
## OL
## = 0.5V
## [2]
## 8 14
## —
mA
## VCC = 3.0V; V
## OL
## = 0.7V
## [2]
## 10 19
## —
mA
## VCC = 4.5V; V
## OL
## = 0.5V
## [2]
## 8 17
## —
mA
## VCC = 4.5V; V
## OL
## = 0.7V
## 2]
## 10 24
## —
mA
## V
## OH
## High Level Output Current
## I
## OH
= -8mA; VCC = 2.3V
## [3]
## 1.8
## — —
## V
## I
## OH
= -10mA; VCC = 2.3V
## [3]
## 1.7
## — —
## V
## I
## OH
=-8mA; VCC = 3.0V
## [3]
## 2.6
## — —
## V
## I
## OH
= -10mA; VCC = 3.0V
## [3]
## 2.5
## — —
## V
## I
## OH
= -8mA; VCC = 4.5V
## [3]
## 4.1
## — —
## V
## I
## OH
= -10mA; VCC = 4.5V
## [3]
## 4.0
## — —
## V
## I
## LI

## Low Level Input Leakage
## Current
## VCC = 3.6V; V
## I
= VCC -1 — 1 μA
## I
## L
Leakage Current VCC = 5.5V; V
## I
## = GND
## —
— -100 μA
## C
i
## Input Capacitance —
## —
3.7 10 pF
## C
o
## Output Capacitance —
## —
3.7 10 pF
## Interrupt
## INT

## I
## OL
## Low Level Output Current V
## OL
= 0.4V 3 — — mA
## Select Inputs A0, A1, A2
## V
## IL
## Low Level Input Voltage — -0.5
## —
## +0.81 V
## V
## IH
## High Level Input Voltage — +1.8
## —
## 5.5 V
## I
## L
Input Leakage Current — -1 — 1 μA
## Note:
- VCC must be lowered to 0.2V for at least 20μs in order to reset part.
- Each I/O must be limited to a maximum current of 25mA and the device must be limited to a maximum current of 100mA.
- The total current sourced by all I/Os must be limited to 85mA.


PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 4

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Dynamic Characteristics
## Table 3: Dynamic Characteristics
## Symbol Parameter Test Conditions
## Standard
## Mode I
## 2
## C
## Fast Mode I
## 2
## C
## Unit
## Min Max Min Max
f
## SCL
SCL Clock Frequency — 0 100 0 400 kHz
t
## BUF

Bus Free Time Between a STOP and
START Condition
— 4.7 — 1.3 — μs
t
## HD;STA

Hold Time (Repeated) START
## Condition
— 4.0 — 0.6 — μs
t
## SU;STA

Setup Time for a Repeated START
## Condition
— 4.7 — 0.6 — μs
t
## SU;STO
Setup Time for STOP Condition — 4.0 — 0.6 — μs
t
## VD;ACK
## [1]
Data Valid Acknowledge Time — — 3.45 — 0.9 μs
t
## HD;DAT
## [2]
Data Hold Time — 0 — 0 — ns
t
## VD;DAT
Data Valid Time — — 3.45 — 0.9 μs
t
## SU;DAT
Data Setup Time — 250 — 100 — ns
t
## LOW
LOW Period of the SCL Clock — 4.7 — 1.3 — μs
t
## HIGH
HIGH Period of the SCL Clock — 4.0 — 0.6 — μs
t
f

Fall Time of Both SDA and SCL
## Signals
— — 300 — 300 ns
t
r

Rise Time of Both SDA and SCL
## Signals
— — 1000 — 300 ns
t
## SP

Pulse Width of Spikes that must be
Suppressed by the Input Filter
— — 50 — 50 ns
## Port Timing
t
v(Q)
## Data Output Valid Time
## [3]
— — 200 — 200 ns
t
su(D)
Data Input Setup Time — 100 — 100 — ns
t
h(D)
Data Input Hold Time — 1 — 1 — μs
## Interrupt Timing
t
v(INT)

Valid Time on pin
## INT

— — 4 — 4 μs
t
rec(INT)

Reset Time on pin
## INT

— — 4 — 4 μs
## Note:
- t
## VD;ACK
= time for acknowledgement signal from SCL LOW to SDA (out) LOW.
- t
## VD;DAT
= minimum time for SDA data out to be valid following SCL LOW.
- t
v(Q)
measured from 0.7VCC on SCL to 50% I/O output.



PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 5

## © Diodes Incorporated









## PI4IOE5V9554/9554A

Figure 3: Timing Parameters for INT Signal

PI4IOE5V9554/PI4IOE5V9554A Block Diagram






















## Figure 4: Block Diagram

All I/Os are set to inputs at reset.

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 6

## © Diodes Incorporated









## PI4IOE5V9554/9554A

## Details Description
a.  Device Address
Following  a  START  condition,  the  bus  master  must  output  the  address  of  the  slave  it  is  accessing.  The  address  of  the
PI4IOE5V9554/54A is shown below. To conserve power, no internal pullup resistors are incorporated on the hardware selectable
address pins, and they must be pulled HIGH or LOW.

## Table 4: Device Address Byte
b7(MSB) b6 b5 b4 b3 b2 b1 b0
## PI4IOE5V9554 0 1 0 0 A2 A1 A0 R/W
## PI4IOE5V9554A 0 1 1 1 A2 A1 A0 R/W
## Note: Read “1”, Write “0”

b. Register Description
i. Command Byte
The  command  byte  is  the  first  byte  to  follow  the  address  byte  during  a  write  transmission.  It  is  used  as  a  pointer  to  determine
which of the following registers are   written or read.

## Table 5: Command Byte
## Command Protocol Function
0 Read Byte Input port register
1 Read/Write Byte Output port register
2 Read/Write Byte Polarity Inversion register
3 Read/Write Byte Configuration register

ii. Register 0: Input Port Register
This register is a read-only port. It reflects the incoming logic levels of the pins, regardless of whether the pin is defined as an input
or an output by Register 3. Writes to this register have no effect.

The default ‘X’ is determined by the externally applied logic level, which is normally ‘1’ when no external signal externally applied
because of the internal pullup resistors.

## Table 6: Input Port Register
## Bit 7 6 5 4 3 2 1 0
## Symbol I7 I6 I5 I4 I3 I2 I1 I0


PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 7

## © Diodes Incorporated









## PI4IOE5V9554/9554A

iii. Register 1: Output Port Register
This register reflects the outgoing logic levels of the pins defined as outputs by Register 3.Bit values in this register have no effect
on pins defined as inputs. Reads from this register return the value that is in the flip-flop controlling the output selection—not the
actual pin value.

## Table 7: Output Port Register
## Bit 7 6 5 4 3 2 1 0
## Symbol
## O7 O6 O5 O4 O3 O2 O1 O0
## Default
## 1 1 1 1 1 1 1 1

iv. Register 2: Polarity Inversion Register
This register allows the user to invert the polarity of the input port register data. If a bit in this register is set (written with ‘1’), the
corresponding input  port data  is  inverted.  If  a  bit  in  this  register  is  cleared  (written  with  a  ‘0’),  the  input  port data  polarity  is
retained.

## Table 8: Polarity Inversion Register
## Bit 7 6 5 4 3 2 1 0
## Symbol
## N7 N6 N5 N4 N3 N2 N1 N0
## Default
## 0 0 0 0 0 0 0 0

v. Register 3: Configuration Register
This  register  configures  the  directions  of  the  I/O  pins.  If  a  bit  in  this  register  is  set,  the  corresponding  port  pin  is  enabled  as  an
input with high-impedance output driver. If a bit in this register is cleared, the corresponding port pin is enabled as an output. At
reset, the I/Os are configured as inputs with a weak pullup to VCC.

## Table 9: Configuration Register
## Bit 7 6 5 4 3 2 1 0
## Symbol
## C7 C6 C5 C4 C3 C2 C1 C0
## Default
## 1 1 1 1 1 1 1 1


PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 8

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Power-on Reset
When power is applied to VCC, an internal power-on reset (POR) holds thePI4IOE5V9554/PI4IOE5V9554A in a reset condition
until VCC has reached VPOR. At that point, the reset condition is released and thePI4IOE5V9554/PI4IOE5V9554A registers and
state machine initialize to their default states. Thereafter, VCC must be lowered below 0.2 V to reset the device.

For a power reset cycle, VCC must be lowered below 0.2 V and then restored to the operating voltage.

c. Interrupt Output
The  open-drain  interrupt  output  is  activated  when  one  of  the  port  pins  change  state  and  the  pin  is  configured  as  an  input.  The
interrupt is deactivated when the input returns to its previous state or the input port register is read.

Note that changing an I/O from and output to an input may cause a false interrupt to occur if the state of the pin does not match
the contents of the input port register.

d. I/O Port
When an I/O is configured as an input, FETs Q1 and Q2 are off, creating a high-impedance input with a weak pullup (100 kΩ typ.)
to VCC. The input voltage may be raised above VCC
to a maximum of 5.5V.

If  the  I/O  is  configured  as  an  output,  then  either  Q1  or  Q2  is  enabled,  depending  on  the  state  of  the  output  port  register.  Care
should  be  exercised  if  an  external  voltage  is  applied  to  an  I/O  configured  as  an  output  because  of  the  low-impedance  paths  that
exist between the pin and either VCC
or GND.
































Figure 5: Simplified Schematic of IO0 to IO7

Note: At power-on reset, all registers return to default values.
100kΩ

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 9

## © Diodes Incorporated









## PI4IOE5V9554/9554A

e. Bus Transaction
Data is transmitted to the PI4IOE5V9554/PI4IOE5V9554A registers using the wri  te mode as shown in Figure 6 and Figure 7.

These  devices  do  not  implement  an  auto-increment  function,  so  once  a  command  byte  has  been  sent,  the  register  which  was
addressed continues to be accessed by reads until a new command byte is sent.



























Figure 6: Write to Output Register

Figure 7: Write to Configuration Register or Polarity Inversion Register


PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 10

## © Diodes Incorporated









## PI4IOE5V9554/9554A

Data is read from the PI4IOE5V9554/PI4IOE5V9554A registers using the read mode as shown in Figure 8 and Figure 9.






























Figure 8: Read from Register

## Figure 9: Read Input Port Register

This figure assumes the command byte has previously been programmed with 00h.
Transfer of data can be stopped at any moment by a STOP condition.

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 11

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Application Design-in Information



















## Figure 10: Typical Application

Device address configured as 0100100x for this example.
IO0, IO4, IO5 configured as outputs.
IO1, IO2, IO3 configured as inputs.
IO6, IO07 are not used and must be configured as outputs.

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 12

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## Part Marking
## PI4IOE5V9554
L Package        ZH Package


## PI4IOE5V9554A
## L Package




PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 13

## © Diodes Incorporated









## PI4IOE5V9554/9554A

## Packaging Mechanical
## SOIC-16(W)

PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 14

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## TSSOP-16(L)


PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 15

## © Diodes Incorporated









## PI4IOE5V9554/9554A
TQFN 3x3-16(ZH)

For latest package information:
See http://www.diodes.com/design/support/packaging/pericom-packaging/packaging-mechanicals-and-thermal-characteristics/.
## Ordering Information
## Part Numbers Package Code Package Description
PI4IOE5V9554LEX L 16-pin, 173 mil Wide (TSSOP)
PI4IOE5V9554ALEX L 16-pin, 173 mil Wide (TSSOP)
PI4IOE5V9554ZHEX ZH 16-contact, Thin Fine Pitch Quad Flat No-Lead (TQFN) 3.0×3.0
## Notes:
- 1. No purposely added lead. Fully EU Directive 2002/95/EC (RoHS), 2011/65/EU (RoHS 2) & 2015/863/EU (RoHS 3) compliant.
- 2. See https://www.diodes.com/quality/lead-free/ for more information about Diodes Incorporated’s definitions of Halogen- and Antimony-free, "Green" and
## Lead-free.
- 3. Halogen- and Antimony-free "Green” products are defined as those which contain <900ppm bromine, <900ppm chlorine (<1500ppm total Br + Cl) and
<1000ppm antimony compounds.
- E = Pb-free and Green
- X suffix = Tape/Reel




PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 16

## © Diodes Incorporated









## PI4IOE5V9554/9554A
## IMPORTANT NOTICE

DIODES INCORPORATED MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARDS TO THIS DOCUMENT, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE (AND THEIR EQUIVALENTS UNDER THE LAWS OF ANY
## JURISDICTION).

Diodes Incorporated and its subsidiaries reserve the right to make modifications, enhancements, improvements, corrections or other changes without further notice to this document and
any product described herein. Diodes Incorporated does not assume any liability arising out of the application or use of this document or any product described herein; neither does Diodes
Incorporated convey any license under its patent or trademark rights, nor the rights of others. Any Customer or user of this document or products described herein in such applications
shall assume all risks of such use and will agree to hold Diodes Incorporated and all the companies whose products are represented on Diodes Incorporated website, harmless against all
damages.

Diodes Incorporated does not warrant or accept any liability whatsoever in respect of any products purchased through unauthorized sales channel.

Should Customers purchase or use Diodes Incorporated products for any unintended or unauthorized application, Customers shall indemnify and hold Diodes Incorporated and its
representatives harmless against all claims, damages, expenses, and attorney fees arising out of, directly or indirectly, any claim of personal injury or death associated with such
unintended or unauthorized application.

Products described herein may be covered by one or more United States, international or foreign patents pending. Product names and markings noted herein may also be covered by one
or more United States, international or foreign trademarks.

This document is written in English but may be translated into multiple languages for reference. Only the English version of this document is the final and determinative format released
by Diodes Incorporated.
## LIFE SUPPORT

Diodes Incorporated products are specifically not authorized for use as critical components in life support devices or systems without the express written approval of the Chief Executive
Officer of Diodes Incorporated. As used herein:

A. Life support devices or systems are devices or systems which:
- are intended to implant into the body, or
- support or sustain life and whose failure to perform when properly used in accordance with instructions for use provided in the labeling can be reasonably expected to result in
significant injury to the user.

B. A critical component is any component in a life support device or system whose failure to perform can be reasonably expected to cause the
failure of the life support device or to affect its safety or effectiveness.

Customers represent that they have all necessary expertise in the safety and regulatory ramifications of their life support devices or systems, and acknowledge and agree that they are
solely responsible for all legal, regulatory and safety-related requirements concerning their products and any use of Diodes Incorporated products in such safety-critical, life support
devices or systems, notwithstanding any devices- or systems-related information or support that may be provided by Diodes Incorporated. Further, Customers must fully indemnify
Diodes Incorporated and its representatives against any damages arising out of the use of Diodes Incorporated products in such safety-critical, life support devices or systems.

## Copyright © 2019, Diodes Incorporated
www.diodes.com



PI4IOE5V9554/ PI4IOE5V9554A   www.diodes.com   November 2019
Document Number DS40769 Rev 3 - 2
## 17

## © Diodes Incorporated
