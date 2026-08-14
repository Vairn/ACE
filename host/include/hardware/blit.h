#ifndef HARDWARE_BLIT_H
#define HARDWARE_BLIT_H

#define HSIZEBITS 6
#define VSIZEBITS 6
#define HSIZEMASK 0x3F
#define VSIZEMASK 0x3F

#define ABC    0x80
#define ABNC   0x40
#define NABC   0x20
#define NANBC  0x10
#define ANBC   0x08
#define ANBNC  0x04
#define NABNC  0x02
#define NANBNC 0x01

#define BC0B_DEST 8
#define BC0B_SRCC 9
#define BC0B_SRCB 10
#define BC0B_SRCA 11
#define BC0F_DEST 0x100
#define BC0F_SRCC 0x200
#define BC0F_SRCB 0x400
#define BC0F_SRCA 0x800

#define DEST BC0F_DEST
#define SRCC BC0F_SRCC
#define SRCB BC0F_SRCB
#define SRCA BC0F_SRCA

#define ASHIFTSHIFT 12
#define BSHIFTSHIFT 12

#define LINEMODE  0x01
#define FILL_XOR  0x10
#define FILL_OR   0x08
#define FILL_CARRYIN 0x04
#define ONEDOT    0x02
#define OVFLAG    0x20
#define SIGNFLAG  0x40
#define BLITREVERSE 0x02
#define BC1F_DESC BLITREVERSE
#define SING      0x02
#define AUL       0x04
#define SUL       0x08
#define SUD       0x10

#endif
