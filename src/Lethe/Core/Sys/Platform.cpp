#include "Platform.h"
#include "../Collect/Array.h"

#if LETHE_OS_WINDOWS
#	include <windows.h>
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

void Platform::AdjustForHyperthreading()
{
#if	LETHE_CPU_X86
	bool hyperThreading = 0;
#	if LETHE_COMPILER_MSC
	int id[4] = {0};
	__cpuid(id, 0);
	int nids = id[0];

	if (nids >= 2)
	{
		id[2] = id[3] = 0;
		__cpuid(id, 1);
		hyperThreading = (id[3] & (1<<28)) != 0;
	}

#	else
	int id[4] = {0};
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
		id[2] = id[3] = 0;
		asm(
			"cpuid":
			"=a" (id[0]),
			"=b" (id[1]),
			"=c" (id[2]),
			"=d" (id[3]) :
			"a" (1), "c" (0)
		);
		hyperThreading = (id[3] & (1<<28)) != 0;
	}

#	endif

	if (hyperThreading)
		physCores = Max(1, physCores / 2);

#endif
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

	LPFN_GLPI glpi = (LPFN_GLPI)GetProcAddress(kmodule, "GetLogicalProcessorInformation");

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
	auto fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
	return fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &pwow) ? pwow != FALSE : false;
#else
	return false;
#endif
}

}
