#include "host_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#endif

typedef struct tOsDir {
#ifdef _WIN32
	HANDLE h;
	WIN32_FIND_DATAA fd;
	int first;
#else
	DIR *pDir;
#endif
} tOsDir;

void *hostOsMapLow(size_t size) {
#ifdef _WIN32
	void *p = VirtualAlloc(
		(LPVOID)(uintptr_t)0x20000000, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
	);
	if(p) {
		return p;
	}
	p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if(p && (uintptr_t)p > (uintptr_t)0xFFFFFFFFULL - size) {
		fprintf(stderr, "[ACE_HOST] ERR: mapping %p is above 4GB\n", p);
		VirtualFree(p, 0, MEM_RELEASE);
		return 0;
	}
	return p;
#else
	void *p = mmap(
		NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_32BIT
		| MAP_32BIT
#endif
		, -1, 0
	);
	if(p == MAP_FAILED) {
		return 0;
	}
	if((uintptr_t)p > (uintptr_t)0xFFFFFFFFULL - size) {
		munmap(p, size);
		return 0;
	}
	return p;
#endif
}

void hostOsUnmapLow(void *p, size_t size) {
	if(!p) {
		return;
	}
#ifdef _WIN32
	(void)size;
	VirtualFree(p, 0, MEM_RELEASE);
#else
	munmap(p, size);
#endif
}

int hostOsIsDir(const char *path) {
#ifdef _WIN32
	DWORD a = GetFileAttributesA(path);
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* AllocMem/FreeMem without exec headers — windows.h typedefs BOOL. */
extern uint32_t AllocMem(uint32_t byteSize, uint32_t attributes);
extern void FreeMem(uint32_t memoryBlock, uint32_t byteSize);
#ifndef MEMF_ANY
#define MEMF_ANY 0L
#endif
#ifndef MEMF_CLEAR
#define MEMF_CLEAR (1L << 16)
#endif

void *hostOsDirOpen(const char *path) {
	tOsDir *p = (tOsDir *)(uintptr_t)AllocMem(
		(uint32_t)sizeof(tOsDir), (uint32_t)(MEMF_ANY | MEMF_CLEAR)
	);
	if(!p) {
		return 0;
	}
#ifdef _WIN32
	{
		char pat[280];
		snprintf(pat, sizeof(pat), "%s\\*", path);
		p->h = FindFirstFileA(pat, &p->fd);
		p->first = (p->h != INVALID_HANDLE_VALUE);
		if(p->h == INVALID_HANDLE_VALUE) {
			FreeMem((uint32_t)(uintptr_t)p, (uint32_t)sizeof(tOsDir));
			return 0;
		}
	}
#else
	p->pDir = opendir(path);
	if(!p->pDir) {
		FreeMem((uint32_t)(uintptr_t)p, (uint32_t)sizeof(tOsDir));
		return 0;
	}
#endif
	return p;
}

void hostOsDirClose(void *dir) {
	tOsDir *p = (tOsDir *)dir;
	if(!p) {
		return;
	}
#ifdef _WIN32
	if(p->h && p->h != INVALID_HANDLE_VALUE) {
		FindClose(p->h);
	}
#else
	if(p->pDir) {
		closedir(p->pDir);
	}
#endif
	FreeMem((uint32_t)(uintptr_t)p, (uint32_t)sizeof(tOsDir));
}

int hostOsDirNext(void *dir, char *name, unsigned nameMax, int *isDir, long *size) {
	tOsDir *p = (tOsDir *)dir;
	if(!p) {
		return 0;
	}
#ifdef _WIN32
	while(p->first || FindNextFileA(p->h, &p->fd)) {
		p->first = 0;
		if(p->fd.cFileName[0] == '.') {
			continue;
		}
		strncpy(name, p->fd.cFileName, nameMax - 1);
		name[nameMax - 1] = 0;
		if(isDir) {
			*isDir = (p->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
		}
		if(size) {
			*size = (long)p->fd.nFileSizeLow;
		}
		return 1;
	}
	return 0;
#else
	{
		struct dirent *e;
		while((e = readdir(p->pDir))) {
			if(e->d_name[0] == '.') {
				continue;
			}
			strncpy(name, e->d_name, nameMax - 1);
			name[nameMax - 1] = 0;
			if(isDir) {
				*isDir = 0;
			}
			if(size) {
				*size = 0;
			}
			return 1;
		}
	}
	return 0;
#endif
}

int hostOsMkdir(const char *path) {
#ifdef _WIN32
	if(CreateDirectoryA(path, NULL)) {
		return 1;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS;
#else
	if(mkdir(path, 0755) == 0) {
		return 1;
	}
	return errno == EEXIST;
#endif
}

#ifdef _WIN32
void *hostOsHeapMalloc(size_t n) {
	if(!n) {
		n = 1;
	}
	return HeapAlloc(GetProcessHeap(), 0, n);
}

void *hostOsHeapCalloc(size_t nmemb, size_t size) {
	size_t n;
	if(size && nmemb > ((size_t)-1) / size) {
		return 0;
	}
	n = nmemb * size;
	if(!n) {
		n = 1;
	}
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
}

void *hostOsHeapRealloc(void *p, size_t n) {
	if(!p) {
		return hostOsHeapMalloc(n);
	}
	if(!n) {
		hostOsHeapFree(p);
		return 0;
	}
	return HeapReAlloc(GetProcessHeap(), 0, p, n);
}

void hostOsHeapFree(void *p) {
	if(p) {
		HeapFree(GetProcessHeap(), 0, p);
	}
}
#else
void *__real_malloc(size_t n);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *p, size_t n);
void __real_free(void *p);

void *hostOsHeapMalloc(size_t n) {
	if(!n) {
		n = 1;
	}
	return __real_malloc(n);
}

void *hostOsHeapCalloc(size_t nmemb, size_t size) {
	return __real_calloc(nmemb, size);
}

void *hostOsHeapRealloc(void *p, size_t n) {
	return __real_realloc(p, n);
}

void hostOsHeapFree(void *p) {
	__real_free(p);
}
#endif
