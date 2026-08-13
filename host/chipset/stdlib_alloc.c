#include "host_os.h"
#include <ace_host/chipset.h>
#include <clib/exec_protos.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define HDR_MAGIC_AMIGA 0xACE0A10Cul
#define HDR_MAGIC_HOST  0xACE00001ul

typedef struct tMallocHdr {
	ULONG ulTotal;
	ULONG ulMagic;
} tMallocHdr;

static int memReady(void) {
	return aceHostBusBase() != 0;
}

static void *mallocHdr(size_t n, int clear) {
	size_t total;
	tMallocHdr *h;
	if(n > (size_t)0x7ffffff0u - sizeof(tMallocHdr)) {
		return 0;
	}
	total = sizeof(tMallocHdr) + n;
	if(memReady()) {
		h = (tMallocHdr *)(uintptr_t)AllocMem(
			(ULONG)total, MEMF_ANY | (clear ? MEMF_CLEAR : 0)
		);
		if(!h) {
			return 0;
		}
		h->ulMagic = HDR_MAGIC_AMIGA;
	}
	else {
		h = (tMallocHdr *)hostOsHeapMalloc(total);
		if(!h) {
			return 0;
		}
		if(clear) {
			memset(h, 0, total);
		}
		h->ulMagic = HDR_MAGIC_HOST;
	}
	h->ulTotal = (ULONG)total;
	return h + 1;
}

static void freeHdr(void *p) {
	tMallocHdr *h;
	ULONG ulTotal;
	ULONG ulMagic;
	if(!p) {
		return;
	}
	h = (tMallocHdr *)p - 1;
	ulMagic = h->ulMagic;
	ulTotal = h->ulTotal;
	h->ulMagic = 0;
	if(ulMagic == HDR_MAGIC_AMIGA) {
		FreeMem((APTR)(uintptr_t)h, ulTotal);
	}
	else if(ulMagic == HDR_MAGIC_HOST) {
		hostOsHeapFree(h);
	}
	else {
		fprintf(stderr, "[ACE_HOST] free: bad ptr %p\n", p);
	}
}

static void *reallocHdr(void *p, size_t n) {
	tMallocHdr *h;
	void *q;
	size_t oldUser;
	if(!p) {
		return mallocHdr(n, 0);
	}
	if(!n) {
		freeHdr(p);
		return 0;
	}
	h = (tMallocHdr *)p - 1;
	if(h->ulMagic != HDR_MAGIC_AMIGA && h->ulMagic != HDR_MAGIC_HOST) {
		fprintf(stderr, "[ACE_HOST] realloc: bad ptr %p\n", p);
		return 0;
	}
	oldUser = (size_t)h->ulTotal - sizeof(tMallocHdr);
	if(n <= oldUser) {
		return p;
	}
	q = mallocHdr(n, 0);
	if(!q) {
		return 0;
	}
	memcpy(q, p, oldUser);
	freeHdr(p);
	return q;
}

static void *callocHdr(size_t nmemb, size_t size) {
	if(size && nmemb > ((size_t)-1) / size) {
		return 0;
	}
	return mallocHdr(nmemb * size, 1);
}

static char *strdupHdr(const char *s) {
	size_t n;
	char *p;
	if(!s) {
		return 0;
	}
	n = strlen(s) + 1;
	p = (char *)mallocHdr(n, 0);
	if(p) {
		memcpy(p, s, n);
	}
	return p;
}

static size_t msizeHdr(void *p) {
	tMallocHdr *h;
	if(!p) {
		return 0;
	}
	h = (tMallocHdr *)p - 1;
	if(h->ulMagic != HDR_MAGIC_AMIGA && h->ulMagic != HDR_MAGIC_HOST) {
		return 0;
	}
	return (size_t)h->ulTotal - sizeof(tMallocHdr);
}

#if defined(_MSC_VER)
void *malloc(size_t n) { return mallocHdr(n, 0); }
void *calloc(size_t nmemb, size_t size) { return callocHdr(nmemb, size); }
void *realloc(void *p, size_t n) { return reallocHdr(p, n); }
void free(void *p) { freeHdr(p); }
char *strdup(const char *s) { return strdupHdr(s); }
char *_strdup(const char *s) { return strdupHdr(s); }
size_t _msize(void *p) { return msizeHdr(p); }
#else
void *__wrap_malloc(size_t n) { return mallocHdr(n, 0); }
void *__wrap_calloc(size_t nmemb, size_t size) { return callocHdr(nmemb, size); }
void *__wrap_realloc(void *p, size_t n) { return reallocHdr(p, n); }
void __wrap_free(void *p) { freeHdr(p); }
char *__wrap_strdup(const char *s) { return strdupHdr(s); }
char *__wrap__strdup(const char *s) { return strdupHdr(s); }
size_t __wrap__msize(void *p) { return msizeHdr(p); }
#endif
