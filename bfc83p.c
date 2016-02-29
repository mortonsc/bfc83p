/*
 * An on-calc BF compiler for TI-83+ series calculators.
 *
 * Copyright (C) Scott Morton 2016
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license.
 */
#include <string.h>
#include <stdbool.h>
#include "lib/c_ti83p.h"
#include "header.h"

#define APPEND_DST(c) (*(dst++) = (c))
#define PUSH(addr) (*(stack_ptr++) = (addr))
#define POP() (*(--stack_ptr))

/* opcodes */
#define LD_HL 0x21
#define INC_HL 0x23
#define DEC_HL 0x2B
#define INC_AT_HL 0x34
#define DEC_AT_HL 0x35
#define ADD_A_N 0xC6
#define INC_A 0x3C
#define DEC_A 0x3D
#define LD_A_N 0x3E
#define LD_AT_HL_N 0x36
#define LD_BC_NN 0x01
#define LD_DE_NN 0x11
#define ADD_HL_DE 0x19
#define LD_A_HL 0x7E
#define LD_HL_A 0x77
#define OR_A 0xB7
#define JR_Z 0x28
#define JR 0x18
#define BCALL 0xEF
#define PUT_C_1 0x04
#define PUT_C_2 0x45
#define GET_KEY_1 0x72
#define GET_KEY_2 0x49

/* extended (2-byte) opcodes */
#define EXTENDED_INSTR 0xED
#define SBC_HL_DE 0x52
#define CPIR 0xB1
#define CPDR 0xB9

uint8_t *src;
uint8_t *src_end;

uint8_t prog_buffer[3000];
uint8_t *dst;
uint8_t *exec;

__at APP_BACKUP_SCREEN uint8_t *stack[BUFFER_SIZE / sizeof(uint8_t*)];
uint8_t **stack_ptr;

/*
 * The compiler is, in essence, a state machine. Many commands are not compiled
 * at the time they are parsed, but instead change the state of the compiler,
 * and then code is written to the output program at the next state transition.
 *
 * For example, when compiling the sequence ++++>, nothing is written when the
 * +'s are read; when the > is reached, machine code is output to add 4 to the
 * value at the memory pointer, but nothing will be written for the > until
 * the next state transition.
 *
 * The correspondance between BF commands and compiler output is for the most
 * part as would be expected: a series of +'s becomes an addition,
 * [-] loads 0 in the current cell, and so on. One oddity is that a series of
 * -'s does not result in a subtraction but an addition of the two's complement
 * negative of the number of -'s.
 */

/* state variables */
uint8_t delta = 0; /* amount to add to the current cell */
uint16_t distance_right = 0; /* number of cells to move to the right */
uint16_t distance_left = 0; /* number of cells to move to the left */

typedef enum {ADDING, LOADING, MOVING_LEFT, MOVING_RIGHT, NONE} State;
State prog_state = NONE;

bool is_reg_a_loaded = false;


/* ensure the a register contains the value under the mem ptr */
void load_reg_a()
{
    if (!is_reg_a_loaded) {
        APPEND_DST(LD_A_HL);
        is_reg_a_loaded = true;
    }
}

/* ensure that the cell under the mem ptr contains the value stored in a */
void unload_reg_a()
{
    if (is_reg_a_loaded) {
        APPEND_DST(LD_HL_A);
        is_reg_a_loaded = false;
    }
}

void resolve_addition()
{
    /*
     * For small delta, it's more efficient to repeatedly inc/dec rather than
     * to add. At some of the marginal values, one method took one more byte
     * but required one less clock cycle; in these cases I opted for brevity
     * rather than speed.
     *
     * If reg a is already loaded, it's faster to directly inc/dec a
     * but if not, it's faster to inc/dec (hl) than to load it.
     */
    if (is_reg_a_loaded) {
        if (delta <= 3) {
            while (delta--)
                APPEND_DST(INC_A);
            return;
        } else if (delta >= 253) {
            while (delta++)
                APPEND_DST(DEC_A);
            return;
        }
    } else {
        if (delta <= 2) {
            while (delta--)
                APPEND_DST(INC_AT_HL);
            return;
        } else if (delta >= 254) {
            while (delta++)
                APPEND_DST(DEC_AT_HL);
            return;
        }
    }

    if (delta != 0) {
        load_reg_a();
        APPEND_DST(ADD_A_N);
        APPEND_DST(delta);
    }

}

/* Write the output for the current state, and reset all the state variables.*/
void resolve_state()
{
    switch (prog_state) {
    case ADDING:
        resolve_addition();
        break;
    case LOADING:
        if (is_reg_a_loaded) {
            APPEND_DST(LD_A_N);
        } else {
            APPEND_DST(LD_AT_HL_N);
        }
        APPEND_DST(delta);
        break;
    case MOVING_LEFT:
        unload_reg_a();
        if (distance_left <= 5) {
            while (distance_left--)
                APPEND_DST(DEC_HL);
        } else {
            APPEND_DST(OR_A);
            APPEND_DST(LD_DE_NN);
            APPEND_DST(distance_left & 0x0F); /* lower nibble */
            APPEND_DST(distance_left >> 4); /* higher nibble */
            APPEND_DST(EXTENDED_INSTR);
            APPEND_DST(SBC_HL_DE);
        }
        break;
    case MOVING_RIGHT:
        unload_reg_a();
        if (distance_right <= 3) {
            while (distance_right--)
                APPEND_DST(INC_HL);
        } else {
            APPEND_DST(LD_DE_NN);
            APPEND_DST(distance_right & 0x0F); /* lower nibble */
            APPEND_DST(distance_right >> 4); /* higher nibble */
            APPEND_DST(ADD_HL_DE);
        }
        break;
    }
    prog_state = NONE;
    delta = 0;
    distance_left = 0;
    distance_right = 0;
}

/* carry out one + command */
void increment()
{
    if (prog_state != ADDING && prog_state != LOADING) {
        resolve_state();
        prog_state = ADDING;
    }
    delta++;
}

/* carry out one - command */
void decrement()
{
    if (prog_state != ADDING && prog_state != LOADING) {
        resolve_state();
        prog_state = ADDING;
    }
    delta--;
}

/* carry out one < command */
void move_left()
{
    if (prog_state != MOVING_LEFT && prog_state != MOVING_RIGHT) {
        resolve_state();
        prog_state = MOVING_LEFT;
    }

    if (prog_state == MOVING_RIGHT) {
        if (distance_left > 0) {
            distance_right--;
        } else {
            distance_left = 0;
            prog_state = MOVING_LEFT;
        }
    } else {
        distance_left++;
    }
}

/* carry out one > command */
void move_right()
{
    if (prog_state != MOVING_LEFT && prog_state != MOVING_RIGHT) {
        resolve_state();
        prog_state = MOVING_RIGHT;
    }

    if (prog_state == MOVING_LEFT) {
        if (distance_left > 0) {
            distance_left--;
        } else {
            distance_right = 0;
            prog_state = MOVING_RIGHT;
        }
    }

    if (prog_state == MOVING_RIGHT)
        distance_right++;
}

/* carry out one [ command */
void begin_loop()
{
    resolve_state();

    unload_reg_a();

    PUSH(dst);
    APPEND_DST(LD_A_HL);
    APPEND_DST(OR_A);
    APPEND_DST(JR_Z); /* ret z */
    APPEND_DST(0x00); /* will be filled in with the jump dist */

}

/* carry out one ] command */
void end_loop()
{
    uint8_t *open;
    int8_t jp_dist;

    resolve_state();

    open = POP();
    jp_dist = dst - open;

    APPEND_DST(JR);
    APPEND_DST(-(jp_dist+2)); /* jump back to start of loop */
    *(open + 3) = jp_dist - 2; /* jump to just past the ] */
}

/* carry out one . command */
void output()
{
    /*
     * load reg a before the addition is resolved--this provides a slight
     * increase in speed if a small number is to be added or subtracted,
     * as inc/dec a is faster than inc/dec (hl).
     */
    if (prog_state == ADDING)
        load_reg_a();

    resolve_state();
    load_reg_a();
    APPEND_DST(BCALL);
    APPEND_DST(PUT_C_1);
    APPEND_DST(PUT_C_2);
}

/* carry out one , command */
void input()
{
    /* , resets the value at this address, so discard any planned addition */
    if (prog_state != ADDING)
        resolve_state();

    APPEND_DST(BCALL);
    APPEND_DST(GET_KEY_1);
    APPEND_DST(GET_KEY_2);
    is_reg_a_loaded = true;
}

/* carry out one [-]/[+] command */
void clear()
{
    if (prog_state != ADDING && prog_state != LOADING)
        resolve_state();

    prog_state = LOADING;
    delta = 0;
}

/* carry out one [<] command */
void scan_left()
{
    resolve_state();
    unload_reg_a();

    APPEND_DST(LD_BC_NN);
    APPEND_DST(0x00);
    APPEND_DST(0x00);
    APPEND_DST(LD_A_N);
    APPEND_DST(0x00);
    APPEND_DST(EXTENDED_INSTR);
    APPEND_DST(CPDR);

    move_right(); /* cpdr decrements hl once after the 0 cell is found */
}

/* carry out one [>] command */
void scan_right()
{
    resolve_state();
    unload_reg_a();

    APPEND_DST(LD_BC_NN);
    APPEND_DST(0x00);
    APPEND_DST(0x00);
    APPEND_DST(LD_A_N);
    APPEND_DST(0x00);
    APPEND_DST(EXTENDED_INSTR);
    APPEND_DST(CPIR);

    move_left(); /* cpir increments hl once after the 0 cell is found */
}

int main()
{
    uint16_t size;
    uint16_t src_size;

    src = CRecallPrgm("BFSRC", &src_size);
    if (!src) {
        CPutS("source not found");
        CNewLine();
        return 1;
    }

    dst = prog_buffer;
    stack_ptr = stack;

    src_end = src + src_size;
    while (src < src_end) {
        switch(*src) {
        case tAdd:
            increment();
            break;
        case tSub:
            decrement();
            break;
        case tGT:
        case tRBrace:
            move_right();
            break;
        case tLT:
        case tLBrace:
            move_left();
            break;
        case tLBrack:
            if ((*(src+1) == tSub || *(src+1) == tAdd)
                    && *(src+2) == tRBrack) {
                clear();
                src += 2;
            } else if ((*(src+1) == tGT || *(src+1) == tRBrace)
                    && *(src+2) == tRBrack) {
                scan_right();
                src += 2;
            } else if ((*(src+1) == tLT || *(src+1) == tLBrace)
                    && *(src+2) == tRBrack) {
                scan_left();
                src += 2;
            } else {
                begin_loop();
            }
            break;
        case tRBrack:
            end_loop();
            break;
        case tDecPt:
            output();
            break;
        case tComma:
            input();
        }
        src++;

        if (dst > prog_buffer + sizeof(prog_buffer)) {
            /* unlikely, but just in case */
            CPutS("error: program too large");
            CNewLine();
            return 1;
        } else if (stack_ptr < stack) {
            CPutS("error: unmatched closing ]");
            CNewLine();
            return 1;
        }
    }

    if (stack_ptr > stack) {
        CPutS("error: unmatched opening \xC1");
        CNewLine();
        return 1;
    }

    /*
     * Create the output file and write the compiled code to it, along with
     * the header and footer
     */
    size = (dst - prog_buffer);
    exec = CCreateProtPrgm("BFDST", size + sizeof(header) + sizeof(footer));
    memcpy(exec, header, sizeof(header));
    exec += sizeof(header);
    memcpy(exec, prog_buffer, size);
    exec += size;
    memcpy(exec, footer, sizeof(footer));

    CPutS("compilation successful");
    CNewLine();

    return 0;
}
