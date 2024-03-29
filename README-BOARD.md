Developing for the STM32F746 using GCC
======================================

Board description
-----------------

> ***It is a good idea to verify if there is a firmware upgrade for the STM32F746G Discovery board. Download the software from [7] and upgrade the firmware.***


The main features of the STM32F74GDISCOVERY board are:

* STM32F746NGH6 Cortex M7 microprocessor with:
    - 1 MByte of Flash memory
    - 340 KBytes of RAM memory


* Color 4.3" LCD display with 480x272 resolution with capacitive touch screen
* Ethernet with RJ45 connector
* USB Full Speed with OTG
* USG High Speed with OTG
* 128 Mbit Flash memory with Quad SPI interface
* 128 Mbit SDRAM (only 64 MBits accessible)
* Two pushbuttons (one used to reset).
* One green LED controlled by PI1.
* SAI audio codec (?).
* Arduino V3 compatible expansion connectors.


The board has many additional connectors:

* Camera 
* MicroSD card
* SPDIF RCA input connector
* Audio line in
* Audio line out
* Stereo speaker outputs

For programming, it has a inboard ST-LINK interface with a MiniUSB connector.

OBS: There is a, not clearly documented, green LED on ARD_D13 signal (pin 6 of Arduino connector CN7).
It is driven by PI1 (Pin D14 of STM32F746NGH6).

Memory map
----------

The 4 GByte address range of the processor is divides in blocks.

Address Block           |  Size      |  Contents 
------------------------|------------|----------------
0x0000_0000-0x000F_FFFF | 512 MByte  | System Memory, Flash memory, ITCM RAM
0x2000_0000-0x3FFF_FFFF | 512 MByte  | RAM, DTCM Memory registers, etc
0x4000_0000-0x5FFF_FFFF | 512 MByte  | APB1, APB2, AHB1, AHB2, AHB3 peripherals
0x6000_0000-0xDFFF_FFFF |   2 GByte  | Flexible Memory Controller area
0xE000_0000_0xFFFF_FFFF | 512 MByte  | Cortex M7 internal peripherals

###  RAM Memory

There are 340 KBytes of RAM memory, but not contiguous. Some are very fast, other 
can be no volatile.

The RAM is composed of five areas as below:

Area        | Size   | Address range           | Access
------------|--------|------------------------ |-------------------
DTCM        |  64 KB | 0x2000_0000-0x2000_FFFF | All AHB masters, no wait state, 64 bits access
SRAM1       | 240 KB | 0x2001_0000-0x2004_BFFF | All AHB masters
SRAM2       |  16 KB | 0x2004_C000-0x2004_FFFF | All AHB masters
Subtotal    | 320 KB |                         |
ITCM RAM    |  16 KB | 0x0000_0000-0x0000_3FFF | Only CPU, no wait state, 64 bits access, no write 
Backup SRAM |   4 KB | 0x4002_4000-0x4002_4XXX | Powered by VBAK
Total       | 340 KB |                         | 

DTCM, SRAM1 and SRAM2 for a 320 KB contiguous area.
In the board, the VBAT is connected to VDD.

### Flash memory

There is a 1 MByte Flash memory, which can be accessed through two interfaces.

Area          | Size   | Bus  | Address Range           | Observations
--------------|--------|------|-------------------------|------------------------
ITCM Window   | 1 MB   | ITCM | 0x0020_0000-0x002F_FFFF | zero wait state, <br/> ART (Adaptive Real Time ) accelerator, <br/> 64 bit access
AXIM window   | 1 MB   | AXIM | 0x0800_0000-0x080F_FFFF | 4 KB cache, 64 bit access

The BOOT0 pin specifies which code is executed after reset. In the board, it is connected to GND thru a 10 K resistor. 

BOOT0	|	Flash           | Address Range
--------|-------------------|------------------
0       | Flash with ITCM   | 0x0020_0000-0x002F_FFFF. 
1       | System bootloader | 0x0010_0000-0x0010_FFFF. Can be set anywhere with 16k precision

The address range can be set anywhere by configuring registers   and    .

AXIM (Advanced eXtensible Interface) is a ARM technology. It has a 64 bit interface.
ITCM (Tightly-Coupled Memory) is a ST technology.  It has a 64 bit interface split in two 32 bit ports: D0TCM and D1TCM.

In the STM32F74xxx and STM32F75xxx device, the flash memory can be read in chunks of 256 bits. In other devices, 128 bits. The is an ART (Adaptive Real-Time) accelerator), a T technology, that implements, among other things, a Pre-Fetcher (ART Prefetch). The ART in the STM32F7 works only for flash memory access using the ITCM bus and implements a cache of 128/256 bits x 64 lines.It should allow a near 0-wait access to flash.

The latency of Flash memory must be adjuste according the CPU Clock and voltage source as show in the table below.

<table>
<thead>
<td>Wait  states     <td colspan=4>Voltage
<thead>
<td>&nbsp; <td> 2.7 - 3.6 V      <td>  2.4 - 2.7 V    <td> 2.1 - 2.4 V      <td>  1.8 - 2.1 V
<tr>
<td> 0 WS (1 CPU cycle)   <td>    0 < HCLK ≤ 30  <td>    0 < HCLK ≤ 24 <td>    0 < HCLK ≤ 22   <td>   0 < HCLK ≤ 20
<tr>
<td> 1 WS (2 CPU cycles)   <td>  30 < HCLK ≤ 60  <td>  24 < HCLK ≤ 48  <td>  22 < HCLK ≤ 44   <td>  20 < HCLK ≤ 40
<tr>
<td>2 WS (3 CPU cycles)  <td>  60 < HCLK ≤ 90  <td>  48 < HCLK ≤ 72  <td>  44 < HCLK ≤ 66   <td>  40 < HCLK ≤ 60
<tr>
<td>3 WS (4 CPU cycles)  <td>  90 < HCLK ≤ 120 <td>  72 < HCLK ≤ 96  <td>  66 < HCLK ≤ 88   <td>  60 < HCLK ≤ 80
<tr>
<td>4 WS (5 CPU cycles)  <td> 120 < HCLK ≤ 150 <td>  96 < HCLK ≤ 120 <td>  88 < HCLK ≤ 110  <td>  80 < HCLK ≤ 100
<tr>
<td>5 WS (6 CPU cycles)  <td> 150 < HCLK ≤ 180 <td> 120 < HCLK ≤ 144 <td> 110 < HCLK ≤ 132  <td> 100 < HCLK ≤ 120
<tr>
<td>6 WS (7 CPU cycles)  <td> 180 < HCLK ≤ 210 <td> 144 < HCLK ≤ 168 <td> 132 < HCLK ≤ 154  <td> 120 < HCLK ≤ 140
<tr>
<td>7 WS (8 CPU cycles)  <td> 210 < HCLK ≤ 216 <td> 168 < HCLK ≤ 192 <td> 154 < HCLK ≤ 176  <td> 140 < HCLK ≤ 160
<tr>
<td>8 WS (9 CPU cycles)  <td> -                <td> 192 < HCLK ≤ 216 <td> 176 < HCLK ≤ 198  <td> 160 < HCLK ≤ 180
<tr>
<td>9 WS (10 CPU cycles) <td> -                <td> -                <td> 198 < HCLK ≤ 216  <td>  -
</table>

Additionaly,

 * when VOS = 0x01, f_HCLKmax = 144 MHz
 * when VOS = 0x10, f_HCLKmax = 168 MHz or 180 MHz with overdrive
 * when VOS = 0x11, f_HCLKmax = 180 MHz or 216 MHz with overdrive
 * Overdrive only availabe for supply voltages above 2.1 V

After reset, the clock frequency is 16 MHz and the flash is configured to 0 wait states.

There is a system memory in the range 0x0010_0000-0x0010_EDBF, which contains
a serial boot loader written during manufacturing.

THere is an additional 1 KByte OTP memory.

The 4 KB Backup SRAM is controlled by the Flash controller and is located at the range 

### Clock

There are three clock sources for the SYSCLK:

* HSICLK: High Speed Internal Clock is an internal RC oscillator clock with 16 MHz frequency
* HSECLK: High Speed External Clock is an external clock based on a 4-26 MHz crystal or signal
* PLLCLK: Main PLL clock is generated by a PLL unit based on HSI ou HSE and can reach frequency as high as 216 MHz.

There are two additional clock sources, used by certain peripherals:

* LSIRCCLK: Low Speed Internal 32 KHz RC oscillator is used by RTC
* LSECLK: Low Speed External 32768 Hz is based on a external crystal or signal and it is by RTC.
* USBCLK: USB Clock is a 24-60 MHz external crystal based oscillator used by USB HS unit.
* ETHCLK: Ethernet Clock is a 25-50 MHz external crystal based oscillator used by Ethernet MAC unit

![Clock-Source](Figures/Clock-Source2.gif)

The HCLK is derived from the SYSCLK through a prescaler. It is used as a clock signal for the core, the memory, DMA and the Advanced High Speed Bus (AHB). Following CMSIS, its frequency is stored in the *SystemCoreClock* variable and is updated by the *SystemCoreClockUpdate* function.

There are two Advanced Peripheral Bus clock signals, both are derived from the HCLK through prescalers. The prescalers are power of 2 in the range 1 to 16.


Clock signal |	Max. Freq	| Source         | Prescaler     |  Prescaler range 
-------------|--------------|----------------|---------------|--------------------
SYSCLK       |	216 MHz     | PLL, HSI, HSE  |  -            |     -
HCLK         |  216 MHz     | SYSCLK         | RCC_CFGR_HPRE | 1, 2, 4, 8, 16, 64, 128, 256, 512
APB1         |   54 MHz     | HCLK           | RCC_CFGR_PPRE1| 1,2,4,8,16
APB2         |  108 MHz     | HCLK           | RCC_CFGR_PPRE2| 1,2,4,8,16


Before using a peripheral unit, its clock must be enabled.

There is a PLL unit to generate clock signal for the SAI unit and another one for the I2S units.


### LED and buttons

There is one LED under direct control of software. It is the LD1 (Green) connected to PI1 and to the PIN 6 of CN7 (Arduino D13).

There is a **blue** button (USER/WAKE-UP) connected to the board signal B_USER and to the PI11 (Active low).

The **black** button (RESET) is connected to the NRST board line and NRST MCU input.


The other LEDs (not controlled by software) are:

* LD2 : 5V Power Supply (Red)
* LD3 : USB HS overcurrent (Red)
* LD4 : USB HS power switch (Green)
* LD5 : USB FS power switch (Green)
* LD6 : USB FS overcurrent (Red)
* LD7 : Controlled by the STLINK MCU (Red/Green)



### Serial

There are two serial communication channels with the host. One uses two lines to the Debug MCU as below.

Signal     |  Board signal      | Jumper    | STM32F746G Pin  |  Unit
-----------|--------------------|----- -----|-----------------|-------------------
ST_LINK TX |  VCP_RX            | SB13      |      PB7        | UART1_RX (AF7)
ST_LINK_RX |  VCP_TX            | SB12      |      PA9        | UART1_TX (AF7)

(Alternative to OTG_FS_VBUS)

The other is the semihosting, which uses the debug lines to communicate. To output a character, a BKPT is used (new generations, above Cortex M1 and M3) or a SWI instruction.


There is another technology called ITM serial, which uses the debug lines to transfer information. It works only when using SWD interface. It uses the PB3 pin.

To use it, one must call the following routines:

	ITM_SendChar(char c);


### External RAM

The board uses a 16-MByte MT48LC4M32B2B5-6A SDRAM device to expand its RAM using the Flexible Memory Controller. In recent version of the board, the fully compatible ISSI IS42S32400F-6BL device is used instead.

The device is a PC133 compatible SDRAM and it has four banks of 1 M cell with 32 bits. In total, it has then 128 MBits (=16 Mbytes) but only half of them will be reached due to limitations in the MCU.

It has the following interface

Signal    | Type |     Description
----------|------------------------------------
DQ31..0   |  I/O | 32-bit data bus
A11..0    |  I   | 12 bit address input
BA1..0    |  I   | 2 bit bank selector
DQM3..0   |  I   | 4 bit mask input to enable byte write operation
CAS#      |  I   | Column Address Selector
RAS#      |  I   | Row Address Selector
WE#       |  I   | Write operation
CS#       |  I   | Chip select
CKE       |  I   | Clock enable
CLK       |  I   | Clock

Only 16 bits of data bus are used. So it is not neccessary to use full DQM. Only DQM0 and DQ1 are used.

It is connected to the MCU as showm below.

Chip signal | Board signal | MCU Signal |  Alternate function
------------|--------------|---------|------------
DQ0         |  FMC_D0      | PD14    |  AF12
DQ1         |  FMC_D1      | PD15    |  AF12
DQ2         |  FMC_D2      | PD0     |  AF12
DQ3         |  FMC_D3      | PD1     |  AF12
DQ4         |  FMC_D4      | PE7     |  AF12
DQ5         |  FMC_D5      | PE8     |  AF12
DQ6         |  FMC_D6      | PE9     |  AF12
DQ7         |  FMC_D7      | PE10    |  AF12
DQ8         |  FMC_D8      | PE11    |  AF12
DQ9         |  FMC_D9      | PE12    |  AF12
DQ10        |  FMC_D10     | PE13    |  AF12
DQ11        |  FMC_D11     | PE14    |  AF12
DQ12        |  FMC_D12     | PE15    |  AF12
DQ13        |  FMC_D13     | PD8     |  AF12
DQ14        |  FMC_D14     | PD9     |  AF12
DQ15        |  FMC_D15     | PD10    |  AF12
A0          |  FMC_A0      | PF0     |  AF12
A1          |  FMC_A1      | PF1     |  AF12
A2          |  FMC_A2      | PF2     |  AF12
A3          |  FMC_A3      | PF3     |  AF12
A4          |  FMC_A4      | PF4     |  AF12
A5          |  FMC_A5      | PF5     |  AF12
A6          |  FMC_A6      | PF12    |  AF12
A7          |  FMC_A7      | PF13    |  AF12
A8          |  FMC_A8      | PF14    |  AF12
A9          |  FMC_A9      | PF15    |  AF12
A10         |  FMC_A10     | PG0     |  AF12
A11         |  FMC_A11     | PG1     |  AF12 ????
BA0         |  FMC_BA0     | PG4     |  AF12
BA1         |  FMC_BA1     | PG5     |  AF12
RAS         |  FMC_SDNRAS  | PF11    |  AF12
CAS         |  FMC_SDNCAS  | PG15    |  AF12
WE          |  FMC_SNDWE   | PH5     |  AF12
CLK         |  FMC_SDCLK   | PG8     |  AF12
CLKE        |  FMC_SDCKE0  | PC3     |  AF12
CS          |  FMC_SDNE0   | PH3     |  AF12
DQM0        |  FMC_NBL0    | PE0     |  AF12
DQM1        |  FMC_NBL1    | PE1     |  AF12



### LCD

The STM32F646 has a LCD control interface and an accelerator, called ChromeART (DMA2D).

The LCD unit is a 4.3" Liquid Crystal Display (LCD) display with Capacitive Touch Panel (CTP). 
It is produced by Rocktech under the code RK043FN48H-CT672B.

It is produced by Rocktech under the code RK043FN48H-CT672B. Its resolution is 480x272 and can
 display 16777216 colors (RGB888 interface).

The main features are:

| Feature        | Description
|----------------|------------------
| Part No.       | RK043FN02H-CT
| Size           | 4.3"
| Resolution     | 480*272
| AA             | 95.04 x 53.86
| Outline Size   | 105.50 x 67.20 x 4.35
| Interface      | RGB for TFT,I2C for CTP
| Brightness     | 400(after TP)
| Contrast       | 500:1
| View angle     | 70/50/70/70
| Touch Panel    | Optional
| Remarks        | 4.3" TFT with CTP
| Application    | Mobile Devices,POS, GPS,black Box, Security,Evaluation Kits etc


The most importante parameters are:

Parameter         |	Min      |   Typ     |  Max    | Unit
------------------|----------|-----------|---------|----------
DCLK Frequency    |     5    |    9      |    12   | MHz
------------------|----------|-----------|---------|----------
HSYNC Period      |   490    |  531      |   605   | DCLK
HSYNC Display     |          |  480      |         | DCLK
HSYNC Back porch  |     8    |   43      |         | DCLK
HSYNC Front porch |     2    |    8      |         | DCLK
------------------|----------|-----------|---------|----------
VSYNC Period      |   275    |  288      |   335   | DCLK
VSYNC Display     |          | 272       |         | H
VSYNC Back porch  |     1    |  12       |         | H
VSYNC Front porch |     1    |  40       |         | H

The connection for LCD are:
 
 LCD signal   | Board signal      | MCU signal             |
--------------|-------------------|------------------------|
  CLK         | LCD_CLK           | PI14                   |
  LCD_R       | LCD_R0-7          | PI15 PJ0-6             |
  LCD_G       | LCD_G0-7          | PJ7-11 PK0-2           |
  LCD_B       | LCD_B0-7          | PE4 PJ13-15 PG12 PK4-6 |
  HSYNC       | LCD_HSYNC         | PI10                   |
  VSYNC       | LCD_VSYNC         | PI9                    |
  DE          | LCD_DE            | PK7                    |
  INT         | LCD_INT           | PI13                   |
  SCL         | LCD_SCL           | PH7                    |
  SDA         | LCD_SDA           | PH8                    |
  SDA         | LCD_RST/NRST      | NRST                   |


The alternative for LCD pins are shown below. Those used for LCD connection are marked.


LCD Signal   | ALT     |  Pin
-------------|---------|--------------------
LCD_CLK      |  AF14   | *PI14* PE14 PG7
LCD_R0       |  AF14   | PG13 PH2 *PI15*
LCD_R1       |  AF14   | PA2 PH3 *PJ0*
LCD_R2       |  AF14   | PA1 PC10 PH8 *PJ1*
LCD_R3       |  AF14   | PH9 *PJ2* PB0 (AF9)
LCD_R4       |  AF14   | PA5 PA11 PH10 *PJ3*
LCD_R5       |  AF14   | PA12 PC0 PH11 *PJ4*
LCD_R6       |  AF14   | PA8 PH12 *PJ5* PB1(AF9)
LCD_R7       |  AF14   | PE15 PG6 *PJ6*
LCD_G0       |  AF14   | PE5 *PJ7*
LCD_G1       |  AF14   | PE6 *PJ8*
LCD_G2       |  AF14   | PA6 PH13 *PJ9*
LCD_G3       |  AF14   | PE11 PH14 *PJ10* PG10(AF9)
LCD_G4       |  AF14   | PB10 PH15 *PJ11*
LCD_G5       |  AF14   | PB11 PI0 *PK0*
LCD_G6       |  AF14   | PC7 PI1 *PK1*
LCD_G7       |  AF14   | PD3 PI2 *PK2*
LCD_B0       |  AF14   | *PE4* PG14 PJ12
LCD_B1       |  AF14   | PG12 *PJ13*
LCD_B2       |  AF14   | PD6 PG10 *PJ14*
LCD_B3       |  AF14   | PD10 PG11 *PJ15*
LCD_B4       |  AF14   | PE12 PG12 *PG12(AF9)* PI4 PK3
LCD_B5       |  AF14   | PA3 PI5 *PK4*
LCD_B6       |  AF14   | PB8 PI6 *PK5*
LCD_B7       |  AF14   | PB9 PI7 *PK6*
LCD_VSYNC    |  AF14   | PA4 *PI9* PI13
LCD_HSYNC    |  AF14   | PC6 *PI10* PI12
LCD_DE       |  AF14   | PE13 PF10 *PK7*


The following uses GPIO I/O or other modes

LCD Signal    |        |  Pin   |  Description
--------------|--------|--------|------------------
 LCD_DISP     | GPIO   | I12    |  Enable LCD
 LCD_INT      | GPIO   | I13    |  LCD Interrupt
 LCD_BL_CTRL  | GPIO   | K3     |  Backlight PWM

#### Serial flash memory

The boad has a MT25QL128ABA serial flash memory. It is a 128 Mb (64 MB) memory with a 
four lane QSPI interface.

It has a 6 signal interface:

Signal  |   Description
--------|------------------
DQ0     |   Data lane (LSB)
DQ1     |   Data lane
DQ2     |   Data lane (Used as W#: Write Protect# signal)
DQ3     |   Data lane (MSB) (Used as a Hold# signal)
C       |   Clock
S#      |   Chip Select (Active low)

The clock frequency can be:

* 133 MHz for single transfer rate
* 90 MHz for double transfer rate

It supports in STR and DTR:
* Extended I/O Protocol
* Dual I/O Protocol
* Quad I/O Protocol

It has 256 sectores (numbered 0 to 255), each sector has two 32 KB subsectors (numbered 0 to 511).
Each sector can be regarded as having eight 4B subsectors.

All memory is addreass in the range 0 to 0x00FF_FFFF. 

The microcontroller has a QUADSPI interface for flash memories. In the memory mapped mode, the flash memory ]
occupies a region in the address space of microcontroller.


#### Micro SD Card interface

The board has micro SD connector, directly connected to the MCU. The MCU has a on chip SDMMC1 port controlled by the SDMMC host interface.



|Board signal | MCU Pin | microSD Pin   | AF#  | Description             |
|-------------|---------|---------------|------|-------------------------|
| uSD_D0      |  PC8    |      7        | AF12 | SDMMC1_D0               |
| uSD_D1      |  PC9    |      8        | AF12 | SDMMC1_D1               |
| uSD_D2      |  PC10   |      1        | AF12 | SDMMC1_D2               |
| uSD_D3      |  PC11   |      2        | AF12 | SDMMC1_D3               |
| uSD_CMD     |  PD2    |      3        | AF12 | SDMMC1_CMD              |
| uSD_CLK     |  PC12   |      5        | AF12 | SDMMC1_CK               |
| uSD_Detect  |  PC13   |     10        | AF0  | Tamper detect (GPIO)    |
| 3V3         |         |      4        |      | VDD                     |
| GND         |         |     6,9       |      | VSS                     |



#### External flash

The board has a 16-MBytes Micron N25Q128A13EF840E NOR Flash with a Quad SPI interface.
In recent version, the fully compatible MT25QL128ABA1EW9-0SIT device is used instead.

|Board signal	 | MCU Pin | NOR Flash Pin   | AF#  | Description           |
|----------------|---------|-----------------|------|=----------------------|
| QSPI_NCS       |   PB6   | S#              | AF10 | QUADSPI_BK1_NCS       |
| QSPI_CLK       |   PB2   | C               | AF9  | QUADSPI_CLK           |
| QSPI_D0        |   PD11  | DQ0             | AF9  | QAUDSPI_BK1_IO0       |
| QSPI_D1        |   PD12  | DQ1             | AF9  | QAUDSPI_BK1_IO1       |
| QSPI_D2        |   PE2   | DQ2/Vpp/W#      | AF9  | QAUDSPI_BK1_IO2       |
| QSPI_D3        |   PD13  | DQ3/HOLD#       | AF9  | QAUDSPI_BK1_IO3       |


#### I2C

Board signal       | MCU Pin
-------------------|-------------------|------------------
LCD_SCL, AUDIO_SCL | PH7
LCD_SDA, AUDIO_SDA | PH8



#### Clock

The LCD unit is driven by three clock signals:

* HCLK:  For data transfer
* APB2 PLCK2: For configuration registers
* LCD_CLK: Pixel clock domain. It is connected to the LCD

The LCD unit is driven by a clock derived from PLLSAI.The R divisor is used to driven it. The valid range is 2 to 7. 
An aditional divider can be set on PLLSAIDIVR (2,4,8,16) of RCC_DCKCFGR1.

The Pixel Clock frequency of 9.5 MHz is shown as an example for 480x272 display

The LCD support a frequency in the range 5-12 MHz.
The HSYNC can take 490-605 of clock pulses.
The VSYNC can take 274 to 335 horizontal hsync pulses.

#### Aditional signals

It LCD unit be reset by setting pin LTDCRST (26) of RCC_APB2RSTR.

There is a global interrupt for event and error for LCD-TFT unit on position 88 and 89 of vector table, respectively.


The backlight is controlled by LCD_BL_CTRL.


The capacitive control panel (CTP) is connected to a I2C interface pin of the MCU. It is shared with the Audio I2C.

#### Connection

The connection to MCU is

LCD signal  | Board signal      |   MCU signal
------------|-------------------|--------------------
CLK         | LCD_CLK           | PI14
LCD_R       | LCD_R0-7          | PI15 PJ1-7
LCD_G       | LCD_G0-7          | PJ7-11 PK0-2
LCD_B       | LCD_B0-7          | PE4 PJ13-15 PG12 PK4-6
HSYNC       | LCD_HSYNC         | PI10
VSYNC       | LCD_VSYNC         | PI9
DE          | LCD_DE            | PK7
INT         | LCD_INT           | PI13
SCL         | LCD_SCL           | PH7
SDA         | LCD_SDA           | PH8
SDA         | LCD_RST/NRST      | NRST
->VBL-/VBL+ | LCD_BL_CTRL       | PK3

The LCD is powered by a 3.3 V and it is controled by a PWM signal on LCD_BL_CTRL.


#### Memory

It uses a one or two frame buffers. 
Each buffer must be 383 KByte (=480*272*3=391680 bytes=382,5 KByte).


Memory organization

word     | Pixel
---------|-----------------
0        | B1 R0 G0 B0
1        | G2 B2 R1 G1
2        | R3 G3 B3 R2



#### Programming

1. Enable LTDC clock on RCC
2. Configure pixel clock
3. Configure VSYNC, HSYNC, Porch, active data area from LCD datasheet
4. Configure synchronous signal and clock polarity
5. Configure Backlight on LTDC->BCCR
6. Configure interrupts on LTDC->IER and LTDC->LIPCR
7. Configure Layer parameters
* Etc
8. Enable layer
9. Enable dithering on LTDC->GCR and LTDC->LxCKCR
10. Reload shadow registers thru LTDC->SRCR
11. Enable LCD-TFT thru LTDC->GCR




Installation
------------

1 - Install the gcc toolchain from (1).

2 - Install the STM32CubeF7 library from (3)

3 - Install stlink-tools (sudo apt install stlink-tools stlink-gui).

4 - Configure Makefile, specially the location of CMSIS include files in STM32CubeF7

Now it is possible to compile, flash and debug a project using make.

OBS: stm32flash (sudo apt install stm32flash) flashes a STM32 device using serial interface.



Configuration
-------------

### Boot

It is possible to modify the boot address controlling the voltage level on BOOT pin.

BOOT    | Boot address      |  Boot process
--------|-------------------|-------------------------------------------------
0       | BOOT_ADD0[15:0]   |  xxx   <br> Flash on ITCM at 0x0020_0000
1       | BOOT_ADD1[15:0]   |  xxx   <br> System boot loader at 0x0010_0000

System boot loader, programmed during manufacturing, permits the use of a serial interface to
program the Flash memory. This can be disabled by flash level 2 protection.

The BOOT0 pin is set to low thru R45, a 10 K resistor.

### Power Supply

The board can be powered by one of these ways:

* ST-LINK USB connector
* USB HS connector
* USB FS connector
* E5V line (VIN on Arduino CN14 connector or JP2 connector).

The E5V line is powered by a LD1117S50TR regulator with 800 mA current capacity from the VIN pin of the Arduino connector. VIN must be greater than 6 V and less than 12 V. Additionally, the 5 V can be connected to the JP2 connector. In this case, the VIN pin must be left unconnected.

The STM32F746NG MCU is powered by a 3.3 V line, obtained from the 5V USB line thru a LD39050PU33R regulator with 500 mA maximal current. This 5V line is configured by the JP1 connector to use one of the USB connector or the E5V. By default, it is energized by the ST_LINK USB connector. Furthermore, the powering of the STM32F746 is controlled by the ST-LINK MCU thru a ST890CDR switch. It asks the host to deliver 500 mA and only after confirmation, the rest of the board including the STM32F746NG is powered.

    E5V ---------| 1   2 |
    5V-ST_LINK---| 3<->4 |------ 5V
    5V-USB-FS----| 5   6 |
    5V_USB-HS----| 7   8 |

There is LD3985M33R regulator (150 mA maximal current) to power the ST-LINK MCU, a STM32F103CBT6. It is powered automatically by one of the USB connectors or the E5V line. No jumps required.

### Clock Frequency

The board uses four clock signals:

* A 25 MHz oscillator to provide the HSE input of the STM32F746NG
* A 32768 Hz crystal for the LSECLK oscillator inputs of the STM32F746NG
* A 25 MHz oscillator for the USB HS OTG and camera module.
* An 8 MHz crystal for the oscillator inputs of the ST-LINK MCU, a STM32F103CBT6.

Since HSE is generated by an external oscillator, the internal oscillator must be disable by setting HSEBP 
on RCC_CR. To use HSE, its stability must be checked verifying bit HSERDY of RCC_CR.
The HSE in enabled by setting the HSEON bit on RCC_CR.

At reset the internal HSI with a nominal frequency of 16 MHz is used as the source of System Clock (SYSCLK).
It is enabled by setting bin HSION on RCC_CR and its stability in bit RCC_CR.


The main PLL unit is enabled by bit PLLON and its stability by verifying bit PLLRDY. The clock source for the Main PPL unit is set in bit PLLSRC of RCC_PLL.


The output frequency is given by the formulas and must obey the limits shown.

![Clock-Source](Figures/Formulas2.png)

The values for the PLL registers are specified in the following table.

Register | Range
---------|-------------------------
M        | 2-63
N        | 50-432
P        | 2,4,6,8
Q        | 2-15
R        | 2-7

The M, N and P factors are shared with the other PLL units. The Q factor is a prescaler for these other clock signals.
The f_USBCLK must be 48 MHz.

The signal used to generate the SYSCLK (System Clock) is specified by field SW of RCC_CFGR.

SW  | SYSCLK
----|-------
00  | HSI
01  | HSE
10  | PLLCLK
11  | not allowed

The AHB prescaler is used to specify the HCLK frequency for core, memory and DMA among others.
It divides the SYSCLK to generate HCLK as shown in table below.

HPRE   |   Divisor
-------|------------
0XXX   |   1
1000   |   2
1001   |   4
1010   |   8
1011   |  16
1100   |  64
1101   | 128
1110   | 256
1111   | 512

OBS: There is no 32 divisor.




The PPRE2 field in RCC_CFGR register is used to configure the prescaler used to generate the APB High Speed Clock. It is a power of 2 in the range 1 to 16. This clock must have a frequency less than 108 MHz.

The PPRE1 field in RCC_CFGR register is used to configure the APB Low Speed Clock. It is a power of 2 in the range 1 to 16. This clock must have a frequency less than 54 MHz.

#### Procedure to change clock source

Enable new clock source
Configure it
Wait until it stabizes
Switch to the new one

#### Procedure to increase clock frequency.

1. Program the new number of wait states to the LATENCY bits in the FLASH_ACR register
2. Check that the new number of wait states is taken into account to access the Flash memory
by reading the FLASH_ACR register
3. Modify the CPU clock source by writing the SW bits in the RCC_CFGR register
4. If needed, modify the CPU clock prescaler by writing the HPRE bits in RCC_CFGR
5. Check that the new CPU clock source or/and the new CPU clock prescaler value is/are 
taken into account by reading the clock source status (SWS bits) or/and the AHB prescaler value (HPRE bits), respectively, in the RCC_CFGR register.

### Procedure to decrease clock frequenncy

1. Modify the CPU clock source by writing the SW bits in the RCC_CFGR register
2. If needed, modify the CPU clock prescaler by writing the HPRE bits in RCC_CFGR
3. Check that the new CPU clock source or/and the new CPU clock prescaler value is/are
taken into account by reading the clock source status (SWS bits) or/and the AHB
prescaler value (HPRE bits), respectively, in the RCC_CFGR register
4. Program the new number of wait states to the LATENCY bits in FLASH_ACR
5. Check that the new number of wait states is used to access the Flash memory by
reading the FLASH_ACR register

To unlock write access to FLASH_ACR.

1. Write KEY1 = 0x45670123 in the Flash key register (FLASH_KEYR)
2. Write KEY2 = 0xCDEF89AB in the Flash key register (FLASH_KEYR)

To lock write access to FLASH_ACR, set LOCK bit in the FLASH_ACR register

To access FLASH_ACR register, verify if BSY bit in the FLASH_SR register is unset.


Debug connections
-----------------

From schematics, there is only support for SWD/SWO.

The debug signal connections are shown in the table below.

Board signal    |   STM32F746   | Debug signal  |   STM32F103CBT6
----------------|---------------|---------------|---------------------
     PA14       | SWCLK         | T_JTCK        |   PB13
     PA13       | SWDIO         | T_JTMS        |   PB14
     NRST       | NRST          | T_RTST        |   PB0
     PB3        | SWO           | T_SWO         |   PB12
     PA9*       | VCP_TX        | STLINK_RX     |   PA3 (SB12)
     PB7        | VCP_RX        | STLINK_TX     |   PA2 (SB13)

*There is possibility to short it to OTG_FS_VBUS.

The jumpers SB12 and SB13 can disconnect the STLINK serial lines from STLINK_Controller.

> NOTE: Null modem connection at the STLINK serial lines. RX and TX crossed.

The T_JTDO, T_JTDI and T_JRST are not connected.

Entering DFU mode
-----------------

Device Firmware Upgrade Standard [8] is a mechanism for upgrading the onboard firmware.

The Micropython Wiki [9] has information on how to enable it on the STM32F746 Discovery Board. It should be noted that "The board doesn't provide a convenient way to enter DFU mode". But. there are some ways to enable it on the USBFS connector (ĆN13).

* Short the pads where R42 would be installed (it isn't installed by default) and press the reset button. You should now be able see the board when using dfu-util -l
* Install a switch between the MCU side of R42 and 3.3 V.
* Install the initial micropython image using the STLINK interface, and then use pyb.bootloader() to enter DFU mode from the REPL.




References
----------

1 - [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)

2 - [STM32Cube](https://www.st.com/en/ecosystems/stm32cube.html)

3 - [STM32CubeF7](https://www.st.com/en/embedded-software/stm32cubef7.html)

4 - [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)

5 - [STM32CubeMX HAL for make and gcc](https://wunderkis.de/stm32cube3/index.html)

6 - [STM32CubeF7](https://github.com/STMicroelectronics/STM32CubeF7)

7 - [STM32 Firmware Upgrade](https://www.st.com/content/st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-programmers/stsw-link007.html)

8 - [USB DFU - The USB Device Firmware Upgrade standard](https://www.usb.org/sites/default/files/DFU_1.1.pdf)

9 - [Micropython - Board STM32F746 Discovery](https://github.com/micropython/micropython/wiki/Board-STM32F746-Discovery)