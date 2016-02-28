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
#define LD_A_HL 0x7E
#define OR_A 0xB7
#define JR_Z 0x28
#define JR 0x18
#define BCALL 0xEF
#define PUT_C_1 0x04
#define PUT_C_2 0x45
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
/* uint8_t **stack_ptr = stack; */
uint8_t **stack_ptr;

int main()
{
    uint8_t *open;
    int8_t dist;
    uint16_t size;
    uint16_t src_size;

    src = CRecallPrgm("BFSRC", &src_size);
    if (!src) {
        CPutS("source not found");
        CNewLine();
        return 1;
    }

    dst = prog_buffer;
    memcpy(dst, header, sizeof(header));
    dst += sizeof(header);

    stack_ptr = stack;

    src_end = src + src_size;
    while (src < src_end) {
        switch(*src) {
        case tAdd:
            APPEND_DST(INC_AT_HL);
            break;
        case tSub:
            APPEND_DST(DEC_AT_HL);
            break;
        case tGT:
        case tRBrace:
            APPEND_DST(INC_HL);
            break;
        case tLT:
        case tLBrace:
            APPEND_DST(DEC_HL);
            break;
        case tLBrack:
            PUSH(dst);
            APPEND_DST(LD_A_HL);
            APPEND_DST(OR_A);
            APPEND_DST(JR_Z); /* ret z */
            APPEND_DST(0x00); /* will be filled in with the jump dist */
            break;
        case tRBrack:
            open = POP();
            dist = dst - open;
            APPEND_DST(JR);
            APPEND_DST(-(dist+2)); /* jump back to start of loop */
            *(open + 3) = dist - 2; /* jump to just past the ] */
            break;
        case tDecPt:
            APPEND_DST(LD_A_HL);
            APPEND_DST(BCALL);
            APPEND_DST(PUT_C_1);
            APPEND_DST(PUT_C_2);
            break;
        }
        src++;
    }
    APPEND_DST(RET);

    size = (dst - prog_buffer);
    exec = CCreateProtPrgm("BFDST", size);
    memcpy(exec, prog_buffer, size);

    return 0;
}
