#include <string.h>
#include "lib/c_ti83p.h"
#include "header.h"

#define APPEND_DST(c) (*(dst++) = (c))
#define PUSH(addr) (*(stack_ptr++) = (addr))
#define POP() (*(--stack_ptr))

#define LD_HL 0x21
#define INC_HL 0x23
#define DEC_HL 0x2B
#define INC_AT_HL 0x34
#define DEC_AT_HL 0x35
#define ADD_A_N 0xC6
#define LD_A_N 0x3E
#define LD_AT_HL_NN 0x36
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

/* state variables */
uint8_t delta = 0;
uint16_t distance_right = 0;
uint16_t distance_left = 0;
typedef enum {ADDING, LOADING, MOVING_LEFT, MOVING_RIGHT, NONE} State;
State prog_state = NONE;

void resolve_state()
{
    switch (prog_state) {
    case ADDING:
        if (delta == 1) {
            APPEND_DST(INC_AT_HL);
        } else if (delta == 2) {
            APPEND_DST(INC_AT_HL);
            APPEND_DST(INC_AT_HL);
        } else if (delta == (uint8_t) -1) {
            APPEND_DST(DEC_AT_HL);
        } else if (delta == (uint8_t) -2) {
            APPEND_DST(DEC_AT_HL);
            APPEND_DST(DEC_AT_HL);
        } else if (delta != 0) {
            APPEND_DST(LD_A_HL);
            APPEND_DST(ADD_A_N);
            APPEND_DST(delta);
            APPEND_DST(LD_HL_A);
        }
        break;
    case LOADING:
        APPEND_DST(LD_AT_HL_NN);
        APPEND_DST(delta);
        break;
    case MOVING_LEFT:
        if (distance_left <= 5) {
            while (distance_left--)
                APPEND_DST(DEC_HL);
        } else {
            APPEND_DST(OR_A);
            APPEND_DST(LD_DE_NN);
            APPEND_DST(distance_left & 0x0F);
            APPEND_DST(distance_left >> 4);
            APPEND_DST(EXTENDED_INSTR);
            APPEND_DST(SBC_HL_DE);
        }
        break;
    case MOVING_RIGHT:
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

void increment()
{
    if (prog_state != ADDING && prog_state != LOADING) {
        resolve_state();
        prog_state = ADDING;
    }
    delta++;
}

void decrement()
{
    if (prog_state != ADDING && prog_state != LOADING) {
        resolve_state();
        prog_state = ADDING;
    }
    delta--;
}

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

void begin_loop()
{
    resolve_state();

    PUSH(dst);
    APPEND_DST(LD_A_HL);
    APPEND_DST(OR_A);
    APPEND_DST(JR_Z); /* ret z */
    APPEND_DST(0x00); /* will be filled in with the jump dist */

}

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

void output()
{
    resolve_state();
    APPEND_DST(LD_A_HL);
    APPEND_DST(BCALL);
    APPEND_DST(PUT_C_1);
    APPEND_DST(PUT_C_2);
}

void input()
{
    /* , resets the value at this address, so discard any planned addition */
    if (prog_state != ADDING)
        resolve_state();

    APPEND_DST(BCALL);
    APPEND_DST(GET_KEY_1);
    APPEND_DST(GET_KEY_2);
    APPEND_DST(LD_HL_A);
}

void clear()
{
    if (prog_state != ADDING && prog_state != LOADING)
        resolve_state();

    prog_state = LOADING;
    delta = 0;
}

void scan_left()
{
    resolve_state();

    APPEND_DST(LD_BC_NN);
    APPEND_DST(0x00);
    APPEND_DST(0x00);
    APPEND_DST(LD_A_N);
    APPEND_DST(0x00);
    APPEND_DST(EXTENDED_INSTR);
    APPEND_DST(CPDR);
    APPEND_DST(INC_HL);
}

void scan_right()
{
    resolve_state();

    APPEND_DST(LD_BC_NN);
    APPEND_DST(0x00);
    APPEND_DST(0x00);
    APPEND_DST(LD_A_N);
    APPEND_DST(0x00);
    APPEND_DST(EXTENDED_INSTR);
    APPEND_DST(CPIR);
    APPEND_DST(DEC_HL);
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
    }
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
