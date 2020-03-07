#include "Timer.h"
#include "../Sys/Platform.h"
#include "../Sys/Assert.h"

// FIXME: gettimeofday is unreliable!!! change it! (really?)

#if LETHE_OS_WINDOWS
#	define USE_TIMEGETTIME			// should be much more accurate, but... there's timeBeginPeriod which is potentially bad
#	include <windows.h>
#	undef min
#	undef max
#	include <time.h>

#	if defined(USE_TIMEGETTIME) && !defined(__GNUC__)
#		pragma comment(lib, "winmm.lib")
#	endif

#else
#	include <time.h>
#	include <sys/time.h>
#endif

#if LETHE_OS_OSX || LETHE_OS_IOS
#	include <mach/mach_time.h>
#endif

namespace lethe
{

// Timer

ULong (*Timer::GetHiCounterFunc)() = Timer::GetHiCounterFallback;
ULong Timer::hiTimerFreq = 1000;
bool Timer::hiTimerAvail = 0;

void Timer::Init()
{
#if LETHE_OS_WINDOWS
	LARGE_INTEGER pf;

	if (QueryPerformanceFrequency(&pf))
	{
		hiTimerFreq = pf.QuadPart;
		GetHiCounterFunc = GetHiCounterInternal;
		hiTimerAvail = 1;
	}

#	if defined(USE_TIMEGETTIME)
	timeBeginPeriod(1);
#	endif
#elif LETHE_OS_LINUX
	hiTimerFreq = 1000000000;
	GetHiCounterFunc = GetHiCounterInternal;
	hiTimerAvail = true;
#elif LETHE_OS_OSX || LETHE_OS_IOS
	mach_timebase_info_data_t tb;
	mach_timebase_info(&tb);

	// num / denom = abs to nanoseconds

	hiTimerFreq = (ULong)(1000000000.0 * (Double)tb.denom / (Double)tb.numer);

	GetHiCounterFunc = GetHiCounterInternal;
	hiTimerAvail = true;
#endif
}

void Timer::Done()
{
#if LETHE_OS_WINDOWS && defined(USE_TIMEGETTIME)
	timeEndPeriod(1);
#endif
}

Int Timer::GetMillisec()
{
#if !LETHE_OS_WINDOWS
	struct timeval tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);

	return (Int)((tp.tv_sec) * 1000 + tp.tv_usec / 1000);
#else

#	ifdef USE_TIMEGETTIME
	return (Int)(timeGetTime() & 0xffffffffU);
#	else
	return (Int)(GetTickCount() & 0xffffffffU);
#	endif

#endif
}

UInt Timer::GetCounter()
{
	return UInt(GetMillisec());
}

ULong Timer::GetCounter64()
{
	return GetCounter();
}

UInt Timer::GetCounterFreq()
{
	return 1000u;
}

ULong Timer::GetCounterFreq64()
{
	return GetCounterFreq();
}

ULong Timer::GetHiCounterFallback()
{
	return GetCounter64();
}

ULong Timer::GetHiCounterInternal()
{
#if LETHE_OS_WINDOWS
	LARGE_INTEGER pc;
	LETHE_VERIFY(QueryPerformanceCounter(&pc));
	return pc.QuadPart;
#elif LETHE_OS_LINUX
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ULong)ts.tv_sec * hiTimerFreq + ts.tv_nsec;
#elif LETHE_OS_OSX || LETHE_OS_IOS
	return mach_absolute_time();
#else
	return GetCounter64();
#endif
}

bool Timer::Has64BitResolution()
{
	return 0;
}

// StopWatch

StopWatch::StopWatch() : deltaTicks(0), running(0)
{
}

// start stopwatch
void StopWatch::Start()
{
	running = 1;
	startTicks = Timer::GetMillisec();
}

// stop, return delta time in ms
Int StopWatch::Stop()
{
	if (running)
	{
		deltaTicks = Timer::GetMillisec() - startTicks;
		running = 0;
	}

	return deltaTicks;
}

// continue, return delta time in ms
Int StopWatch::Continue()
{
	Int current = Timer::GetMillisec();

	if (running)
		deltaTicks = current - startTicks;

	startTicks = current;
	running = 1;
	return deltaTicks;
}

Int StopWatch::Get() const
{
	return running ? Timer::GetMillisec() - startTicks : deltaTicks;
}

// PerfWatch

Long PerfWatch::GetDelta(ULong s, ULong e)
{
	if (LETHE_UNLIKELY(!Timer::hiTimerAvail))
		return Long(Int((UInt)e - (UInt)s))*1000;

	ULong d = e - s;
	// FIXME: overflow check?!
	d *= 1000000u;
	d /= Timer::hiTimerFreq;
	return Long(d);
}

PerfWatch::PerfWatch() : deltaTicks(0), running(0)
{
}

// start PerfWatch
void PerfWatch::Start()
{
	running = 1;
	startTicks = Timer::GetHiCounter();
}

// stop, return delta time in ms
Long PerfWatch::Stop()
{
	if (running)
	{
		deltaTicks = GetDelta(startTicks, Timer::GetHiCounter());
		running = 0;
	}

	return deltaTicks;
}

// continue, return delta time in ms
Long PerfWatch::Continue()
{
	ULong current = Timer::GetHiCounter();

	if (running)
		deltaTicks = GetDelta(startTicks, current);

	startTicks = current;
	running = 1;
	return deltaTicks;
}

Long PerfWatch::Get() const
{
	return running ? GetDelta(startTicks, Timer::GetHiCounter()) : deltaTicks;
}

}
