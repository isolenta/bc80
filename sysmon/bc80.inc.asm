; bc80 board definitions
  TIMER_CTL   equ 0x33
  TIMER_CNT1  equ 0x31
  UART_CMD    equ 0x41
  UART_DATA   equ 0x40
  KBD_DATA    equ 0x60
  PIC_CTL0    equ 0x50
  PIC_CTL1    equ 0x51

  STACK_TOP   equ 0xffff

  ISRVEC_BASE EQU 0x20
