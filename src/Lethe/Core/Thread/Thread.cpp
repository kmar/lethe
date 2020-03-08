#include "../Sys/Platform.h"
#include "Thread.h"

#if defined(LETHE_OS_WINDOWS)
#	include <windows.h>
#	undef min
#	undef max
#	include <time.h>
#	include <process.h>
#	define LETHE_USE_BEGINTHREADEX	1
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

// Thread

#if defined(LETHE_OS_WINDOWS)
static unsigned int
#	if !(LETHE_CPU_AMD64 && LETHE_COMPILER_CLANG)
__stdcall
#endif
ThreadProc(void *param)
{
	static_cast<Thread *>(param)->PrivateStartWork();
	return 0;
}
#else
static void *ThreadProc(void *param)
{
	static_cast<Thread *>(param)->PrivateStartWork();
	return 0;
}
#endif

Thread::Thread()
	: handle(nullptr)
	, killFlag(0)
{
}

// use this instead of dtor!
void Thread::Destroy()
{
}

void Thread::Wait()
{
	if (!handle)
		return;			// not running

	Destroy();
	LETHE_VERIFY(Atomic::Increment(killFlag) != 0);
#if defined(LETHE_OS_WINDOWS)
	WaitForSingleObject((HANDLE)handle, INFINITE);
	CloseHandle((HANDLE)handle);
#else
	void *vp = 0;

	if (pthread_join(*(pthread_t *)handle, &vp) !=  0)
	{
#ifndef __ANDROID__
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
		pthread_cancel(*(pthread_t *)handle);
#endif
	}

	delete (pthread_t *)handle;
#endif
	handle = 0;
	Atomic::Decrement(killFlag);
}

Thread::~Thread()
{
	if (!Atomic::Load(killFlag))
	{
		LETHE_VERIFY(Atomic::Increment(killFlag) != 0);
		Wait();
	}
}

void Thread::Kill()
{
	LETHE_VERIFY(Atomic::Increment(killFlag) != 0);
	Wait();
	delete this;
}

void Thread::PrivateStartWork()
{
	Work();
}

void Thread::Work()
{
	onWork();
}

// create and run thread
bool Thread::Run()
{
	if (handle)
	{
		// already running
		return false;
	}

#if defined(LETHE_OS_WINDOWS)
#	if LETHE_USE_BEGINTHREADEX
	handle = (HANDLE)_beginthreadex(0, 0, ThreadProc, this, 0/*CREATE_SUSPENDED*/, 0);

	if (!handle || handle == INVALID_HANDLE_VALUE)
		return 0;

#	else
	handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0/*CREATE_SUSPENDED*/, 0);

	if (!handle)
		return 0;

#	endif
#else
	handle = new pthread_t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create((pthread_t *)handle, 0 /* attr */, ThreadProc, this);
	pthread_attr_destroy(&attr);
#endif
	return 1;
}

// sleep in ms
void Thread::Sleep(int ms)
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
