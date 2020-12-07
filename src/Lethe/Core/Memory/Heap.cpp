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
	__builtin___clear_cache(ptr, (void *)((char *)ptr+size));
#endif

	return res;
}

#endif

// Heap

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

}
