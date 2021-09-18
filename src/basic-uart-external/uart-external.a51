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

    ; start timer 0 to count centiseconds
    mov TMOD, #001h           ; timer 0: 16 bit
    mov TL0,  #low(divisor)   ; timer 0:  overflows 100 times per second
    mov TH0,  #high(divisor)
    setb ET0
    setb EA                   ; enable interrupts
    setb TR0                  ; start timer 0

    ; initialize the am85c30 in asynchronous polling mode
    ;
    ; PORT2 + R0 = am85c30 channel b control address
    ; DPTR = am85c30 initialization data
    ;
    mov R1, #0
    mov P2, #high(uart_control_b)
    mov R0, #low(uart_control_b)
    mov DPTR, #am85c30_init_data
am85c30_init:
    mov a, R1
    inc R1
    movc a, @a+DPTR
    movx @R0, a
    cjne R1, #am85c30_init_data_len, am85c30_init

    ; Print any characters that might be in the
    ; receive FIFO. Then print a message.
    ;
    ; PORT2 + R0 = am85c30 channel b control
    ; PORT2 + R1 = am85c30 channel b data
    ; DPTR = message
    ;
    mov P2, #high(uart_control_b)
    mov R0, #low(uart_control_b)
    mov R1, #low(uart_data_b)
loop:
    mov R7, #0                ; clear overflow count

print_fifo:
    movx a, @R0
    anl a, #01h
    jz print_message
    movx a, @R1
    acall am85c30_put_char
    ajmp print_fifo

print_message:
    mov DPTR, #message
nextchar:
    clr a
    movc a, @a+DPTR
    jz wait
    inc DPTR
    acall am85c30_put_char
    ajmp nextchar
wait:
    cjne R7, #200, wait
    ajmp loop

; put character into uart
am85c30_put_char:
    movx @R1, a
am85c30_put_char_wait:
    movx a, @R0
    anl a, #04h
    jz am85c30_put_char_wait
    ret

; code memory constants
am85c30_init_data:
    ; configure stuff
    db 9, 0C0h
    db 4, 004h
    db 2, 000h
    db 3, 0C0h
    db 5, 060h
    db 9, 000h
    db 10, 000h
    db 11, 056h
    ; setup baud
    db 12, 22
    db 13, 0
    ; other stuff
    db 14, 002h
    db 14, 003h
    db 3, 0C1h
    db 5, 068h
am85c30_init_data_end:

; message to print to UART
message: db 'Hello, 8051 assembler (external uart)', 13, 10, 0

; compiler time constants
am85c30_init_data_len equ   (am85c30_init_data_end - am85c30_init_data)
divisor               equ   56320
uart_control_b        xdata 09400h
uart_data_b           xdata 09401h
uart_control_a        xdata 09402h
uart_data_a           xdata 09403h

END