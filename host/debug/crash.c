/* Host-only crash dump: exception address, stack, addr2line if present. */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static FILE *s_pCrash;
static int s_inCrash;

static void crashOut(const char *szFmt, ...) {
	va_list va;
	va_start(va, szFmt);
	vfprintf(stderr, szFmt, va);
	va_end(va);
	if(s_pCrash) {
		va_start(va, szFmt);
		vfprintf(s_pCrash, szFmt, va);
		va_end(va);
		fflush(s_pCrash);
	}
	fflush(stderr);
	fflush(stdout);
}

static int canRead(ULONG_PTR p) {
	MEMORY_BASIC_INFORMATION mbi;
	if(!p || !VirtualQuery((const void *)p, &mbi, sizeof(mbi))) {
		return 0;
	}
	if(mbi.State != MEM_COMMIT) {
		return 0;
	}
	if(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) {
		return 0;
	}
	return 1;
}

static void *rebaseAddr(void *pAddr) {
	ULONG_PTR a = (ULONG_PTR)pAddr;
	ULONG_PTR base = (ULONG_PTR)GetModuleHandleA(NULL);
	ULONG_PTR low = base & 0xFFFFFFFFu;
	if(!a) {
		return pAddr;
	}
	/* 32-bit truncated pointer into this module (ASLR high bits stripped). */
	if(a < 0x100000000ull && a >= low && a < low + 0x800000u) {
		a = (base & ~(ULONG_PTR)0xFFFFFFFFu) | a;
	}
	if(a >= base && a < base + 0x800000u) {
		a = (ULONG_PTR)0x140000000ull + (a - base);
	}
	return (void *)a;
}

static void tryAddr2line(const char *szExe, void *pAddr) {
	char szA2l[MAX_PATH];
	char szCmd[1024];
	char szLine[256];
	FILE *pPipe;
	void *pFileAddr;

	if(!pAddr) {
		return;
	}
	pFileAddr = rebaseAddr(pAddr);
	if(GetEnvironmentVariableA("ACE_HOST_ADDR2LINE", szA2l, sizeof(szA2l)) == 0) {
		strcpy(szA2l, "C:\\msys64\\ucrt64\\bin\\addr2line.exe");
	}
	snprintf(
		szCmd, sizeof(szCmd),
		"\"%s\" -e \"%s\" -f -C -p %p",
		szA2l, szExe, pFileAddr
	);
	pPipe = _popen(szCmd, "r");
	if(!pPipe) {
		return;
	}
	while(fgets(szLine, sizeof(szLine), pPipe)) {
		crashOut("    %s", szLine);
	}
	_pclose(pPipe);
}

static void dumpContextStack(CONTEXT *pCtx) {
#ifdef _WIN64
	ULONG_PTR rbp, rip;
	int i;
	char szExe[MAX_PATH];

	if(!pCtx) {
		return;
	}
	GetModuleFileNameA(NULL, szExe, MAX_PATH);
	rip = pCtx->Rip;
	rbp = pCtx->Rbp;
	crashOut("call stack:\n");
	for(i = 0; i < 24; ++i) {
		crashOut("  [%d] %p\n", i, (void *)rip);
		tryAddr2line(szExe, (void *)rip);
		if(!rbp || rbp < 0x10000u || !canRead(rbp) || !canRead(rbp + 8)) {
			break;
		}
		rip = *(ULONG_PTR *)(rbp + 8);
		rbp = *(ULONG_PTR *)rbp;
	}
#else
	(void)pCtx;
#endif
}

static void dumpStack(void *pFault) {
	void *pFrames[32];
	USHORT n, i;
	char szExe[MAX_PATH];

	GetModuleFileNameA(NULL, szExe, MAX_PATH);
	crashOut("exe: %s\n", szExe);
	if(pFault) {
		crashOut("fault: %p\n", pFault);
		tryAddr2line(szExe, pFault);
	}
	n = CaptureStackBackTrace(0, 32, pFrames, NULL);
	crashOut("stack (%u frames):\n", (unsigned)n);
	for(i = 0; i < n; ++i) {
		crashOut("  [%u] %p\n", (unsigned)i, pFrames[i]);
		tryAddr2line(szExe, pFrames[i]);
	}
}

static int isFatal(DWORD code) {
	switch(code) {
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_PRIV_INSTRUCTION:
		case EXCEPTION_IN_PAGE_ERROR:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			return 1;
		default:
			return 0;
	}
}

static LONG WINAPI crashFilter(EXCEPTION_POINTERS *pInfo) {
	DWORD code;
	void *pAddr;

	if(!pInfo || !pInfo->ExceptionRecord || !isFatal(pInfo->ExceptionRecord->ExceptionCode)) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if(s_inCrash) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	s_inCrash = 1;
	s_pCrash = fopen("host_crash.txt", "w");
	code = pInfo && pInfo->ExceptionRecord
		? pInfo->ExceptionRecord->ExceptionCode : 0;
	pAddr = pInfo && pInfo->ExceptionRecord
		? pInfo->ExceptionRecord->ExceptionAddress : 0;
	crashOut(
		"\n[ACE_HOST] CRASH exception=0x%08lX addr=%p",
		(unsigned long)code, pAddr
	);
#ifdef _WIN64
	if(pInfo && pInfo->ContextRecord) {
		crashOut(" rip=%p rsp=%p\n",
			(void *)pInfo->ContextRecord->Rip,
			(void *)pInfo->ContextRecord->Rsp);
	}
	else {
		crashOut("\n");
	}
#else
	if(pInfo && pInfo->ContextRecord) {
		crashOut(" eip=%p esp=%p\n",
			(void *)pInfo->ContextRecord->Eip,
			(void *)pInfo->ContextRecord->Esp);
	}
	else {
		crashOut("\n");
	}
#endif
	if(code == EXCEPTION_ACCESS_VIOLATION && pInfo && pInfo->ExceptionRecord) {
		ULONG_PTR *p = pInfo->ExceptionRecord->ExceptionInformation;
		crashOut(
			"  av: %s %p\n",
			p[0] ? "write" : "read",
			(void *)p[1]
		);
	}
	dumpContextStack(pInfo ? pInfo->ContextRecord : 0);
	dumpStack(pAddr);
	if(s_pCrash) {
		fclose(s_pCrash);
		s_pCrash = 0;
	}
	if(IsDebuggerPresent()) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	_exit(1);
}

void aceHostCrashInit(void) {
	SetErrorMode(
		SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
	);
	SetUnhandledExceptionFilter(crashFilter);
	AddVectoredExceptionHandler(1, crashFilter);
}

#else

void aceHostCrashInit(void) {
}

#endif
