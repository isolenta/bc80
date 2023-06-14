#include <stm32f1xx.h>

#include "bc80_hw.h"
#include "dbgproto.h"
#include "stm32_hw.h"
#include "usb.h"
#include "usbhw.h"

int main() {
  USB_setup();

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

  cfg_pin_as_output(BUS_D0);
  cfg_pin_as_output(BUS_D1);
  cfg_pin_as_output(BUS_D2);
  cfg_pin_as_output(BUS_D3);
  cfg_pin_as_output(BUS_D4);
  cfg_pin_as_output(BUS_D5);
  cfg_pin_as_output(BUS_D6);
  cfg_pin_as_output(BUS_D7);

  cfg_pin_as_output(BUS_MREQ);
  cfg_pin_as_output(BUS_IORQ);
  cfg_pin_as_output(BUS_WR);
  cfg_pin_as_output(BUS_RD);
  cfg_pin_as_output(BUS_RESET);
  cfg_pin_as_output(BUS_INT);

  set_pin_high(BUS_MREQ);
  set_pin_high(BUS_IORQ);
  set_pin_high(BUS_WR);
  set_pin_high(BUS_RD);
  set_pin_high(BUS_RESET);
  set_pin_high(BUS_INT);

  while (1) {
    char recvbuf[256];
    uint8_t nbytes;

    usb_proc();

    nbytes = usb_recv(recvbuf);
    if (nbytes == 0) continue;

    for (int i = 0; i < nbytes; i++) {
      uint8_t cmd = recvbuf[i];

      if ((cmd & VAR_MASK) == VAR_BATCH) {
        // TODO: sanity checks for buffer overrun for multibyte commands

        if (cmd == CMD_VERSION) {
          char resp[] = {CMD_VERSION | VAR_BATCH, BOARD_VER1, BOARD_VER2};
          usb_send(resp, sizeof(resp));
        } else if (cmd == CMD_MEM_WR) {
          uint16_t addr = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t byte = recvbuf[i + 3];

          bc80_write_mem(addr, byte);

          usb_send(recvbuf + i, 4);

          i += 3;
        } else if (cmd == CMD_MEM_RD) {
          uint16_t addr = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t byte = bc80_read_mem(addr);

          usb_send(recvbuf + i, 3);
          usb_send((char *)&byte, 1);

          i += 2;
        } else if (cmd == CMD_MEM_WRN) {
          uint16_t addr = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t nbytes = recvbuf[i + 3];

          for (int j = 0; j < nbytes; j++)
            bc80_write_mem(addr + j, recvbuf[i + 3 + j]);

          usb_send(recvbuf + i, 4 + nbytes);
          i += 3 + nbytes;
        } else if (cmd == CMD_MEM_RDN) {
          uint16_t addr = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t nbytes = recvbuf[i + 3];

          usb_send(recvbuf + i, 4);

          for (int j = 0; j < nbytes; j++) {
            uint8_t byte = bc80_read_mem(addr + j);
            usb_send((char *)&byte, 1);
          }

          i += 3;
        } else if (cmd == CMD_IO_RD) {
          uint16_t port = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t byte = bc80_read_io(port);

          usb_send(recvbuf + i, 3);
          usb_send((char *)&byte, 1);

          i += 2;
        } else if (cmd == CMD_IO_WR) {
          uint16_t port = (((uint16_t)recvbuf[i + 1]) << 8) | recvbuf[i + 2];
          uint8_t byte = recvbuf[i + 3];

          bc80_write_io(port, byte);

          usb_send(recvbuf + i, 4);

          i += 3;
        } else if (cmd == CMD_LCD_CTRL) {
          uint8_t is_data = recvbuf[i + 1];
          uint8_t value = recvbuf[i + 2];

          bc80_lcd_ctrl(is_data, value);

          usb_send(recvbuf + i, 3);
          i += 2;
        } else {
          // for unimplemented commands just echo them
          usb_send(recvbuf + i, 1);
        }
      } else {
        // raw pin access
        uint8_t subcmd = cmd & SUBCMD_MASK;
        int pin = cmd & PIN_MASK;

        if (subcmd == SUBCMD_SET_HI) {
          cfg_pin_as_output(pin);
          set_pin_high(pin);
          usb_send(recvbuf + i, 1);
        } else if (subcmd == SUBCMD_SET_LO) {
          cfg_pin_as_output(pin);
          set_pin_low(pin);
          usb_send(recvbuf + i, 1);
        } else if (subcmd == SUBCMD_GET) {
          uint8_t resp;

          cfg_pin_as_input(pin);
          resp = is_pin_high(pin);
          usb_send(recvbuf + i, 1);
          usb_send((char *)&resp, 1);
        } else {
          // for unimplemented subcommand just echo
          usb_send(recvbuf + i, 1);
        }
      }    // !VAR_BATCH
    }      // for (nbytes)
  }        // while(1)

  return 0;
}
