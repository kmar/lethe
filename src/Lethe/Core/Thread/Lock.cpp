#include "Lock.h"
#include "../Sys/Platform.h"

#if defined(LETHE_OS_WINDOWS)
#	include <windows.h>
#	undef min
#	undef max
#else
#	include <pthread.h>
#endif

namespace lethe
{

// SpinMutex

SpinMutex::~SpinMutex()
{
	LETHE_ASSERT(!lockFlag);
}

bool SpinMutex::TryLock(AtomicInt &value)
{
	return Atomic::CompareAndSwap(value, 0, 1);
}

void SpinMutex::Lock(AtomicInt &value)
{
	while (!Atomic::CompareAndSwap(value, 0, 1))
	{
		while (Atomic::Load(value))
			Atomic::Pause();
	}
}

void SpinMutex::Unlock(AtomicInt &value)
{
	Atomic::Store(value, 0);
}

bool SpinMutex::TryLock()
{
	return TryLock(lockFlag);
}

void SpinMutex::Lock()
{
	Lock(lockFlag);
}

void SpinMutex::Unlock()
{
	Unlock(lockFlag);
}

// Mutex

LETHE_SINGLETON_INSTANCE(Mutex)

Mutex::Mutex()
{
#if LETHE_OS_WINDOWS
	handle = malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)handle);
#else
	handle = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init((pthread_mutex_t *)handle, 0);
#endif
}

Mutex::Mutex(Recursive)
{
#if LETHE_OS_WINDOWS
	handle = malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)handle);
#else
	handle = malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init((pthread_mutex_t *)handle, &attr);
#endif
}

Mutex::~Mutex()
{
#if defined(LETHE_OS_WINDOWS)
	DeleteCriticalSection((LPCRITICAL_SECTION)handle);
#else
	pthread_mutex_destroy((pthread_mutex_t *)handle);
#endif

	free(handle);
}

bool Mutex::TryLock()
{
#if defined(LETHE_OS_WINDOWS)
	return TryEnterCriticalSection((LPCRITICAL_SECTION)handle) != FALSE;
#else
	return pthread_mutex_trylock((pthread_mutex_t *)handle) == 0;
#endif
}

void Mutex::Lock()
{
#if defined(LETHE_OS_WINDOWS)
	EnterCriticalSection((LPCRITICAL_SECTION)handle);
#else
	pthread_mutex_lock((pthread_mutex_t *)handle);
#endif
}

void Mutex::Unlock()
{
#if defined(LETHE_OS_WINDOWS)
	LeaveCriticalSection((LPCRITICAL_SECTION)handle);
#else
	pthread_mutex_unlock((pthread_mutex_t *)handle);
#endif
}

// RWMutex

RWMutex::RWMutex()
	: counter(0)
{
}

void RWMutex::LockRead()
{
	SpinMutexLock _(readMutex);

	if (++counter == 1)
		writeMutex.Lock();
}

void RWMutex::UnlockRead()
{
	SpinMutexLock _(readMutex);

	if (--counter == 0)
		writeMutex.Unlock();
}

void RWMutex::LockWrite()
{
	writeMutex.Lock();
}

void RWMutex::UnlockWrite()
{
	writeMutex.Unlock();
}

// MutexLock

MutexLock::MutexLock(Mutex &m, bool nolock) : mref(&m)
{
	if (nolock)
		mref = 0;
	else
		mref->Lock();
}

MutexLock::~MutexLock()
{
	if (mref)
		mref->Unlock();
}

// SpinMutexLock

SpinMutexLock::SpinMutexLock(SpinMutex &m, bool nolock) : mref(&m)
{
	if (nolock)
		mref = 0;
	else
		mref->Lock();
}

SpinMutexLock::~SpinMutexLock()
{
	if (mref)
		mref->Unlock();
}

// ReadMutexLock

ReadMutexLock::ReadMutexLock(RWMutex &m)
	: mref(&m)
{
	mref->LockRead();
}

ReadMutexLock::~ReadMutexLock()
{
	mref->UnlockRead();
}

// WriteMutexLock

WriteMutexLock::WriteMutexLock(RWMutex &m)
	: mref(&m)
{
	mref->LockWrite();
}

WriteMutexLock::~WriteMutexLock()
{
	mref->UnlockWrite();
}

}
