/*
 * This file is part of the MLX90640 project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "usbhw.h"

#include "usb.h"
#include "usb_lib.h"

void USB_setup() {
  // set PA13 as output push-pull, normal speed
  GPIOA->CRH = 1 << 20;

  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
  USBPU_OFF();
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  RCC->APB2ENR |= USB_RCC;

  // Force USB Reset
  USB->CNTR = USB_CNTR_FRES;

  for (uint32_t ctr = 0; ctr < 72000; ++ctr) nop();  // wait >1ms

  USB->CNTR = 0;
  USB->BTABLE = 0;
  USB->DADDR = 0;
  USB->ISTR = 0;

  // allow only wakeup & reset interrupts
  USB->CNTR = USB_CNTR_RESETM | USB_CNTR_WKUPM;

  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  USBPU_ON();
}

static uint16_t lastaddr = LASTADDR_DEFAULT;

/**
 * Endpoint initialisation
 * @param number - EP num (0...7)
 * @param type - EP type (EP_TYPE_BULK, EP_TYPE_CONTROL, EP_TYPE_ISO,
 * EP_TYPE_INTERRUPT)
 * @param txsz - transmission buffer size @ USB/CAN buffer
 * @param rxsz - reception buffer size @ USB/CAN buffer
 * @param uint16_t (*func)(ep_t *ep) - EP handler function
 * @return 0 if all OK
 */
int EP_Init(uint8_t number, uint8_t type, uint16_t txsz, uint16_t rxsz,
            void (*func)(ep_t ep)) {
  if (number >= STM32ENDPOINTS) {
    // out of configured amount
    return 4;
  }

  if (txsz > USB_BTABLE_SIZE || rxsz > USB_BTABLE_SIZE) {
    // buffer too large
    return 1;
  }

  if (lastaddr + txsz + rxsz >= USB_BTABLE_SIZE) {
    // out of btable
    return 2;
  }

  if (rxsz & 1 || rxsz > 512) {
    // wrong rx buffer size
    return 3;
  }

  USB->EPnR[number] = (type << 9) | (number & USB_EPnR_EA);
  USB->EPnR[number] ^= USB_EPnR_STAT_RX | USB_EPnR_STAT_TX_1;

  uint16_t countrx = 0;
  if (rxsz < 64) {
    countrx = rxsz / 2;
  } else {
    if (rxsz & 0x1f) {
      // should be multiple of 32
      return 3;
    }
    countrx = 31 + rxsz / 32;
  }

  USB_BTABLE->EP[number].USB_ADDR_TX = lastaddr;
  endpoints[number].tx_buf = (uint16_t *)(USB_PMAADDR + lastaddr * 2);
  endpoints[number].txbufsz = txsz;
  lastaddr += txsz;
  USB_BTABLE->EP[number].USB_COUNT_TX = 0;
  USB_BTABLE->EP[number].USB_ADDR_RX = lastaddr;
  endpoints[number].rx_buf = (uint16_t *)(USB_PMAADDR + lastaddr * 2);
  lastaddr += rxsz;
  USB_BTABLE->EP[number].USB_COUNT_RX = countrx << 10;
  endpoints[number].func = func;

  return 0;
}

// standard IRQ handler
void USB_LP_CAN1_RX0_IRQHandler() {
  if (USB->ISTR & USB_ISTR_RESET) {
    usbON = 0;
    // Reinit registers
    USB->CNTR =
        USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
    USB->ISTR = 0;
    // Endpoint 0 - CONTROL
    // ON USB LS size of EP0 may be 8 bytes, but on FS it should be 64 bytes!
    lastaddr = LASTADDR_DEFAULT;
    // clear address, leave only enable bit
    USB->DADDR = USB_DADDR_EF;
    USB_Dev.USB_Status = USB_STATE_DEFAULT;
    USB->ISTR = ~USB_ISTR_RESET;
    if (EP_Init(0, EP_TYPE_CONTROL, USB_EP0_BUFSZ, USB_EP0_BUFSZ,
                EP0_Handler)) {
      return;
    }
  }

  if (USB->ISTR & USB_ISTR_CTR) {
    uint8_t n = USB->ISTR & USB_ISTR_EPID;
    uint16_t epstatus = USB->EPnR[n];

    // copy received bytes amount. Low 10 bits is counter
    endpoints[n].rx_cnt = USB_BTABLE->EP[n].USB_COUNT_RX & 0x3FF;

    // check direction
    if (USB->ISTR & USB_ISTR_DIR) {
      // OUT interrupt - receive data, CTR_RX==1 (if CTR_TX == 1 - two pending
      // transactions: receive following by transmit)
      if (n == 0) {
        // control endpoint
        if (epstatus & USB_EPnR_SETUP) {
          // setup packet -> copy data to conf_pack
          EP_Read(0, (uint16_t *)&setup_packet);
          ep0dbuflen = 0;
          // interrupt handler will be called later
        } else if (epstatus & USB_EPnR_CTR_RX) {
          // data packet -> push received data to ep0databuf
          ep0dbuflen = endpoints[0].rx_cnt;
          EP_Read(0, (uint16_t *)&ep0databuf);
        }
      }
    }

    // call EP handler
    if (endpoints[n].func) endpoints[n].func(endpoints[n]);
  }

  // suspend -> still no connection, may sleep
  if (USB->ISTR & USB_ISTR_SUSP) {
    usbON = 0;
    USB->CNTR |= USB_CNTR_FSUSP | USB_CNTR_LP_MODE;
    USB->ISTR = ~USB_ISTR_SUSP;
  }

  // wakeup
  if (USB->ISTR & USB_ISTR_WKUP) {
    // clear suspend flags
    USB->CNTR &= ~(USB_CNTR_FSUSP | USB_CNTR_LP_MODE);
    USB->ISTR = ~USB_ISTR_WKUP;
  }
}
