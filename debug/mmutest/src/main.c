#include <stm32f1xx.h>

#include "dbgproto.h"
#include "stm32_hw.h"
#include "printf.h"

#define TRUTH_SIZE 256
#define NUM_STABILITY_TESTS 1000

// fill 256-byte buffer with values: 0..3: bit0: RAM_CS, bit1: ROM_CS
// byte index is set of input signals bits
void acquire_truth_table(uint8_t *buf) {
  for (int i = 0; i < TRUTH_SIZE; i++) {
    if (i & (1 << 0))
      set_pin_high(PIN_PB0);
    else
      set_pin_low(PIN_PB0);

    if (i & (1 << 1))
      set_pin_high(PIN_PB1);
    else
      set_pin_low(PIN_PB1);

    if (i & (1 << 2))
      set_pin_high(PIN_PB3);
    else
      set_pin_low(PIN_PB3);

    if (i & (1 << 3))
      set_pin_high(PIN_PA7);
    else
      set_pin_low(PIN_PA7);

    if (i & (1 << 4))
      set_pin_high(PIN_PB5);
    else
      set_pin_low(PIN_PB5);

    if (i & (1 << 5))
      set_pin_high(PIN_PB6);
    else
      set_pin_low(PIN_PB6);

    if (i & (1 << 6))
      set_pin_high(PIN_PB7);
    else
      set_pin_low(PIN_PB7);

    if (i & (1 << 7))
      set_pin_high(PIN_PB8);
    else
      set_pin_low(PIN_PB8);

    delay_us(10);

    buf[i] = 0;

    // RAM_CS
    if (is_pin_high(PIN_PB9))
      buf[i] |= 1;

    // ROM_CS
    if (is_pin_high(PIN_PB10))
      buf[i] |= 2;
  }
}

#define MASK_HIMEM_ON (1 << 0)
#define MASK_A13 (1 << 1)
#define MASK_A14 (1 << 2)
#define MASK_A15 (1 << 3)
#define MASK_WR (1 << 4)
#define MASK_RD (1 << 5)
#define MASK_MREQ (1 << 6)
#define MASK_ROM_EN (1 << 7)

void show_truth_table(uint8_t *buf) {
  printf("N   | a15 | a14 | a13 | mreq | rd | wr | rom_en | himem_on  ||  rom_cs   || ram_cs\n\r");

  for (int i = 0; i < TRUTH_SIZE; i++) {
    printf("%03d | %d  | %d   | %d   |  %d   | %d  | %d  |   %d    |    %d      ||    %d      ||   %d\n\r",
      i,
      !!(i & MASK_A15),
      !!(i & MASK_A14),
      !!(i & MASK_A13),
      !!(i & MASK_MREQ),
      !!(i & MASK_RD),
      !!(i & MASK_WR),
      !!(i & MASK_ROM_EN),
      !!(i & MASK_HIMEM_ON),
      !!(buf[i] & 2),
      !!(buf[i] & 1));
  }
}

void synth_truth_table(uint8_t *buf) {
  for (int i = 0; i < TRUTH_SIZE; i++) {
    bool himem_on = i & MASK_HIMEM_ON;
    bool a15 = i & MASK_A15;
    bool a14 = i & MASK_A14;
    bool a13 = i & MASK_A13;
    bool wr = i & MASK_WR;
    bool rd = i & MASK_RD;
    bool mreq = i & MASK_MREQ;
    bool rom_en = i & MASK_ROM_EN;

    buf[i] = 0;

    // RAM_CS
    if ((((rd && !a15) && wr) || ((!wr && (!a15 && !rd)) || ((!a15 && !rd) && !rom_en))) || (mreq || ((a15 && a14) && (a13 && himem_on))))
      buf[i] |= 1;

    // ROM_CS
    if ((a15 || mreq || rd || rom_en) || (!wr))
      buf[i] |= 2;
  }
}

int my_memcmp(uint8_t *b1, uint8_t *b2, int len) {
  int result = 0;
  for (int i = 0; i < len; i++) {
    if (b1[i] != b2[i])
      result++;
  }

  return result;
}

void _putchar(char ch) {
  while ((USART1->SR & USART_SR_TXE) == 0)
    ;

  USART1->DR = (uint32_t)ch;
}

int main() {
  uint8_t truth[2 * TRUTH_SIZE];
  int errtestcnt = 0, errtotalcnt = 0;

  cfg_pin_as_output(PIN_PB0);   // HIMEM_ON
  cfg_pin_as_output(PIN_PB1);   // A13
  cfg_pin_as_output(PIN_PB3);   // A14
  cfg_pin_as_output(PIN_PA7);   // A15
  cfg_pin_as_output(PIN_PB5);   // WR
  cfg_pin_as_output(PIN_PB6);   // RD
  cfg_pin_as_output(PIN_PB7);   // MREQ
  cfg_pin_as_output(PIN_PB8);   // ROM_EN

  cfg_pin_as_input(PIN_PB9);    // RAM_CS
  cfg_pin_as_input(PIN_PB10);    // ROM_CS


  // set_pin_low(PIN_PA7); // A15
  // set_pin_low(PIN_PB7); // MREQ
  // set_pin_low(PIN_PB6); // RD
  // set_pin_low(PIN_PB8); // ROMEN
  // set_pin_high(PIN_PB5);  // WR

  // while(1);

  // configure UART1, transmit only. PA9: TXD
  GPIOA->CRH &= (~GPIO_CRH_CNF9_0);
  GPIOA->CRH |= (GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9);
  USART1->CR1 = USART_CR1_UE;

  USART1->BRR = 156;

  USART1->CR1 |= USART_CR1_TE;
  USART1->CR2 = 0;
  USART1->CR3 = 0;

  printf("====================\n\r"
         "Hello from MMU test!\n\r");

  // while (1) {
  //   set_pin_high(PIN_PA7); // A15
  //   set_pin_low(PIN_PB3); // A14
  //   set_pin_high(PIN_PB1); // A13
  //   set_pin_low(PIN_PB7); // MREQ
  //   set_pin_low(PIN_PB6); // RD
  //   set_pin_high(PIN_PB5);  // WR
  //   set_pin_low(PIN_PB8); // ROMEN
  //   set_pin_low(PIN_PB0); // HIMEM

  //   delay_us(50);

  //   printf("RAM_CS: %d\n\r", is_pin_high(PIN_PB9));
  // }

  // 1. acquire truth table and compare it with synthesized one
  synth_truth_table(truth);
  acquire_truth_table(&truth[TRUTH_SIZE]);

  if (my_memcmp(truth, &truth[TRUTH_SIZE], TRUTH_SIZE) != 0) {
    printf("acquired truth table is differs from synthesized one:\n\rACQUIRED:\n\r");
    show_truth_table(&truth[TRUTH_SIZE]);
    printf("SYNTHESIZED:\n\r");
    show_truth_table(truth);
  } else {
    printf("acquired truth table is ok!\n\r");
  }

  // 2. stability check: acquired truth table must not change in time
  for (int i = 0; i < NUM_STABILITY_TESTS; i++) {
    delay_us(50);
    acquire_truth_table(truth);
    delay_us(50);
    acquire_truth_table(&truth[TRUTH_SIZE]);

    int errcnt = my_memcmp(truth, &truth[TRUTH_SIZE], TRUTH_SIZE);
    if (errcnt != 0) {
      errtestcnt++;
      errtotalcnt += errcnt;
    }
  }

  printf("Stability check: %d/%d tests failed, %d errors total\n\r\n\r",
    errtestcnt, NUM_STABILITY_TESTS, errtotalcnt);

  return 0;
}
