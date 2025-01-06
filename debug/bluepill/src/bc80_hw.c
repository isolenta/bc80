#include <stdbool.h>
#include <stdint.h>
#include <stm32f1xx.h>

#include "dbgproto.h"
#include "stm32_hw.h"

static void __attribute__((noinline)) delay_1us() {
  for (volatile int c = 0; c < 72; c++) {
    __asm volatile("nop");
  }
}

static inline void cfg_address_pins() {
  cfg_pin_as_output(BUS_A0);
  cfg_pin_as_output(BUS_A1);
  cfg_pin_as_output(BUS_A2);
  cfg_pin_as_output(BUS_A3);
  cfg_pin_as_output(BUS_A4);
  cfg_pin_as_output(BUS_A5);
  cfg_pin_as_output(BUS_A6);
  cfg_pin_as_output(BUS_A7);
  cfg_pin_as_output(BUS_A8);
  cfg_pin_as_output(BUS_A9);
  cfg_pin_as_output(BUS_A10);
  cfg_pin_as_output(BUS_A11);
  cfg_pin_as_output(BUS_A12);
  cfg_pin_as_output(BUS_A13);
  cfg_pin_as_output(BUS_A14);
  cfg_pin_as_output(BUS_A15);
}

static inline void cfg_data_pins(bool output) {
  if (output) {
    cfg_pin_as_output(BUS_D0);
    cfg_pin_as_output(BUS_D1);
    cfg_pin_as_output(BUS_D2);
    cfg_pin_as_output(BUS_D3);
    cfg_pin_as_output(BUS_D4);
    cfg_pin_as_output(BUS_D5);
    cfg_pin_as_output(BUS_D6);
    cfg_pin_as_output(BUS_D7);
  } else {
    cfg_pin_as_input(BUS_D0);
    cfg_pin_as_input(BUS_D1);
    cfg_pin_as_input(BUS_D2);
    cfg_pin_as_input(BUS_D3);
    cfg_pin_as_input(BUS_D4);
    cfg_pin_as_input(BUS_D5);
    cfg_pin_as_input(BUS_D6);
    cfg_pin_as_input(BUS_D7);
  }
}

static inline void cfg_bus_control_pins() {
  cfg_pin_as_output(BUS_MREQ);
  cfg_pin_as_output(BUS_IORQ);
  cfg_pin_as_output(BUS_WR);
  cfg_pin_as_output(BUS_RD);
}

static inline void cfg_release_bus() {
  // don't drive CPU bus to prevent conflicts with Z80: set these pins to hi-z state
  cfg_pin_as_input(BUS_A0);
  cfg_pin_as_input(BUS_A1);
  cfg_pin_as_input(BUS_A2);
  cfg_pin_as_input(BUS_A3);
  cfg_pin_as_input(BUS_A4);
  cfg_pin_as_input(BUS_A5);
  cfg_pin_as_input(BUS_A6);
  cfg_pin_as_input(BUS_A7);
  cfg_pin_as_input(BUS_A8);
  cfg_pin_as_input(BUS_A9);
  cfg_pin_as_input(BUS_A10);
  cfg_pin_as_input(BUS_A11);
  cfg_pin_as_input(BUS_A12);
  cfg_pin_as_input(BUS_A13);
  cfg_pin_as_input(BUS_A14);
  cfg_pin_as_input(BUS_A15);

  cfg_pin_as_input(BUS_D0);
  cfg_pin_as_input(BUS_D1);
  cfg_pin_as_input(BUS_D2);
  cfg_pin_as_input(BUS_D3);
  cfg_pin_as_input(BUS_D4);
  cfg_pin_as_input(BUS_D5);
  cfg_pin_as_input(BUS_D6);
  cfg_pin_as_input(BUS_D7);

  // don't drive bus control signals as well except RESET: we'll use it to lock/unlock CPU
  cfg_pin_as_input(BUS_MREQ);
  cfg_pin_as_input(BUS_IORQ);
  cfg_pin_as_input(BUS_WR);
  cfg_pin_as_input(BUS_RD);
  cfg_pin_as_input(BUS_INT);
}

static inline uint8_t read_data() {
  uint8_t result = 0;

  if (is_pin_high(BUS_D0)) result |= (1 << 0);
  if (is_pin_high(BUS_D1)) result |= (1 << 1);
  if (is_pin_high(BUS_D2)) result |= (1 << 2);
  if (is_pin_high(BUS_D3)) result |= (1 << 3);
  if (is_pin_high(BUS_D4)) result |= (1 << 4);
  if (is_pin_high(BUS_D5)) result |= (1 << 5);
  if (is_pin_high(BUS_D6)) result |= (1 << 6);
  if (is_pin_high(BUS_D7)) result |= (1 << 7);

  return result;
}

static inline void set_address(uint16_t addr) {
  set_pin_low(BUS_A0);
  set_pin_low(BUS_A1);
  set_pin_low(BUS_A2);
  set_pin_low(BUS_A3);
  set_pin_low(BUS_A4);
  set_pin_low(BUS_A5);
  set_pin_low(BUS_A6);
  set_pin_low(BUS_A7);
  set_pin_low(BUS_A8);
  set_pin_low(BUS_A9);
  set_pin_low(BUS_A10);
  set_pin_low(BUS_A11);
  set_pin_low(BUS_A12);
  set_pin_low(BUS_A13);
  set_pin_low(BUS_A14);
  set_pin_low(BUS_A15);

  if (addr & (1 << 0)) set_pin_high(BUS_A0);
  if (addr & (1 << 1)) set_pin_high(BUS_A1);
  if (addr & (1 << 2)) set_pin_high(BUS_A2);
  if (addr & (1 << 3)) set_pin_high(BUS_A3);
  if (addr & (1 << 4)) set_pin_high(BUS_A4);
  if (addr & (1 << 5)) set_pin_high(BUS_A5);
  if (addr & (1 << 6)) set_pin_high(BUS_A6);
  if (addr & (1 << 7)) set_pin_high(BUS_A7);
  if (addr & (1 << 8)) set_pin_high(BUS_A8);
  if (addr & (1 << 9)) set_pin_high(BUS_A9);
  if (addr & (1 << 10)) set_pin_high(BUS_A10);
  if (addr & (1 << 11)) set_pin_high(BUS_A11);
  if (addr & (1 << 12)) set_pin_high(BUS_A12);
  if (addr & (1 << 13)) set_pin_high(BUS_A13);
  if (addr & (1 << 14)) set_pin_high(BUS_A14);
  if (addr & (1 << 15)) set_pin_high(BUS_A15);
}

static inline void set_data(uint8_t byte) {
  set_pin_low(BUS_D0);
  set_pin_low(BUS_D1);
  set_pin_low(BUS_D2);
  set_pin_low(BUS_D3);
  set_pin_low(BUS_D4);
  set_pin_low(BUS_D5);
  set_pin_low(BUS_D6);
  set_pin_low(BUS_D7);

  if (byte & (1 << 0)) set_pin_high(BUS_D0);
  if (byte & (1 << 1)) set_pin_high(BUS_D1);
  if (byte & (1 << 2)) set_pin_high(BUS_D2);
  if (byte & (1 << 3)) set_pin_high(BUS_D3);
  if (byte & (1 << 4)) set_pin_high(BUS_D4);
  if (byte & (1 << 5)) set_pin_high(BUS_D5);
  if (byte & (1 << 6)) set_pin_high(BUS_D6);
  if (byte & (1 << 7)) set_pin_high(BUS_D7);
}

void bc80_write_mem(uint16_t addr, uint8_t byte) {
  cfg_address_pins();
  cfg_bus_control_pins();
  cfg_data_pins(true);

  set_address(addr);
  set_data(byte);

  set_pin_high(BUS_IORQ);
  set_pin_high(BUS_RD);

  set_pin_low(BUS_WR);
  set_pin_low(BUS_MREQ);

  delay_1us();

  set_pin_high(BUS_MREQ);
  set_pin_high(BUS_WR);

  cfg_release_bus();
}

uint8_t bc80_read_mem(uint16_t addr) {
  uint8_t result;

  cfg_address_pins();
  cfg_bus_control_pins();
  cfg_data_pins(false);

  set_address(addr);

  set_pin_high(BUS_IORQ);
  set_pin_high(BUS_WR);

  set_pin_low(BUS_RD);
  set_pin_low(BUS_MREQ);

  delay_1us();

  result = read_data();

  set_pin_high(BUS_MREQ);
  set_pin_high(BUS_RD);

  cfg_release_bus();

  return result;
}

void bc80_write_io(uint16_t addr, uint8_t byte) {
  cfg_address_pins();
  cfg_bus_control_pins();
  cfg_data_pins(true);

  set_address(addr);
  set_data(byte);

  set_pin_high(BUS_MREQ);
  set_pin_high(BUS_RD);

  set_pin_low(BUS_IORQ);
  set_pin_low(BUS_WR);

  delay_1us();

  set_pin_high(BUS_WR);
  set_pin_high(BUS_IORQ);

  cfg_release_bus();
}

uint8_t bc80_read_io(uint16_t addr) {
  uint8_t result;

  cfg_address_pins();
  cfg_bus_control_pins();
  cfg_data_pins(false);

  set_address(addr);

  set_pin_high(BUS_MREQ);
  set_pin_high(BUS_WR);

  set_pin_low(BUS_RD);
  set_pin_low(BUS_IORQ);

  delay_1us();

  result = read_data();

  set_pin_high(BUS_IORQ);
  set_pin_high(BUS_RD);

  cfg_release_bus();

  return result;
}

void bc80_lcd_ctrl(uint8_t is_data, uint8_t val) {
  // 1602 pins to 8255 mapping:
  //  RS: PC2
  //  EN: PC3
  //  D4: PC4
  //  D5: PC5
  //  D6: PC6
  //  D7: PC7

  // function expected that PortC is configured as output

#define CTL_BIT_8255(bitnum, set) ((bitnum << 1) | (set & 1))
  // data bus to 0
  bc80_write_io(0x07, CTL_BIT_8255(4, 0));
  bc80_write_io(0x07, CTL_BIT_8255(5, 0));
  bc80_write_io(0x07, CTL_BIT_8255(6, 0));
  bc80_write_io(0x07, CTL_BIT_8255(7, 0));

  // reset RS, EN
  bc80_write_io(0x07, CTL_BIT_8255(2, 0));
  bc80_write_io(0x07, CTL_BIT_8255(3, 0));

  // for DATA set RS
  if (is_data)
    bc80_write_io(0x07, CTL_BIT_8255(2, 1));

  // high half-byte
  if (val & 0x10)
    bc80_write_io(0x07, CTL_BIT_8255(4, 1));
  if (val & 0x20)
    bc80_write_io(0x07, CTL_BIT_8255(5, 1));
  if (val & 0x40)
    bc80_write_io(0x07, CTL_BIT_8255(6, 1));
  if (val & 0x80)
    bc80_write_io(0x07, CTL_BIT_8255(7, 1));

  // set EN
  bc80_write_io(0x07, CTL_BIT_8255(3, 1));

  delay_us(2);

  // reset EN
  bc80_write_io(0x07, CTL_BIT_8255(3, 0));

  delay_us(200);

  // data bus to 0
  bc80_write_io(0x07, CTL_BIT_8255(4, 0));
  bc80_write_io(0x07, CTL_BIT_8255(5, 0));
  bc80_write_io(0x07, CTL_BIT_8255(6, 0));
  bc80_write_io(0x07, CTL_BIT_8255(7, 0));

  // low half-byte
  if (val & 0x1)
    bc80_write_io(0x07, CTL_BIT_8255(4, 1));
  if (val & 0x2)
    bc80_write_io(0x07, CTL_BIT_8255(5, 1));
  if (val & 0x4)
    bc80_write_io(0x07, CTL_BIT_8255(6, 1));
  if (val & 0x8)
    bc80_write_io(0x07, CTL_BIT_8255(7, 1));

  // set EN
  bc80_write_io(0x07, CTL_BIT_8255(3, 1));

  delay_us(2);

  // reset EN
  bc80_write_io(0x07, CTL_BIT_8255(3, 0));

  delay_us(2000);
}
