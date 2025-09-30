#include "Platform.h"
#include "../Collect/Array.h"

#if LETHE_OS_WINDOWS
#	include "Windows_Include.h"
#	include <intrin.h>

#	if LETHE_CPU_AMD64 && LETHE_COMPILER_CLANG
// FIXME: clang 64-bit mode warning hack
#		undef WINAPI
#		define WINAPI
#	endif

#elif LETHE_OS_BSD || LETHE_OS_OSX || LETHE_OS_IOS
#	include <sys/sysctl.h>
#elif LETHE_OS_LINUX || LETHE_OS_ANDROID
#	include <unistd.h>
#endif

namespace lethe
{

// Platform

int Platform::physCores = 0;
int Platform::logCores = 0;

#if LETHE_OS_WINDOWS
typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
UINT winOldErrorMode;
#endif

unsigned Platform::GetCPUTier()
{
	unsigned res = 0;
#if	LETHE_CPU_X86
	int id[4] = {0, 0, 0, 0};
#	if LETHE_COMPILER_MSC
	__cpuid(id, 0);
	int nids = id[0];

	if (nids >= 2)
	{
		__cpuid(id, 1);
	}

#	else
	asm(
		"cpuid":
		"=a" (id[0]),
		"=b" (id[1]),
		"=c" (id[2]),
		"=d" (id[3]) :
		"a" (0)
	);
	int nids = id[0];

	if (nids >= 2)
	{
		asm(
			"cpuid":
			"=a" (id[0]),
			"=b" (id[1]),
			"=c" (id[2]),
			"=d" (id[3]) :
			"a" (1), "c" (0)
		);
	}

#	endif
	if ((id[2] & (1<<19)) != 0)
		res |= CPU_TIER_SSE4_1;

	if ((id[2] & (1<<20)) != 0)
		res |= CPU_TIER_SSE4_2;

	if ((id[2] & (1<<23)) != 0)
		res |= CPU_TIER_POPCNT;

	if ((id[3] & (1<<28)) != 0)
		res |= CPU_TIER_HYPERTHREADING;
#endif
	return res;
}

void Platform::AdjustForHyperthreading()
{
	auto tier = GetCPUTier();

	if (tier & CPU_TIER_HYPERTHREADING)
		physCores = Max(1, physCores / 2);
}

void Platform::Init()
{
#if LETHE_OS_WINDOWS
	SYSTEM_INFO si;
	si.dwNumberOfProcessors = 1;
	GetSystemInfo(&si);

	physCores = logCores = si.dwNumberOfProcessors;
	AdjustForHyperthreading();

	winOldErrorMode = SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

	// reference: http://msdn.microsoft.com/en-us/library/ms683194
	HMODULE kmodule = GetModuleHandle(TEXT("kernel32"));

	if (!kmodule)
		return;

	LPFN_GLPI glpi = (LPFN_GLPI)(void *)GetProcAddress(kmodule, "GetLogicalProcessorInformation");

	if (!glpi)
		return;

	Array<Byte> buf;
	DWORD len = 0;
	glpi(0, &len);
	buf.Resize(len);

	if (glpi((PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)buf.GetData(), &len) == FALSE)
		return;

	DWORD count = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
	physCores = logCores = 0;

	for (DWORD i=0; i<count; i++)
	{
		const SYSTEM_LOGICAL_PROCESSOR_INFORMATION *inf =
			(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)buf.GetData() + i;

		if (inf->Relationship != RelationProcessorCore)
			continue;

		physCores++;
		ULONG_PTR mask = inf->ProcessorMask;

		while (mask)
		{
			logCores++;
			mask &= mask - 1;
		}
	}

#elif LETHE_OS_BSD || LETHE_OS_OSX || LETHE_OS_IOS
	// reference: http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
	int numCPU = 0;
	int mib[4];
	size_t len = sizeof(numCPU);

	// set the mib for hw.ncpu
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU

	// get the number of CPUs from the system
	sysctl(mib, 2, &numCPU, &len, NULL, 0);

	if (numCPU < 1)
	{
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &numCPU, &len, NULL, 0);
	}

	physCores = logCores = numCPU;
	AdjustForHyperthreading();
#elif LETHE_OS_LINUX || LETHE_OS_ANDROID
	physCores = logCores = sysconf(_SC_NPROCESSORS_ONLN);
	AdjustForHyperthreading();
#else
	// fallback
	physCores = logCores = 1;
#endif
}

void Platform::Done()
{
#if LETHE_OS_WINDOWS
	SetErrorMode(winOldErrorMode);
#endif
}

bool Platform::Is32BitProcessOn64BitOS()
{
#if LETHE_32BIT && LETHE_OS_WINDOWS
	BOOL pwow = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	auto fnIsWow64Process = (LPFN_ISWOW64PROCESS)(void *)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
	return fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &pwow) ? pwow != FALSE : false;
#else
	return false;
#endif
}

}
