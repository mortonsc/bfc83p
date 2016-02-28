#include <string.h>
#include "lib/c_ti83p.h"

#define APPEND_DST(c) (*(dst++) = (c))
#define PUSH(addr) (*(stack_ptr++) = (addr))
#define POP() (*(--stack_ptr))

#define LD_HL 0x21
#define INC_HL 0x23
#define DEC_HL 0x2B
#define INC_AT_HL 0x34
#define DEC_AT_HL 0x35
#define ADD_A_N 0xC6
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
#define RET 0xC9

const uint8_t header[] =
    {
        t2ByteTok, tasmCmp,
        0x21, 0x72, 0x98, /* ld hl,#appBackUpScreen */
        0x01, 0x00, 0x03, /*ld bc,#buffer_size */
    /* ZeroLoop: */
        0x36, 0x00,       /*ld (hl),#0          */
        0x23,             /*inc hl */
        0x0B,             /*dec bc */
        0x78,             /*ld a,b */
        0xB7,             /*or a */
        0x20, 0xF8,       /*jr nz,ZeroLoop */
        0x79,             /*ld a,c */
        0xB7,             /*or a */
        0x20, 0xF4,       /*jr nz,ZeroLoop */
        0x21, 0x72, 0x98 /* ld hl,#appBackUpScreen */
    };

uint8_t *src;
uint8_t *src_end;

uint8_t prog_buffer[3000];
uint8_t *dst;
uint8_t *exec;

__at APP_BACKUP_SCREEN uint8_t *stack[BUFFER_SIZE / sizeof(uint8_t*)];
uint8_t **stack_ptr;

/* state variables */
uint8_t delta = 0;
uint16_t distance = 0;
typedef enum {ADDING, MOVING_LEFT, MOVING_RIGHT, NONE} State;
State prog_state = NONE;

void resolve_state()
{
    switch (prog_state) {
    case ADDING:
        APPEND_DST(LD_A_HL);
        APPEND_DST(ADD_A_N);
        APPEND_DST(delta);
        APPEND_DST(LD_HL_A);
        break;
    case MOVING_LEFT:
    case MOVING_RIGHT:
        APPEND_DST(LD_DE_NN);
        APPEND_DST(distance & 0x0F); /* lower nibble */
        APPEND_DST(distance >> 4); /* higher nibble */
        APPEND_DST(ADD_HL_DE);
        break;
    }
    prog_state = NONE;
    delta = 0;
    distance = 0;
}

void increment()
{
    if (prog_state != ADDING) {
        resolve_state();
        prog_state = ADDING;
    }
    delta++;
}

void decrement()
{
    if (prog_state != ADDING) {
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

    if (prog_state == MOVING_RIGHT && distance == 0)
        prog_state = MOVING_LEFT;

        distance += (uint16_t) -1;
}

void move_right()
{
    if (prog_state != MOVING_LEFT && prog_state != MOVING_RIGHT) {
        resolve_state();
        prog_state = MOVING_RIGHT;
    }

    if (prog_state == MOVING_LEFT && distance == 0)
        prog_state = MOVING_RIGHT;

        distance++;
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
    uint8_t *open = POP();
    int8_t jp_dist = dst - open;

    resolve_state();

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
            begin_loop();
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
    APPEND_DST(RET);

    size = (dst - prog_buffer);
    exec = CCreateProtPrgm("BFDST", size + sizeof(header));
    memcpy(exec, header, sizeof(header));
    exec += sizeof(header);
    memcpy(exec, prog_buffer, size);

    CPutS("compilation successful");
    CNewLine();

    return 0;
}
