#include <string.h>
#include "lib/c_ti83p.h"

#define LD_HL 0x21
#define APP_BACKUP_SCREEN_1 0x72
#define APP_BACKUP_SCREEN_2 0x98
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
        0
    };

uint8_t *src;
uint8_t *src_end;

uint8_t prog_buffer[3000];
uint8_t *dst;
uint8_t *exec;

__at APP_BACKUP_SCREEN uint8_t *stack[BUFFER_SIZE / sizeof(uint8_t*)];
uint8_t **stack_ptr = stack;

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
    strcpy(dst, header);
    dst += strlen(header) - 1;


    src_end = src + src_size;
    while (src < src_end) {
        switch(*src) {
        case '+':
        case tAdd:
            *(dst++) = INC_AT_HL;
            break;
        case '-':
        case tSub:
            *(dst++) = DEC_AT_HL;
            break;
        case '>':
        case tGT:
            *(dst++) = INC_HL;
            break;
        case '<':
        case tLT:
            *(dst++) = DEC_HL;
            break;
        case '[':
        case tLBrack:
            *(stack_ptr++) = dst;
            *(dst++) = LD_A_HL;
            *(dst++) = OR_A;
            *(dst++)= JR_Z;
            *(dst++) = 0x00; /* will be filled in with the jump dist */
            break;
        case ']':
        case tRBrack:
            open = *(--stack_ptr);
            dist = dst - open;
            *(dst++) = JR;
            *(dst++) = -dist;
            *(open + 3) = dist; /* jump to just past the ] */
            break;
        case '.':
        case tDecPt:
            *(dst++) = LD_A_HL;
            *(dst++) = BCALL;
            *(dst++) = PUT_C_1;
            *(dst++) = PUT_C_2;
            break;
        }
        src++;
    }
    *(dst++) = RET;

    size = (dst - prog_buffer);
    exec = CCreateProtPrgm("BFDST", size);
    memcpy(exec, prog_buffer, size);

    return 0;
}
