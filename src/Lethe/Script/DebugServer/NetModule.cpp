#include <Lethe/Core/Sys/Platform.h>
#include "NetModule.h"

#include <stdio.h>

#include "Inline/NetOsIncludes.inl"

#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
// link to ws2_32.lib
#	pragma comment(lib, "ws2_32.lib")
#endif

namespace lethe
{

// NetModule

AtomicInt NetModule::refCount = 0;

void NetModule::Init()
{
	if (Atomic::Increment(refCount) != 1)
		return;

#if LETHE_OS_WINDOWS
	WSADATA wsaData;
	int err;

	err = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (err != 0)
		printf("WSAStartup failed with error: %d", err);

#endif
}

void NetModule::Done()
{
	if (!Atomic::Decrement(refCount))
		return;

#if LETHE_OS_WINDOWS
	WSACleanup();
#endif
}

}
