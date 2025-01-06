; uart init sequence:
;   internal reset
;   mode: 1 stop bit, no parity, 8 bit word, x1 clock divider
;   command: transmit enable, receive enable, error reset
uart_init_seq:      defb          0, 0, 0, 0x40, 0x4d, 0x37
uart_init_end:

uart_init:
  ; switch off timer counter 2
  ld a, 0x90
  out (TIMER_CTL), a

  ; set timer control word
  ld a, 0x56  ; select counter 1, load LSB, mode #3 (square wave generator)
  out (TIMER_CTL), a

  ; set counter 1 initial value 52: divider for UART clock (2000000 / 52 = 38461 Hz -- almost 38400 baud)
  ld a, 52
  out (TIMER_CNT1), a

  ; send 8251a initialization sequence
  ld c, UART_CMD
  ld hl, uart_init_seq
  ld b, uart_init_end - uart_init_seq
  otir
  ret

puts:
  ld a, (hl)
  cp 0
  jr z, _puts_ret
  call putchar
  inc hl
  jp puts

_puts_ret:
  ret

crlf:
  ld a, 10
  call putchar
  ld a, 13
  call putchar
  ret

putdec_value: defw 0

; hl: value to print
putdec:
  ld (putdec_value), hl
  ld bc, 0xff
  push bc

_putdec_0:
  push hl
  pop bc
  srl h
  rr l
  srl b
  rr c
  srl b
  rr c
  adc hl, bc        ; q = (num >> 1) + (num >> 2)

  push hl
  pop bc
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  adc hl, bc        ; q = q + (q >> 4)

  push hl
  pop bc
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  srl b
  rr c
  adc hl, bc        ; q = q + (q >> 8)

  srl h
  rr l
  srl h
  rr l
  srl h
  rr l             ; q = q >> 3

  push hl
  pop de
  push hl
  pop bc
  push hl
  push hl
  pop bc
  add hl, hl
  add hl, hl
  push hl
  pop bc
  pop hl
  adc hl, bc
  add hl, hl
  push hl
  pop bc
  ld hl, (putdec_value)
  sbc hl, bc       ; r = num - (((q << 2) + q) << 1)

  push de
  pop bc
  ld a, l
  cp 10
  jr c, _putdec_1
  inc bc
  ld de, 10
  sbc hl, de

_putdec_1:
  ld (putdec_value), bc   ; num = q + (r > 9)

  ld de, '0'
  add hl, de          ; r + '0'
  push hl

  ; if (num == 0) break
  ld hl, (putdec_value)
  ld a, l
  cp 0
  jr z, _putdec_2

  jp _putdec_0

_putdec_2:
  ld a, h
  cp 0
  jr z, _putdec_3

  jp _putdec_0

_putdec_3:
  pop hl
  ld a, l
  cp 0xff
  jr z, _putdec_4
  call putchar
  jp _putdec_3

_putdec_4:
  ret

; hl: value to print
; b=1: full width (4 chars) regardless of value
puthex:
  ; full width?
  ld a, b
  cp 1
  jr z, _puthex_msb

  ; should we print msb (value > 256)?
  ld a, h
  cp 0
  jr z, _puthex_lsb

_puthex_msb:
  ; print msb
  srl a
  srl a
  srl a
  srl a

  cp 10
  jr c, _puthex_0
  sbc a, 10
  add a, 'A'
  call putchar
  jp _puthex_10

_puthex_0:
  add a, '0'
  call putchar

_puthex_10:
  ld a, h
  and 0xF

  cp 10
  jr c, _puthex_1
  sbc a, 10
  add a, 'A'
  call putchar
  jp _puthex_lsb

_puthex_1:
  add a, '0'
  call putchar

_puthex_lsb:
  ; print lsb
  ld a, l

  srl a
  srl a
  srl a
  srl a

  cp 10
  jr c, _puthex_3
  sbc a, 10
  add a, 'A'
  call putchar
  jp _puthex_13

_puthex_3:
  add a, '0'
  call putchar

_puthex_13:
  ld a, l
  and 0xF

  cp 10
  jr c, _puthex_4
  sbc a, 10
  add a, 'A'
  call putchar
  ret

_puthex_4:
  add a, '0'
  call putchar

  ret

putchar:
  ld d, a

  ; wait until transmitter is ready to accept a new byte from CPU
_putchar_wait_txe:
  in a, (UART_CMD)
  bit 2, a
  jr z, _putchar_wait_txe

  ld a, d
  out (UART_DATA), a
  ret
