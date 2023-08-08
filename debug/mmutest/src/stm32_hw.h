#pragma once

#include <stdbool.h>

#include "dbgproto.h"

#define MODE_INPUT 0
#define MODE_NORMAL 1  // 10MHz
#define MODE_SLOW 2    // 2MHz
#define MODE_FAST 3    // 50MHz

#define CNF_ANALOG (0 << 2)
#define CNF_PPOUTPUT (0 << 2)
#define CNF_FLINPUT (1 << 2)
#define CNF_ODOUTPUT (1 << 2)
#define CNF_PUDINPUT (2 << 2)
#define CNF_AFPP (2 << 2)
#define CNF_AFOD (3 << 2)

void sys_clock_init();
void gpio_clock_init();

void cfg_pin_as_output(int pin);
void cfg_pin_as_input(int pin);
void set_pin_low(int pin);
void set_pin_high(int pin);
bool is_pin_high(int pin);
bool is_pin_input(int pin);

void delay_us(int n);
