; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at https://mozilla.org/MPL/2.0/.

    ; vector table
    ; reset vector
    org 0000h
    ljmp main
    ; timer 0 overflow vector
    org 000Bh
    ljmp timer0_interrupt
    org 0036h

    ; increment R7 whenever timer 0 overflows (100 times per second)
timer0_interrupt:
    mov TH0,  #high(divisor)
    inc R7
    reti

main:
    mov SP,   #030h           ; move SP after all registers and bit memory

    mov TMOD, #021h           ; timer 0: 16 bit, timer 1: 8 bit, autoreload
    mov TL0,  #low(divisor)   ; timer 0:  overflows 100 times per second
    mov TH0,  #high(divisor)
    mov TH1,  #0FFh           ; timer 1: 28.8k baud @ 11.0592 MHz (baud = fosc / (12 * 32 * (255 - TH1)))
    setb ET0

    mov SCON, #050h           ; 8n1, receiver enabled
    mov a, PCON               ; double baud rate to 57.6k baud
    orl a, #080h
    mov PCON, a

    setb EA                   ; enable interrupts
    setb TR0                  ; start timers
    setb TR1

    ; print message
print:
    mov R7, #0                ; clear overflow count
    mov DPTR, #message
nextchar:
    clr a
    movc a, @a+DPTR
    jz wait
    mov SBUF, a
    inc DPTR
tx:
    jnb TI, tx
    clr TI
    ajmp nextchar

    ; wait one second
wait:
    cjne R7, #100, wait
    ajmp print

; constants
message: db 'Hello, 8051 assembler', 13, 10, 0
divisor equ 56320

END