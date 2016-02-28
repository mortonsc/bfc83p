;; This is a rendering in assembly of the output from bfc83p.
;; It can be assembled and run using SPASM-ng, and, more likely than not,
;; other assemblers as well, as long as you have a copy of ti83plus.inc.
;; (The one included with c_ti83p will not work, as it uses sdasz80 syntax.)
;;
;; The header inserted by bfc83p does a few things: it zeroes out
;; all of appBackUpScreen, initializes hl to point to the beginning
;; of appBackUpScreen, and sets up an interrupt that will terminate the program
;; if an [ON]-key interrupt is generated. The last is necessary for on-calc
;; BF-programming to be an idea even remotely worth considering.
;;
;; Copyright (C) 2016 Scott Morton
;; This program is free software; you may distribute and/or modify it
;; according to the terms of the MIT license.

#include "ti83plus.inc"

#define     progStart $9D95
#define     sys_interrupt $003A

.org        progStart - 2
.db         t2ByteTok, tasmCmp

        jr Start ; 9D95, 9D96

save_sp:
        .dw $0000 ; 9D97, 9D98

;; no real purpose for this, but we need something to fill 4 bytes
bf_header:
        .db "BFCx" ; 9D99, 9D9A, 9D9B, 9D9C

;; yes, the interrupt has to begin exactly at $9D9D.
;; the CPU jumps to a (more-or-less) random point in the vector table,
;; so the address of the interrupt must have identical high and low bytes.
Interrupt: ; 9D9D--make a wish!
        ex af, af'
        exx

        in a,(4) ; check if an [ON]-key interrupt has been generated.
        bit 0,a  ; this doesn't actually check if [ON] is being pressed;
        jp z,sys_interrupt ; the system interrupt takes care of that.

        ;; user pressed [ON], so we're terminating the program.
        ;; return the stack to where it was when the program was first called,
        ;; and return.
        ld sp,(save_sp)
        ex af,af'
        exx
        im 1
        ret

Start:
        ld (save_sp),sp ; save the stack pointer, so we can terminate at once
                        ; even if we later push things onto the stack.

        di
        ;; set up vector table in statVars pointing to the interrupt
        ld hl,$8B00
        ld de,$8B01
        ld bc,$100
        ld (hl),$9D
        ldir

        ld a,$8B
        ld i,a

        im 2
        ei

        ;; zero out appBackUpScreen
        ld hl,appBackUpScreen
        ld de,appBackUpScreen+1
        ld bc,$300
        ld (hl),0
        ldir
        ld hl,appBackUpScreen

        ;; compiled BF program goes here
        inc (hl)        ; +
        dec (hl)        ; -
        inc hl          ; >
        dec hl          ; <
StartLoop:              ; [
        ld a,(hl)
        or a
        jr z,EndLoop

        jr StartLoop    ; ]
EndLoop:

        ld a,(hl)       ; .
        bcall(_PutC)

        bcall(_getkey)  ; ,
        ld (hl),a

        im 1
        ret

.end
