#include "stm32_hw.h"

#include <stm32f1xx.h>

#include "dbgproto.h"

static GPIO_TypeDef *get_pinnumport(int pin, uint32_t *pinnum) {
  GPIO_TypeDef *pinport = GPIOA;

  switch (pin) {
    case PIN_PA0:
      *pinnum = 0;
      pinport = GPIOA;
      break;
    case PIN_PA1:
      *pinnum = 1;
      pinport = GPIOA;
      break;
    case PIN_PA2:
      *pinnum = 2;
      pinport = GPIOA;
      break;
    case PIN_PA3:
      *pinnum = 3;
      pinport = GPIOA;
      break;
    case PIN_PA4:
      *pinnum = 4;
      pinport = GPIOA;
      break;
    case PIN_PA5:
      *pinnum = 5;
      pinport = GPIOA;
      break;
    case PIN_PA6:
      *pinnum = 6;
      pinport = GPIOA;
      break;
    case PIN_PA7:
      *pinnum = 7;
      pinport = GPIOA;
      break;
    case PIN_PA8:
      *pinnum = 8;
      pinport = GPIOA;
      break;
    case PIN_PA9:
      *pinnum = 9;
      pinport = GPIOA;
      break;
    case PIN_PA10:
      *pinnum = 10;
      pinport = GPIOA;
      break;
    case PIN_PA13:
      *pinnum = 13;
      pinport = GPIOA;
      break;
    case PIN_PA15:
      *pinnum = 15;
      pinport = GPIOA;
      break;
    case PIN_PB0:
      *pinnum = 0;
      pinport = GPIOB;
      break;
    case PIN_PB1:
      *pinnum = 1;
      pinport = GPIOB;
      break;
    case PIN_PB3:
      *pinnum = 3;
      pinport = GPIOB;
      break;
    case PIN_PB4:
      *pinnum = 4;
      pinport = GPIOB;
      break;
    case PIN_PB5:
      *pinnum = 5;
      pinport = GPIOB;
      break;
    case PIN_PB6:
      *pinnum = 6;
      pinport = GPIOB;
      break;
    case PIN_PB7:
      *pinnum = 7;
      pinport = GPIOB;
      break;
    case PIN_PB8:
      *pinnum = 8;
      pinport = GPIOB;
      break;
    case PIN_PB9:
      *pinnum = 9;
      pinport = GPIOB;
      break;
    case PIN_PB10:
      *pinnum = 10;
      pinport = GPIOB;
      break;
    case PIN_PB11:
      *pinnum = 11;
      pinport = GPIOB;
      break;
    case PIN_PB12:
      *pinnum = 12;
      pinport = GPIOB;
      break;
    case PIN_PB13:
      *pinnum = 13;
      pinport = GPIOB;
      break;
    case PIN_PB14:
      *pinnum = 14;
      pinport = GPIOB;
      break;
    case PIN_PB15:
      *pinnum = 15;
      pinport = GPIOB;
      break;
    case PIN_PC13:
      *pinnum = 13;
      pinport = GPIOC;
      break;
    case PIN_PC14:
      *pinnum = 14;
      pinport = GPIOC;
      break;
    case PIN_PC15:
      *pinnum = 15;
      pinport = GPIOC;
      break;
    default:
      break;
  }

  return pinport;
}

void cfg_pin_as_output(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  if (pinnum < 8) {
    port->CRL &= (uint32_t)~(0xf << (pinnum << 2));
    port->CRL |= (CNF_PPOUTPUT | MODE_NORMAL) << (pinnum << 2);
  } else {
    port->CRH &= (uint32_t)~(0xf << ((pinnum - 8) << 2));
    port->CRH |= (CNF_PPOUTPUT | MODE_NORMAL) << ((pinnum - 8) << 2);
  }
}

void cfg_pin_as_input(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  if (pinnum < 8) {
    port->CRL &= (uint32_t)~(0xf << (pinnum << 2));
    port->CRL |= (CNF_FLINPUT | MODE_INPUT) << (pinnum << 2);
  } else {
    port->CRH &= (uint32_t)~(0xf << ((pinnum - 8) << 2));
    port->CRH |= (CNF_FLINPUT | MODE_INPUT) << ((pinnum - 8) << 2);
  }
}

void set_pin_low(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  port->BSRR = 1 << (pinnum + 16);
}

void set_pin_high(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  port->BSRR = 1 << pinnum;
}

bool is_pin_high(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  return port->IDR & (1 << pinnum);
}

bool is_pin_input(int pin) {
  uint32_t pinnum;
  GPIO_TypeDef *port = get_pinnumport(pin, &pinnum);

  if (pinnum < 8)
    return (port->CRL & (0x3 << (pinnum << 2))) == 0;
  else
    return (port->CRH & (0x3 << ((pinnum - 8) << 2))) == 0;
}
