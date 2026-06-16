

## Product Specification
RoHS

## Compliant
## REACH

## Compliant
## Newhaven Display International, Inc.
2661 Galvin Court, Elgin, IL 60124  USA
## Ph: 847.844.8795  |  Fx: 847.844.8796
www.newhavendisplay.com
## NHD- 1.91-176176UBC3



## 1

Table of Contents
Document Revision History .......................................................................................................................... 2
Mechanical Drawing .................................................................................................................................... 3
Pin Description ............................................................................................................................................ 4
Interface Selection ....................................................................................................................................... 5
Wiring Diagram ........................................................................................................................................... 5
Electrical Characteristics .............................................................................................................................. 6
Optical Characteristics ................................................................................................................................. 6
Controller information ................................................................................................................................. 6
Table of Commands ..................................................................................................................................... 7
Timing Characteristics ................................................................................................................................ 13
Example Initialization Sequence ................................................................................................................. 17
Quality Information ................................................................................................................................... 19




## Additional Resources
➢ Support Forum: https://support.newhavendisplay.com/hc/en-us/community/topics
➢ GitHub: https://github.com/newhavendisplay
➢ Example Code: https://support.newhavendisplay.com/hc/en-us/categories/4409527834135-Example-Code/
➢ Knowledge Center: https://www.newhavendisplay.com/knowledge_center.html
➢ Quality Center: https://www.newhavendisplay.com/quality_center.html
➢ Precautions for using LCDs/LCMs: https://www.newhavendisplay.com/specs/precautions.pdf
➢ Warranty / Terms & Conditions: https://www.newhavendisplay.com/terms.html




## 2

## Document Revision History
## Revision Date Description Changed By
- 04/29/2024 Initial Release KL


## C
## B
## A
## D
## E
## F
## C
## B
## A
## D
## E
## F
## 43218765
## 43218765
## Mechanical Drawing
## Drawn Date:
Drawing/Part Number:
## Standard Tolerance:
(Unless otherwise specified)
## Linear:   ±0.3mm
## Revision:
## Drawn By:
## K. Lewis
## Approved By:
## Approved Date:
## 04/29/2024
## 04/29/2024
## K. Lewis
Unless otherwise specified:
- Dimensions are in Millimeters
## •  Third Angle Projection
## NHD-1.91-176176UBC3
## -
This drawing is solely the property of Newhaven Display International, Inc.
The information it contains is not to be disclosed, reproduced or copied in
whole or part without written approval from Newhaven Display.
## 2VDD
## 3NC
## 4D/C#
## 5
## R/W#
## 6
## E
## 7DB0
## 8DB1
## 9
## DB2
## 10DB3
## 11DB4
## 12DB5
## 13DB6
## 14DB7
## 15GND
## 16RES#
## 17
## CS#
## 18
## GND
## 19BS1
## 20BS2
## 1GND
Product Descrip�on: 1.91” 176x176 Color OLED
1.Driver IC: SSD1333
2.Interface: 8-bit 6800/8080 Parallel, 4-wire SPI, I²C
3.Power Requirement: 3.3V OLED
4.Op�cal Features: Full Color, Full View
5.Recommended Pin Header: 2x10pin 2.54mm pitch



## 4

## Pin Description
## Parallel Interface:
## Pin No. Symbol External Connection Function Description
1 GND Power Supply Ground
## 2 V
## DD
Power Supply Supply Voltage for OLED and logic.
3 NC - No Connect
4 D/C# MPU Register select signal. D/C=0: Command,  D/C=1: Data
## 5 R/W#
## WR#
MPU 6800-interface:
Read/Write select signal, R/W=1: Read  R/W: =0: Write
## 8080-interface:
Active LOW Write signal.
## 6 E
## RD#
MPU 6800-interface:
Operation enable signal.  Falling edge triggered.
## 8080-interface:
Active LOW Read signal.
7-14 DB0 – DB7 MPU 8-bit Bi-directional data bus lines.
15 GND Power Supply Ground
16 RES# MPU Active LOW Reset signal.
17 CS# MPU Active LOW Chip Select signal.
18 GND Power Supply Ground
19 BS1 MPU Interface Select Signal
20 BS2 MPU Interface Select Signal
4-wire SPI Interface:
## Pin No. Symbol External Connection Function Description
1 GND Power Supply Ground
## 2 V
## DD
Power Supply Supply Voltage for OLED and logic.
3 NC - No Connect
4 D/C# MPU Register select signal. D/C=0: Command,  D/C=1: Data
5-6 GND Power Supply Ground
7 SCLK MPU Serial Clock signal.
8 SDIN MPU Serial Data Input signal.
9-15 GND Power Supply Ground
16 RES# MPU Active LOW Reset signal.
17 CS# MPU Active LOW Chip Select signal.
18 GND Power Supply Ground
19 BS1 MPU Interface Select Signal
20 BS2 MPU Interface Select Signal

I²C Interface:
## Pin No. Symbol External Connection Function Description
1 GND Power Supply Ground
## 2 V
## DD
Power Supply Supply Voltage for OLED and logic.
3 NC - No Connect
4 SA0 MPU Slave Address Selection signal
5-6 GND Power Supply Ground
7 SCL MPU Serial Clock signal.
## 8 SDA
## IN
MPU Serial Data Input signal
## 9 SDA
## OUT
MPU Serial Data Output signal
10-15 GND Power Supply Ground
16 RES# MPU Active LOW Reset signal.
17-18 GND Power Supply Ground
19 BS1 MPU Interface Select Signal
20 BS2 MPU Interface Select Signal
Recommended display connector: 2x10pin 2.54mm pitch



## 5

## Interface Selection
MPU Interface Pin Selections and Assignment Summary
6800 Parallel 8080 Parallel 4-Wire SPI I
## 2
## C
## BS1 0 1 0 1
## BS2 1 1 0 0

## Bus
## Interface
Data/Command Interface Control Signals
## D7 D6 D5 D4 D3 D2 D1 D0 E R/W# CS# D/C# RES#
8-bit 6800 D[7:0] E R/W# CS# D/C# RES#
8-bit 8080 D[7:0] RD# WR# CS# D/C# RES#
4-Wire SPI Tie Low SDIN SCLK Tie Low CS# D/C# RES#
## I
## 2
C Tie Low SDA
## OUT
## SDA
## IN
SCL Tie Low SA0 RES#
## I
## 2
C Interface: SDA
## IN
and SDA
## OUT
are tied together and serve as SDA. The SDA
## IN
pin must be connected to act as SDA. The SDA
## OUT
pin
may be disconnected. When SDA
## OUT
pin is disconnected, the acknowledgement signal will be ignored in the I
## 2
## C-bus.

## Wiring Diagram






## 6

## Electrical Characteristics
## Item Symbol Condition Min. Typ. Max. Unit
## Operating Temperature Range T
## OP
Absolute Max -40 - +70 ⁰C
## Storage Temperature Range T
## ST
Absolute Max -40 - +85 ⁰C
Supply Voltage for Logic V
## DD
## - 2.7 3.3 3.5 V
## Supply Current  I
## DD
## V
## DD
=3.3V, 100% ON - 285 310 mA
Sleep mode Current I
## DD_ SLEEP

## V
## DD
= 3.3V - 2 10 μA
“H” Level input V
## IH
## - 0.8*V
## DD
## - V
## DD
## V
“L” Level input V
## IL
## - V
## SS
## - 0.2*V
## DD
## V
“H” Level output V
## OH
## - 0.9*V
## DD
## - V
## DD
## V
“L” Level output V
## OL
## - V
## SS
## - 0.1*V
## DD
## V

## Optical Characteristics
## Item Symbol Condition Min. Typ. Max. Unit
## Optimal Viewing
## Angles
## Top
φY+
## -
## - 80 - ⁰
## Bottom
φY-
## - 80 - ⁰
## Left
θX-
## - 80 - ⁰
Right θX+ - 80 - ⁰
Contrast Ratio CR - - >10,000:1 - -
## Response Time
## Rise  T
## R
## -
- 10 - μs
## Fall T
## F
- 10 - μs
## Brightness
## L
## V
- 70 80 - cd/m
## 2
## Lifetime -
## 80cd/m², T
## OP
## =25°C, 50%
## Checkerboard
## 16,500 - - Hrs.
## Chromaticity
## Red
## X
## R
## - 0.62 0.66 0.70 -
## Y
## R
## - 0.29 0.33 0.37 -
## Green
## X
## G
## - 0.26 0.30 0.34 -
## Y
## G
## - 0.59 0.63 0.67 -
## Blue
## X
## B
## - 0.10 0.14 0.18 -
## Y
## B
## - 0.14 0.18 0.22 -
## White
## X
## W
## - 0.26 0.30 0.34 -
## Y
## W
## - 0.29 0.33 0.37 -
Note:  Lifetime  at  typical temperature  is  based  on  accelerated  high-temperature  operation.  Lifetime  is  tested  at
average 50% pixels on and is rated as Hours until Half-Brightness. The Display OFF command can be used to extend
the lifetime of the display.
Luminance  of  active  pixels  will  degrade  faster  than  inactive  pixels.  Residual  (burn-in)  images  may  occur.  To  avoid
this, every pixel should be illuminated uniformly.

Controller information
Built-in SSD1333 Controller: https://support.newhavendisplay.com/hc/en-us/articles/11256414111767-SSD1333








## 7

Table of Commands








## 8






## 9








## 10





## 11





## 12

























## 13

## Timing Characteristics
6800-Series MCU Parallel Interface:







## 14

8080-Series MCU Parallel Interface:







## 15

4-wire SPI:














## 16

## I2C:






## 17

## Example Initialization Sequence
void Init_OLED(void)
## {
digitalWrite(P16,LOW);              //RES
delay(10);
digitalWrite(P16,HIGH);             //RES
Set_Command_Lock(0x12);             // Unlock Driver IC
Set_Display_On_Off(0xAE);           // Display Off
Set_Remap_Format(0x64);             // Set Horizontal Address Increment
Set_Start_Line(0x00);               // Set Mapping RAM Display Start Line
Set_Display_Offset(0x00);           // Shift Mapping RAM Counter
Set_Display_Mode(0xA6);             // Normal Display Mode
Set_Phase_Length(0x24);
Set_Display_Clock(0x80);
Set_Precharge_Period(0x0F);         // Set Second Pre-Charge Period
Set_Precharge_Voltage(0x1F);        // Set Pre-Charge Voltage Level as 0.40*VCC
Set_VCOMH(0x05);                    // Set Common Pins Deselect Voltage Level as
## 0.82*VCC
Set_Contrast_Color(0x3C,0x39,0x43); // Set Contrast of Color A
Set_Master_Current(0x0F);           // Set Scale Factor of Segment Output Current
## Control
Set_Multiplex_Ratio(0xAF);          // 1/128 Duty (Max_Row)
command(0xB8);                      //Gamma Look-Up Table
data(0x00);//1
data(0x02);//2
data(0x04);//3
data(0x06);//4
data(0x07);//5
data(0x08);//6
data(0x09);//7
data(0x0a);//8
data(0x0b);//9
data(0x0c);//10
data(0x0d);//11
data(0x0e);//12
data(0x10);//13
data(0x11);//14
data(0x12);//15
data(0x14);//16
data(0x15);//17
data(0x16);//18
data(0x18);//19
data(0x1a);//20
data(0x1b);//21
data(0x1d);//22
data(0x1f);//23
data(0x20);//24
data(0x22);//25
data(0x24);//26
data(0x26);//27
data(0x27);//28
data(0x29);//29
data(0x2b);//30
data(0x2d);//31
data(0x2f);//32
data(0x31);//33
data(0x33);//34
data(0x35);//35
data(0x37);//36
data(0x39);//37



## 18

data(0x3b);//38
data(0x3d);//39
data(0x3f);//40
data(0x42);//41
data(0x44);//42
data(0x46);//43
data(0x48);//44
data(0x4b);//45
data(0x4d);//46
data(0x50);//47
data(0x52);//48
data(0x55);//49
data(0x57);//50
data(0x5a);//51
data(0x5b);//52
data(0x5f);//53
data(0x62);//54
data(0x65);//55
data(0x67);//56
data(0x6a);//57
data(0x6d);//58
data(0x70);//59
data(0x73);//60
data(0x76);//61
data(0x79);//62
data(0x7c);//63
Set_Display_On_Off(0xAF);           // Display On
## }




## 19

## Quality Information
Test Item Content of Test Test Condition Note
High Temperature storage Test the endurance of the display at high
storage temperature.
+85⁰C, 240hrs 2
Low Temperature storage Test the endurance of the display at low
storage temperature.
-40⁰C, 240hrs 1,2
## High Temperature
## Operation
Test the endurance of the display by
applying electric stress (voltage & current)
at high temperature.
+70⁰C, 240hrs 2
## Low Temperature
## Operation
Test the endurance of the display by
applying electric stress (voltage & current)
at low temperature.
-40⁰C, 240hrs 1,2
## High Temperature /
## Humidity Operation
Test the endurance of the display by
applying electric stress (voltage & current)
at high temperature with high humidity.
+65⁰C, 90% RH, 96hrs 1,2
Thermal Shock resistance Test the endurance of the display by
applying electric stress (voltage & current)
during a cycle of low and high
temperatures.
-40⁰C, 30min-> 85⁰C,30min
= 1 cycle
20 cycles

Vibration test Test the endurance of the display by
applying vibration to simulate
transportation and use.
5-50Hz, 0.5G
2hrs in each of 3 directions
## X,Y,Z
## 3
Static electricity test Test the endurance of the display by
applying electric static discharge.
Air discharge ±8kV
10 times

Note 1: No condensation to be observed.
Note 2: Conducted after 2 hours of storage at 25⁰C, 0%RH.
Note 3: Test performed on product itself, not inside a container.
