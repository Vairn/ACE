#ifndef HARDWARE_CUSTOM_H
#define HARDWARE_CUSTOM_H

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma pack(push, 2)
#define ACE_HOST_CUSTOM_PACKED
#else
#define ACE_HOST_CUSTOM_PACKED __attribute__((packed, aligned(2)))
#endif

struct AudChannel {
	APTR ac_ptr;
	UWORD ac_len;
	UWORD ac_per;
	UWORD ac_vol;
	UWORD ac_dat;
	UWORD ac_pad[2];
};

struct SpriteDef {
	UWORD pos;
	UWORD ctl;
	UWORD dataa;
	UWORD datab;
};

/* Layout matches Amiga hardware/custom.h (16-bit aligned, 32-bit pointer fields). */
struct Custom {
	UWORD bltddat;
	UWORD dmaconr;
	UWORD vposr;
	UWORD vhposr;
	UWORD dskdatr;
	UWORD joy0dat;
	UWORD joy1dat;
	UWORD clxdat;
	UWORD adkconr;
	UWORD pot0dat;
	UWORD pot1dat;
	UWORD potinp;
	UWORD serdatr;
	UWORD dskbytr;
	UWORD intenar;
	UWORD intreqr;
	APTR dskpt;
	UWORD dsklen;
	UWORD dskdat;
	UWORD refptr;
	UWORD vposw;
	UWORD vhposw;
	UWORD copcon;
	UWORD serdat;
	UWORD serper;
	UWORD potgo;
	UWORD joytest;
	UWORD strequ;
	UWORD strvbl;
	UWORD strhor;
	UWORD strlong;
	UWORD bltcon0;
	UWORD bltcon1;
	UWORD bltafwm;
	UWORD bltalwm;
	APTR bltcpt;
	APTR bltbpt;
	APTR bltapt;
	APTR bltdpt;
	UWORD bltsize;
	UBYTE pad2d;
	UBYTE bltcon0l;
	UWORD bltsizv;
	UWORD bltsizh;
	UWORD bltcmod;
	UWORD bltbmod;
	UWORD bltamod;
	UWORD bltdmod;
	UWORD pad34[4];
	UWORD bltcdat;
	UWORD bltbdat;
	UWORD bltadat;
	UWORD pad3b[3];
	UWORD deniseid;
	UWORD dsksync;
	ULONG cop1lc;
	ULONG cop2lc;
	UWORD copjmp1;
	UWORD copjmp2;
	UWORD copins;
	UWORD diwstrt;
	UWORD diwstop;
	UWORD ddfstrt;
	UWORD ddfstop;
	UWORD dmacon;
	UWORD clxcon;
	UWORD intena;
	UWORD intreq;
	UWORD adkcon;
	struct AudChannel aud[4];
	APTR bplpt[8];
	UWORD bplcon0;
	UWORD bplcon1;
	UWORD bplcon2;
	UWORD bplcon3;
	UWORD bpl1mod;
	UWORD bpl2mod;
	UWORD bplcon4;
	UWORD clxcon2;
	UWORD bpldat[8];
	APTR sprpt[8];
	struct SpriteDef spr[8];
	UWORD color[32];
	UWORD htotal;
	UWORD hsstop;
	UWORD hbstrt;
	UWORD hbstop;
	UWORD vtotal;
	UWORD vsstop;
	UWORD vbstrt;
	UWORD vbstop;
	UWORD sprhstrt;
	UWORD sprhstop;
	UWORD bplhstrt;
	UWORD bplhstop;
	UWORD hhposw;
	UWORD hhposr;
	UWORD beamcon0;
	UWORD hsstrt;
	UWORD vsstrt;
	UWORD hcenter;
	UWORD diwhigh;
	UWORD padf3[11];
	UWORD fmode;
} ACE_HOST_CUSTOM_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#undef ACE_HOST_CUSTOM_PACKED

#ifdef __cplusplus
}
#endif

#endif
