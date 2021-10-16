#include "../Sys/Platform.h"
#include "Memory.h"
#include "Heap.h"

#include <stdlib.h>

#if LETHE_OS_WINDOWS
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <Windows.h>
#else
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#if LETHE_OS_OSX || LETHE_OS_IOS
// note: needs Leopard or higher
#	include <libkern/OSCacheControl.h>
#endif

namespace lethe
{

// platform-specific stuff, i.e. allocs etc.
#if LETHE_OS_WINDOWS

static size_t GetPageSize()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	// 64k is minimum for MEM_RESERVE, we won't get contiguous space otherwise!
	return Max((size_t)si.dwPageSize, (size_t)65536);
}

// request new segment
static void *AllocSegment(size_t &size, bool executable = false)
{
	LETHE_ASSERT(size);
	size_t pmask = GetPageSize()-1;
	LETHE_ASSERT(IsPowerOfTwo(GetPageSize()));
	size += pmask;
	size &= ~pmask;

	return VirtualAlloc(nullptr, size, MEM_COMMIT|MEM_RESERVE, executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
}

static void FreeSegment(void *ptr, size_t size)
{
	LETHE_ASSERT(size);
	char *c = (char *)ptr;

	while (size)
	{
		MEMORY_BASIC_INFORMATION mbi;
		VirtualQuery(c, &mbi, sizeof(mbi));
		BOOL res = VirtualFree(mbi.BaseAddress, 0, MEM_RELEASE);
		LETHE_ASSERT(res);
		(void)res;
		c += mbi.RegionSize;
		size -= (size_t)mbi.RegionSize;
	}
}

static bool WriteProtectSegment(void *ptr, size_t size, bool enable)
{
	LETHE_ASSERT(ptr && size);

	DWORD tmp;
	bool res = VirtualProtect(ptr, size, enable ? PAGE_EXECUTE_READ : PAGE_EXECUTE_READWRITE, &tmp) != FALSE;

	if (FlushInstructionCache(GetCurrentProcess(), ptr, size) != TRUE)
		res = false;

	return res;
}

#else

// OSX fix
#ifndef MAP_ANONYMOUS
#	define MAP_ANONYMOUS MAP_ANON
#endif

static size_t GetPageSize()
{
	return (size_t)Max(sysconf(_SC_PAGE_SIZE), 65536l);
}

// request new segment
static void *AllocSegment(size_t &size, bool executable = false)
{
	LETHE_ASSERT(size);
	size_t pmask = GetPageSize()-1;
	LETHE_ASSERT(IsPowerOfTwo(GetPageSize()));
	size += pmask;
	size &= ~pmask;

	return mmap(nullptr, size, PROT_READ|PROT_WRITE|(executable ? PROT_EXEC : 0), MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

static void FreeSegment(void *ptr, size_t size)
{
	LETHE_ASSERT(size);
	munmap(ptr, size);
}

static bool WriteProtectSegment(void *ptr, size_t size, bool enable)
{
	LETHE_ASSERT(ptr && size);

	bool res = !mprotect(ptr, size, enable ? PROT_READ|PROT_EXEC : PROT_READ|PROT_WRITE|PROT_EXEC);

	// hmm, how to flush icache here in a portable way?!

#if LETHE_OS_OSX || LETHE_OS_IOS
	sys_icache_invalidate(ptr, size);
#elif LETHE_OS_LINUX || LETHE_OS_ANDROID
	__builtin___clear_cache((char *)ptr, ((char *)ptr+size));
#endif

	return res;
}

#endif

// Heap

size_t Heap::GetOSPageSize()
{
	return GetPageSize();
}

void *Heap::AllocateExecutableMemory(size_t &size)
{
	return AllocSegment(size, true);
}

bool Heap::WriteProtectExecutableMemory(void *ptr, size_t size, bool enable)
{
	return WriteProtectSegment(ptr, size, enable);
}

void Heap::FreeExecutableMemory(void *ptr, size_t size)
{
	FreeSegment(ptr, size);
}

#if LETHE_OS_WINDOWS && LETHE_64BIT

// reference: https://bugzilla.mozilla.org/show_bug.cgi?id=844196

struct UnwindInfo
{
	Byte version : 3;
	Byte flags : 5;
	Byte sizeOfPrologue;
	Byte countOfUnwindCodes;
	Byte frameRegister : 4;
	Byte frameOffset : 4;
	ULONG exceptionHandler;
};

struct ExceptionHandlerRecord
{
	RUNTIME_FUNCTION runtimeFunction;
	UnwindInfo unwindInfo;
	Byte thunk[12];
};

static DWORD WINAPI JITExceptionHandler(
	PEXCEPTION_RECORD exceptionRecord,
	_EXCEPTION_REGISTRATION_RECORD *,
	PCONTEXT context,
	_EXCEPTION_REGISTRATION_RECORD **)
{
	EXCEPTION_POINTERS pointers =
	{
		(PEXCEPTION_RECORD)exceptionRecord,
		(PCONTEXT)context
	};

	// call UnhandledExceptionFilter if any - is there a better, more general way?
	// such as executing SEH __try {} __catch {}?
	auto old = SetUnhandledExceptionFilter(NULL);
	SetUnhandledExceptionFilter(old);

	if (old)
		old(&pointers);

	return EXCEPTION_EXECUTE_HANDLER;
}

bool Heap::RegisterExecutableMemory(void *ptr, size_t size)
{
	auto *rec = (ExceptionHandlerRecord *)ptr;

	auto &rtfun = rec->runtimeFunction;

	rtfun.BeginAddress = 0;
	rtfun.EndAddress = (DWORD)size;
	rtfun.UnwindData = offsetof(ExceptionHandlerRecord, unwindInfo);

#ifndef UNW_FLAG_EHANDLER
	constexpr Byte UNW_FLAG_EHANDLER = 1;
#endif
	auto &unw = rec->unwindInfo;
	MemSet(&unw, 0, sizeof(rec->unwindInfo));
	unw.version = 1;
	unw.flags = UNW_FLAG_EHANDLER;
	unw.exceptionHandler = offsetof(ExceptionHandlerRecord, thunk);

	// prepare thunk
	auto *thunk = rec->thunk;
	// mov rax, imm64
	thunk[0] = 0x48;
	thunk[1] = 0xb8;
	auto *h = (void *)&JITExceptionHandler;
	MemCpy(thunk+2, &h, sizeof(void *));
	// jmp rax
	thunk[10] = 0xff;
	thunk[11] = 0xe0;

	return RtlAddFunctionTable(&rec->runtimeFunction, 1, (DWORD64)ptr) != FALSE;
}

bool Heap::UnregisterExecutableMemory(void *ptr)
{
	return RtlDeleteFunctionTable((PRUNTIME_FUNCTION)ptr) != FALSE;
}

#else
bool Heap::RegisterExecutableMemory(void *ptr, size_t size)
{
	(void)ptr;
	(void)size;
	return true;
}

bool Heap::UnregisterExecutableMemory(void *ptr)
{
	(void)ptr;
	return true;
}
#endif

}
