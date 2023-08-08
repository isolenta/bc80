#include <stm32f1xx.h>

static void sys_clock_init() {
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;

  // Set HSION bit
  RCC->CR |= (uint32_t)RCC_CR_HSION;

  // Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits
  RCC->CFGR &= (uint32_t)0xF8FF0000;

  // Reset HSEON, CSSON and PLLON bits
  RCC->CR &= (uint32_t)0xFEF6FFFF;

  // Reset HSEBYP bit
  RCC->CR &= (uint32_t)0xFFFBFFFF;

  // Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits
  RCC->CFGR &= (uint32_t)0xFF80FFFF;

  // Disable all interrupts and clear pending bits
  RCC->CIR = 0x009F0000;

  // Vector Table Relocation in Internal FLASH
  SCB->VTOR = FLASH_BASE;

  // Enable HSE
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);

  // Wait till HSE is ready
  while ((RCC->CR & RCC_CR_HSERDY) == 0)
    ;

  // Enable Prefetch Buffer
  FLASH->ACR |= FLASH_ACR_PRFTBE;

  // Flash 2 wait state
  FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
  FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;

  // Disable PLL
  RCC->CR &= (uint32_t)((uint32_t)~RCC_CR_PLLON);

  // Set PREDIV1 Value to DIV1
  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_PLLXTPRE);

  // PLL configuration: PLLCLK = HSE * 9 = 72 MHz
  RCC->CFGR &= (uint32_t)((uint32_t) ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL));
  RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9);

  // Enable PLL
  RCC->CR |= RCC_CR_PLLON;

  // Wait till PLL is ready
  while ((RCC->CR & RCC_CR_PLLRDY) == 0)
    ;

  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_PPRE1);
  RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV16;

  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_PPRE2);
  RCC->CFGR |= (uint32_t)(RCC_CFGR_PPRE1_DIV16 << 3);

  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_HPRE);
  RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

  // SYSCLK is PLLCLK
  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_SW);
  RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

  // Wait till PLL is used as system clock source
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    ;

  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_PPRE1);
  RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

  RCC->CFGR &= (uint32_t)((uint32_t)~RCC_CFGR_PPRE2);
  RCC->CFGR |= (uint32_t)(RCC_CFGR_PPRE1_DIV1 << 3);
}

static void gpio_clock_init() {
  volatile uint32_t tmp;

  // Set APB2 clock to 72/4=18MHz
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;

  // Enable clocks to the GPIO subsystems, turn on AFIO clocking to disable
  // SWD/JTAG
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  tmp = RCC->APB2ENR;

  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
  tmp = RCC->APB2ENR;

  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
  tmp = RCC->APB2ENR;

  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  tmp = RCC->APB2ENR;

  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

  RCC->APB1ENR |= RCC_APB1ENR_PWREN;

  (void)tmp;

  // Serial wire JTAG configuration: JTAG-DP Disabled and SW-DP Disabled
  // AFIO->MAPR &= 0x07000000;
  // AFIO->MAPR |= 0x04000000;
}

void SystemInit(void) {
  sys_clock_init();
  gpio_clock_init();

  /* Disable TRC */
  CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk; // ~0x01000000;
  /* Enable TRC */
  CoreDebug->DEMCR |=  CoreDebug_DEMCR_TRCENA_Msk; // 0x01000000;

  /* Disable clock cycle counter */
  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk; //~0x00000001;
  /* Enable  clock cycle counter */
  DWT->CTRL |=  DWT_CTRL_CYCCNTENA_Msk; //0x00000001;

  /* Reset the clock cycle counter value */
  DWT->CYCCNT = 0;

  /* 3 NO OPERATION instructions */
  __ASM volatile ("NOP");
  __ASM volatile ("NOP");
  __ASM volatile ("NOP");
}
