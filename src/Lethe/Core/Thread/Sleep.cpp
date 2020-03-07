#include "../Sys/Platform.h"
#include "Sleep.h"
#include "../Sys/Assert.h"

#if defined(LETHE_OS_WINDOWS)
#	include <windows.h>
#	undef min
#	undef max
#else
#	include <time.h>
#	include <sys/time.h>
#	include <pthread.h>
#	include <unistd.h>
#	include <sched.h>
#	include "../Time/Timer.h"
#endif

namespace lethe
{
namespace Thread
{

// sleep in ms
void Sleep(int ms)
{
#if defined(LETHE_OS_WINDOWS)
	::Sleep(ms);
#else
	LETHE_ASSERT(ms >= 0);
	StopWatch sw;
	sw.Start();

	Int delta;

	while ((delta = sw.Get()) < ms)
		::usleep((ms - delta > 1000) ? 1000000 : (ms - delta)*1000);

#endif
}

}
}
