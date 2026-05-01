
the palette typing mismatch has annoyed me for ages; I finally wrote up the options in one place so you can sanity-check or say if you already had a direction.

**The mess:** `tVPort::pPalette` is `UWORD *` in `extview.h`, but in AGA we actually allocate `sizeof(ULONG) * (1 << ubBpp)` and use it as `ULONG *`. Loaders are the same story: `paletteLoadFromPath` / `paletteLoadFromFd` take `UWORD *` but AGA `.plt` is `ULONG` per entry. It works if you know the score and cast, but the API promises the wrong width — so you get easy stride/index mistakes, call sites full of casts, and the headers don’t help static tools. Mixed ECS/AGA also makes it too easy to free the wrong size if you’re not tight about which vport “owns” the contract.

**What I want:** types (or at least access) that make the real width obvious at the boundary, without forcing a massive tree-wide rewrite, and that still feel like ACE.

**A — keep `UWORD *`, add helpers** that check `VP_FLAG_AGA` and return `UWORD *` vs `ULONG *` (e.g. `vPortPaletteEcs` / `vPortPaletteAga`, assert or NULL on mismatch). Lowest churn, no struct change. Annoyance: the field in the header still lies, and anyone can skip the helpers.

```c
/* extview.h — field stays misleading but “official” access is typed */
UWORD *pPalette;

UWORD *vPortPaletteEcs(tVPort *pVPort); /* NULL / assert if AGA */
ULONG *vPortPaletteAga(tVPort *pVPort); /* NULL / assert if not AGA */

/* call site */
#ifdef ACE_USE_AGA_FEATURES
if (pVPort->eFlags & VP_FLAG_AGA) {
	ULONG *pPal = vPortPaletteAga(pVPort);
	pPal[1] = 0x0fff0f00;
} else
#endif
{
	UWORD *pPal = vPortPaletteEcs(pVPort);
	pPal[1] = 0x0f00;
}
```

**B — `void *` + helpers** so storage is opaque and you only ever touch palette memory through helpers. Honest about mode-dependent layout; more call sites to touch; helpers have to become the default habit.

```c
/* extview.h */
void *pPalette;

/* one pattern: buffer + typed views (names negotiable) */
UWORD *vPortPaletteAsUword(tVPort *pVPort); /* ECS only; NULL if AGA */
ULONG *vPortPaletteAsUlong(tVPort *pVPort); /* AGA only; NULL if ECS */

#ifdef ACE_USE_AGA_FEATURES
if (pVPort->eFlags & VP_FLAG_AGA) {
	ULONG *pPal = vPortPaletteAsUlong(pVPort);
	pPal[3] = 0x00ffffff;
} else
#endif
{
	UWORD *pPal = vPortPaletteAsUword(pVPort);
	pPal[3] = 0x0fff;
}
```

**C — union on `tVPort`** like `union { UWORD *ecs; ULONG *aga; }` and branch on AGA *(this is what I’d ship)*. Reads clearer at use sites than random casts; access is a bit uglier (`uPalette.aga` vs one flat name), and mixed paths still need discipline — but you see the intent in the member, not buried in casts.

```c
typedef union _tVPortPalette {
	UWORD *ecs;
	ULONG *aga;
} tVPortPalette;

typedef struct _tVPort {
	/* ... */
	tVPortPalette uPalette;
} tVPort;

#ifdef ACE_USE_AGA_FEATURES
if (pVPort->eFlags & VP_FLAG_AGA)
	pVPort->uPalette.aga[0] = 0x00000000;
else
#endif
	pVPort->uPalette.ecs[0] = 0x000;
```

**D — split loader APIs** (`paletteLoadFromPathEcs` / `...Aga` or similar). Best compile-time signal for wrong width; more surface area and doc/example churn. Natural fit on top of **C** — `uPalette.ecs` / `uPalette.aga` feed the right loader without shoehorning types.

```c
/* palette.h — two entry points, pointer type matches file / mode intent */
void paletteLoadFromPathEcs(const char *szPath, UWORD *pPalette, UWORD uwMaxLength);
void paletteLoadFromPathAga(const char *szPath, ULONG *pPalette, UWORD uwMaxLength);

/* ECS build or v1 .plt */
paletteLoadFromPathEcs("data/bg.plt", pVPort->uPalette.ecs, 1u << pVPort->ubBpp);

/* AGA v2 .plt — wrong function = won’t compile if you pass UWORD* */
paletteLoadFromPathAga("data/bg_aga.plt", pVPort->uPalette.aga, 1u << pVPort->ubBpp);
```
(With **A** you’d still call a single `paletteLoadFromPath` and cast internally or in a thin wrapper — D is specifically about splitting the loader surface.)

**E — leave as is** No change to `tVPort`, loaders, or signatures — maybe clearer comments in the headers about “AGA backs this with `ULONG`” so at least the contract is written down. Zero churn for existing games and forks; all the typing footguns from **the mess** stay unless everyone is disciplined with casts. Reasonable if we decide this isn’t worth API motion right now.

```c
/* today — unchanged; AGA paths keep lying to the type system on purpose */
UWORD *pPalette;

#ifdef ACE_USE_AGA_FEATURES
if (pVPort->eFlags & VP_FLAG_AGA) {
	ULONG *pPal = (ULONG *)pVPort->pPalette;
	pPal[2] = 0x00f000f0;
} else
#endif
{
	pVPort->pPalette[2] = 0x00f0;
}

paletteLoadFromPath("data/bg.plt", pVPort->pPalette, 1u << pVPort->ubBpp); /* cast inside if needed */
```

**My gut:** spell the invariants in `extview.h` / `palette.h` so people aren’t reverse-engineering from malloc sites. Then move `pPalette` to something like **`C` (typed union)** — honestly prefer that over **`B`** opaque `void *` because the layout stays visible in the struct without every touch going through accessors. **`A`** is fine as a stopgap but it doesn’t fix the lying header. **`E`** if we explicitly punt on typing work and only improve docs — fair. **`D`** optional: nice with **C** if we want loaders to nag at compile time; otherwise keep one loader entry and cast/wrap internally.

**Things I’ll hold myself to regardless:** don’t free based on another vport’s idea of the allocation; allocate/free under the same mode + color count; keep casts in helpers; examples stay ECS=`UWORD`, AGA=`ULONG`.

Does **C** match how you want `tVPort` to read? Main pushback is usually verbosity of `uPalette.ecs` / `.aga` vs a single field name — if that’s acceptable I’d rather do that than opaque **`B`**. Or are you happier with **`E`** (document-only) for now? And separately: worth adding **`D`** split loaders or keep one path and tidy inside?
