#ifndef HARDWARE_INTBITS_H
#define HARDWARE_INTBITS_H

#define INTB_TBE         0
#define INTB_DSKBLK      1
#define INTB_SOFTINT     2
#define INTB_PORTS       3
#define INTB_COPER       4
#define INTB_VERTB       5
#define INTB_BLIT        6
#define INTB_AUD0        7
#define INTB_AUD1        8
#define INTB_AUD2        9
#define INTB_AUD3        10
#define INTB_RBF         11
#define INTB_DSKSYNC     12
#define INTB_EXTER       13
#define INTB_INTEN       14
#define INTB_SETCLR      15

#define INTF_TBE         (1L << INTB_TBE)
#define INTF_DSKBLK      (1L << INTB_DSKBLK)
#define INTF_SOFTINT     (1L << INTB_SOFTINT)
#define INTF_PORTS       (1L << INTB_PORTS)
#define INTF_COPER       (1L << INTB_COPER)
#define INTF_VERTB       (1L << INTB_VERTB)
#define INTF_BLIT        (1L << INTB_BLIT)
#define INTF_AUD0        (1L << INTB_AUD0)
#define INTF_AUD1        (1L << INTB_AUD1)
#define INTF_AUD2        (1L << INTB_AUD2)
#define INTF_AUD3        (1L << INTB_AUD3)
#define INTF_RBF         (1L << INTB_RBF)
#define INTF_DSKSYNC     (1L << INTB_DSKSYNC)
#define INTF_EXTER       (1L << INTB_EXTER)
#define INTF_INTEN       (1L << INTB_INTEN)
#define INTF_SETCLR      (1L << INTB_SETCLR)

#endif
