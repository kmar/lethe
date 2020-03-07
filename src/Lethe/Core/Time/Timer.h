#pragma once

#include "../Sys/Types.h"

namespace lethe
{

class StopWatch;
class PerfWatch;

struct LETHE_API Timer
{
	friend class StopWatch;
	friend class PerfWatch;

	static void Init();
	static void Done();

	// these return simple monotonic counters (low frequency, should be around 1msec resolution)
	// resolution is not guaranteed; no units here
	// 64-bit counter may not cover full 64-bit range and may be emulated with 32-bit counter
	static UInt GetCounter();
	static ULong GetCounter64();
	// get monotonic counter frequency (=ticks per second)
	static UInt GetCounterFreq();
	static ULong GetCounterFreq64();

	// returns 1 if GetCounter64 has full 64-bit resolution
	static bool Has64BitResolution();
	// returns 1 if high precision counter has 64-bit resolution
	static inline bool HasHi64BitResolution()
	{
		return hiTimerAvail;
	}

	// high precision, high frequency counter (wraps to GetCounter64 if not available)
	// FIXME: const-protect?
	static inline ULong GetHiCounter()
	{
		return GetHiCounterFunc();
	}
	static inline ULong GetHiCounterFreq()
	{
		return hiTimerFreq;
	}
private:
	// get millisecond counter
	static Int GetMillisec();
	// high precision timer
	static ULong (*GetHiCounterFunc)();
	static ULong hiTimerFreq;
	static bool hiTimerAvail;

	static inline ULong GetHiCounterInternal();
	static inline ULong GetHiCounterFallback();
};

// millisecond resolution stop watch
class LETHE_API StopWatch
{
	Int startTicks;
	Int deltaTicks;
	bool running;
public:
	StopWatch();
	// start stopwatch
	void Start();
	// return delta time in ms but continue
	Int Continue();
	// stop, return delta time in ms
	Int Stop();
	// get delta time in ms
	Int Get() const;
};

// microsecond resolution performance stop watch (if available)
class LETHE_API PerfWatch
{
	ULong startTicks;
	Long deltaTicks;
	bool running;
	static Long GetDelta(ULong s, ULong e);
public:
	PerfWatch();
	// start stopwatch
	void Start();
	// return delta time in us but continue
	Long Continue();
	// stop, return delta time in us
	Long Stop();
	// get delta time in us
	Long Get() const;
};

}
