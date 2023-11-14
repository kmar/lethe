#include "Lock.h"
#include "../Sys/Platform.h"
#include "Atomic.h"

#if defined(LETHE_OS_WINDOWS)
#	include "../Sys/Windows_Include.h"
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

bool SpinMutex::TryLock(AtomicSByte &value)
{
	return Atomic::CompareAndSwap(value, (SByte)0, (SByte)1);
}

void SpinMutex::Lock(AtomicSByte &value)
{
	while (!Atomic::CompareAndSwap(value, (SByte)0, (SByte)1))
	{
		while (Atomic::Load(value))
			Atomic::Pause();
	}
}

void SpinMutex::Unlock(AtomicSByte &value)
{
	Atomic::Store(value, (SByte)0);
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

Mutex::Mutex(Recursive, int enabled)
{
	enabledFlag = enabled;

	if (!enabled)
		return;

#if LETHE_OS_WINDOWS
	LETHE_COMPILE_ASSERT(LOCK_INTERNAL_SIZE == sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)internal);
#else
	LETHE_COMPILE_ASSERT(LOCK_INTERNAL_SIZE == sizeof(pthread_mutex_t));

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init((pthread_mutex_t *)internal, &attr);
#endif
}

Mutex::Mutex(int enabled)
{
	enabledFlag = enabled;

	if (!enabled)
		return;

#if LETHE_OS_WINDOWS
	LETHE_COMPILE_ASSERT(LOCK_INTERNAL_SIZE == sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)internal);
#else
	LETHE_COMPILE_ASSERT(LOCK_INTERNAL_SIZE == sizeof(pthread_mutex_t));
	pthread_mutex_init((pthread_mutex_t *)internal, 0);
#endif
}

Mutex::~Mutex()
{
#if defined(LETHE_OS_WINDOWS)

	if (enabledFlag)
		DeleteCriticalSection((LPCRITICAL_SECTION)internal);

#else

	if (enabledFlag)
		pthread_mutex_destroy((pthread_mutex_t *)internal);

#endif
}

bool Mutex::TryLock()
{
#if defined(LETHE_OS_WINDOWS)
	return !enabledFlag || TryEnterCriticalSection((LPCRITICAL_SECTION)internal) != FALSE;
#else
	return !enabledFlag || pthread_mutex_trylock((pthread_mutex_t *)internal) == 0;
#endif
}

void Mutex::Lock()
{
#if defined(LETHE_OS_WINDOWS)

	if (LETHE_LIKELY(enabledFlag))
		EnterCriticalSection((LPCRITICAL_SECTION)internal);

#else

	if (LETHE_LIKELY(enabledFlag))
		pthread_mutex_lock((pthread_mutex_t *)internal);

#endif
}

void Mutex::Unlock()
{
#if defined(LETHE_OS_WINDOWS)

	if (LETHE_LIKELY(enabledFlag))
		LeaveCriticalSection((LPCRITICAL_SECTION)internal);

#else

	if (LETHE_LIKELY(enabledFlag))
		pthread_mutex_unlock((pthread_mutex_t *)internal);

#endif
}

// RWMutex

void RWMutex::LockRead()
{
retry_lock:
	auto tmp = Atomic::Increment(data);

	// since we're locked exclusively AND it the exclusive lock originated from another read lock, we're done here (also: no overflow)
	if ((tmp & (LOCKED_EXCLUSIVE | LOCKED_READ | COUNTER_OVERFLOW)) == (LOCKED_EXCLUSIVE | LOCKED_READ))
		return;

	if (tmp & COUNTER_OVERFLOW)
	{
		// decrement and wait => we should probably abort here, but we bet we can't do 250m from several threads this quickly to overflow multiple times
		// the safe thing would be to abort here, of course, but doing an extra bit of overflow seems like a science fiction - would require CPUs with millions of cores
		// aborting/freezing here would suffer from the same problem though
		Atomic::Decrement(data);
		Atomic::Pause();
		goto retry_lock;
	}

	// best would be compare and swap, probably => the only way, actually
	for(;;)
	{
		tmp = Atomic::Load(data);

		// we're done if someone else is sharing for read and we can happily exit
		if ((tmp & (LOCKED_EXCLUSIVE | LOCKED_READ)) == (LOCKED_EXCLUSIVE | LOCKED_READ))
			return;

		if (!(tmp & (LOCKED_EXCLUSIVE | LOCKED_READ | WANT_EXCLUSIVE)))
		{
			// try compare and swap - we cannot do atomic or here
			if (Atomic::CompareAndSwap(data, tmp, tmp | LOCKED_EXCLUSIVE | LOCKED_READ))
				break;
		}

		Atomic::Pause();
	}
}

void RWMutex::UnlockRead()
{
	if ((Atomic::Decrement(data) & COUNTER_MASK) != 0)
		return;

	for (;;)
	{
		auto tmp = Atomic::Load(data);

		// if someone else incremented the read counter or we're no longer in exclusive read mode,
		// abort since we're too late already
		if ((tmp & (COUNTER_MASK | LOCKED_EXCLUSIVE | LOCKED_READ)) != (LOCKED_EXCLUSIVE | LOCKED_READ))
			break;

		// try compare and swap - we cannot do atomic or here
		if (Atomic::CompareAndSwap(data, tmp, tmp & ~(LOCKED_EXCLUSIVE | LOCKED_READ)))
			break;
	}
}

void RWMutex::LockWrite()
{
	for (;;)
	{
		// attempt to let readers know a prority write lock is desired
		// we don't reuse the result of atomic or here or it would degrade to a cas (msc)
		Atomic::Or(data, WANT_EXCLUSIVE);
		auto tmp = Atomic::Load(data);

		if (!(tmp & LOCKED_EXCLUSIVE))
		{
			// try cas, break if done;
			if (Atomic::CompareAndSwap(data, tmp, (tmp | LOCKED_EXCLUSIVE) & ~WANT_EXCLUSIVE))
				break;
		}

		Atomic::Pause();
	}
}

void RWMutex::UnlockWrite()
{
	for (;;)
	{
		auto tmp = Atomic::Load(data);

		LETHE_ASSERT(!(tmp & LOCKED_READ));

		if (Atomic::CompareAndSwap(data, tmp, tmp & ~LOCKED_EXCLUSIVE))
			break;
	}
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
