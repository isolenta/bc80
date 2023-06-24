; GNU z80asm requires colon after constant name, our as doesn't.
; By the way it's also valid syntax for EQU in the our implementation

nop
val1: equ 10
ld a,val1
val2: equ (val1 + 2)
ld a,val2
val3: equ (val1 - 2)
ld a,val3
val4: equ (val1 * 2h)
ld a,val4
val5: equ (val1 / 5)
ld a,val5

val6: equ val1 + val2
ld a,val6
val7: equ val1 - val2
ld a,val7
val8: equ val1 * val2
ld a,val8
val9: equ val1 / val5
ld a,val9

cmplx: equ ((val1 + val2) * (val2 - val1)) / val5
ld a,cmplx

val10: equ 055h
val11: equ 0aah
val12: equ (val10 & val11)
ld a, val12
val13: equ (val10 | val11)
ld a, val13
