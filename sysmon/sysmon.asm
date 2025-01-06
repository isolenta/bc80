include 'bc80.inc.asm'

    org 0
    di
    im 0
    ld sp, STACK_TOP

    jp _start

    ; ISR entry points
    org 0x0020

    jp isr0_handler   ; 0x0020
    nop
    jp isr0_handler   ; 0x0024
    nop
    jp isr0_handler   ; 0x0028
    nop
    jp isr0_handler   ; 0x002C
    nop
    jp isr0_handler   ; 0x0030
    nop
    jp isr0_handler   ; 0x0034
    nop
    jp isr0_handler   ; 0x0038
    nop
    jp isr0_handler   ; 0x003C
    nop

    org 0x66

nmi_handler:
    retn
    push af
    push hl
    push bc
    push de
    ld hl, nmi_str
    call puts
    call crlf
    pop de
    pop bc
    pop hl
    pop af
    retn

isr0_handler:
    ;di
    ;push af
    ;push hl
    ;push bc
    ;push de
    exx
    ld hl, isr_str
    call puts

    ; read irr
    ld a, 0x0a
    out (PIC_CTL0), a
    in a, (PIC_CTL0)
    ld h, 0
    ld l, a
    call puthex

    ld a, ' '
    call putchar

    ; read isr
    ld a, 0x0b
    out (PIC_CTL0), a
    in a, (PIC_CTL0)
    ld h, 0
    ld l, a
    call puthex

    call crlf

    ;pop de
    ;pop bc
    ;pop hl

    ; issue EOI
    ld a, 0x20
    out (PIC_CTL0), a

    ;pop af
    exx
    ;ei
    reti

_start:
    call uart_init

    ; i8259 initialization
    ;
    ; ICW1:
    ;   IC4=0:    ICW4 hasn't to be read
    ;   SNGL=1:   the only 8259A in the system
    ;   ADI=1:    CALL address interval = 4
    ;   LTIM=0:   Edge detect logic on the interrupt input
    ;   A7,A6,A5: 0b001 (CALL addresses starts from 0x20)
    ld a, 0x36
    out (PIC_CTL0), a
    ;
    ; ICW2:
    ;   A15..A8:  0b00000000 (CALL addresses starts from 0x20)
    ld a, 0
    out (PIC_CTL1), a
    ;
    ; ICW3: not needed due SNGL=1
    ;
    ; ICW4: not needed due IC4=0
    ;
    ; OCW1:
    ;   unmask all interrupts
    ld a, 0   ; 0xff
    out (PIC_CTL1), a
    ;
    ; enable interrupts
    ei

loop:
scancode_not_changed:
    in a, (KBD_DATA)
    cp e
    ld e, a
    jr z, scancode_not_changed

    ld hl, scancode_msg
    call puts

    ld h, 0
    ld l, e
    call puthex

    call crlf

    jp loop

    halt

include 'uart.asm'

scancode_msg:      defb          'PS/2 Scancode: ', 0
nmi_str:           defb          'Oops! NMI handler', 0
isr_str:           defb          'ISR: ', 0
