#include "lib/c_ti83p.h"

const uint8_t safe_header[] =
    {
        t2ByteTok, tasmCmp,

        0xEF, 0x84, 0x4C, /* bcall _DisableAPD */
        0xF3,             /* di */
        0xED, 0x73, 0xCF, 0x9D, /* ld (save_sp),sp */

        0x21, 0xBA, 0x9D, /* ld hl,#interrupt */
        0x22, 0x3F, 0x87, /*ld (0x873F),hl */
        0x22, 0x7F, 0x87, /*ld (0x877F),hl */
        0x22, 0xBF, 0x87, /*ld (0x87BF),hl */
        0x22, 0xFF, 0x87, /*ld (0x87FF),hl */
        0x3E, 0x87,       /*ld a,#0x87 */
        0xED, 0x47,       /* ld i,a */

        0x3E, 0x0B,       /* ld a,#hardware_interrupt_on */
        0xD3, 0x03,       /* out (3),a */
        0xED, 0x5E,       /* im 2 */
        0xFB,             /* ei */
        0xC3, 0x3C, 0x00, /*jp Init */
        
    /* interrupt: */
        0x08,             /* ex af, af' */
        0xD9,             /* exx */
        0xDB, 0x04,       /* in a,(4) */
        0xCB, 0x47,       /* bit 0,a */
        /*0xCA, 0x3A, 0x00, /* jp z,sys_interrupt */
        0xC3, 0x3A, 0x00,
        0x08,             /* ex af, af' */
        0xD9,             /* exx */
        0xEF, 0x87, 0x4C, /* bcall _EnableAPD */
        0xED, 0x56,       /* im 1 */
        0xED, 0x7B, 0xCF, 0x9D, /* ld sp,(save_sp) */
        0xC9,             /* ret */

    /* save_sp: */
        0x00, 0x00,       /* .dw 0x0000 */

    /*Init: */
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
