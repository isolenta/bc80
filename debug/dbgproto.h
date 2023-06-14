#pragma once

/* DEBUG PROTOCOL DEFINITION

request encoded as 8bit byte: [p4][p3][p2][p1][p0][c1][c0][v]
response followed for any request immediately

v == 0 => raw pin access

p -- pin code (see below)
c -- subcommand:
  00 - nothing
  01 - configure as output and set to high; resp: same byte
  10 - configure as output and set to low; resp: same byte
  11 - configure as inpit and get state; resp: 0|1


v == 1 => batch or service commands

request: 0x00
    get version
response: 0x00 v1 v2


memory access commands:
  request: 0x02 <adr> <b>
      writes byte to [adr]
  response: 0x02 <adr> <b>

  request: 0x04 <adr>
      reads byte from [adr]
  response: 0x04 <adr> <b>

  request: 0x06 <adr> <n> <b1>...<bn>
      writes n bytes starting from [adr] (n <= 128)
  response: 0x06 <adr> <n> <b1>...<bn>

  request: 0x08 <adr> <n>
      reads n bytes starting from [adr] (n <= 128)
  response: 0x08 <adr> <n> <b1>...<bn>

io port access commands:
  request: 0x0a <port> <b>
      writes byte to [port]
  response: 0x0a <port> <b>

  request: 0x0c <port>
      reads byte from [port]
  response: 0x0c <port> <b>

*/

#define VAR_RAW     (0 << 0)
#define VAR_BATCH   (1 << 0)
#define VAR_MASK    (0x1)

#define CMD_VERSION  (0x00 | VAR_BATCH)
#define CMD_MEM_WR   (0x02 | VAR_BATCH)
#define CMD_MEM_RD   (0x04 | VAR_BATCH)
#define CMD_MEM_WRN  (0x06 | VAR_BATCH)
#define CMD_MEM_RDN  (0x08 | VAR_BATCH)
#define CMD_IO_WR    (0x0A | VAR_BATCH)
#define CMD_IO_RD    (0x0C | VAR_BATCH)
#define CMD_LCD_CTRL (0x0E | VAR_BATCH)

#define BATCH_MAX_SIZE (128)

#define SUBCMD_SET_HI  (1 << 1)
#define SUBCMD_SET_LO  (2 << 1)
#define SUBCMD_GET     (3 << 1)
#define SUBCMD_MASK    (0x3 << 1)

#define PIN_PA0     (0ul << 3)
#define PIN_PA1     (1ul << 3)
#define PIN_PA2     (2ul << 3)
#define PIN_PA3     (3ul << 3)
#define PIN_PA4     (4ul << 3)
#define PIN_PA5     (5ul << 3)
#define PIN_PA6     (6ul << 3)
#define PIN_PA7     (7ul << 3)
#define PIN_PA8     (8ul << 3)
#define PIN_PA9     (9ul << 3)
#define PIN_PA10    (10ul << 3)
#define PIN_PA15    (11ul << 3)
#define PIN_PB0     (12ul << 3)
#define PIN_PB1     (13ul << 3)
#define PIN_PB3     (14ul << 3)
#define PIN_PB4     (15ul << 3)
#define PIN_PB5     (16ul << 3)
#define PIN_PB6     (17ul << 3)
#define PIN_PB7     (18ul << 3)
#define PIN_PB8     (19ul << 3)
#define PIN_PB9     (20ul << 3)
#define PIN_PB10    (21ul << 3)
#define PIN_PB11    (22ul << 3)
#define PIN_PB12    (23ul << 3)
#define PIN_PB13    (24ul << 3)
#define PIN_PB14    (25ul << 3)
#define PIN_PB15    (26ul << 3)
#define PIN_PC13    (27ul << 3)
#define PIN_PC14    (28ul << 3)
#define PIN_PC15    (29ul << 3)
#define PIN_MASK    (0x1f << 3)

// extra pin definitions for usblib
#define PIN_PA13    (30 << 3)

#define BUS_A0      PIN_PA0
#define BUS_A1      PIN_PA1
#define BUS_A2      PIN_PA2
#define BUS_A3      PIN_PA3
#define BUS_A4      PIN_PA4
#define BUS_A5      PIN_PA5
#define BUS_A6      PIN_PA6
#define BUS_A7      PIN_PA7
#define BUS_A8      PIN_PA8
#define BUS_A9      PIN_PA9
#define BUS_A10     PIN_PA10
#define BUS_A11     PIN_PA15
#define BUS_A12     PIN_PB0
#define BUS_A13     PIN_PB1
#define BUS_A14     PIN_PB3
#define BUS_A15     PIN_PB4
#define BUS_D0      PIN_PB5
#define BUS_D1      PIN_PB6
#define BUS_D2      PIN_PB7
#define BUS_D3      PIN_PB8
#define BUS_D4      PIN_PB9
#define BUS_D5      PIN_PB10
#define BUS_D6      PIN_PB11
#define BUS_D7      PIN_PB12
#define BUS_MREQ    PIN_PB13
#define BUS_IORQ    PIN_PB14
#define BUS_RD      PIN_PB15
#define BUS_WR      PIN_PC13
#define BUS_RESET   PIN_PC14
#define BUS_INT     PIN_PC15

#define BOARD_VER1 0xBC
#define BOARD_VER2 0x80
