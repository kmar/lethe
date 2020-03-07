#pragma once

#include "../Sys/Assert.h"
#include "../Sys/NoCopy.h"
#include "Atomic.h"

namespace lethe
{

class LETHE_API SpinMutex : NoCopy
{
	AtomicInt lockFlag;
public:

	inline SpinMutex() : lockFlag(0) {}
	~SpinMutex();

	bool TryLock();
	void Lock();
	void Unlock();

	static bool TryLock(AtomicInt &value);
	static void Lock(AtomicInt &value);
	static void Unlock(AtomicInt &value);
};

class LETHE_API Mutex : NoCopy
{
protected:
#if LETHE_OS_WINDOWS
#	if LETHE_64BIT
	static const size_t LOCK_INTERNAL_SIZE = 40;
#	else
	static const size_t LOCK_INTERNAL_SIZE = 24;
#	endif
	ULong internal[(LOCK_INTERNAL_SIZE + sizeof(ULong)-1) / sizeof(ULong)];
#else
#	if LETHE_OS_ANDROID
#		if LETHE_64BIT
	static const size_t LOCK_INTERNAL_SIZE = 40;
#		else
	static const size_t LOCK_INTERNAL_SIZE = 4;
#		endif
#	elif LETHE_OS_OSX || LETHE_OS_IOS
#			if LETHE_64BIT
	static const size_t LOCK_INTERNAL_SIZE = 56 + sizeof(long);
#			else
	static const size_t LOCK_INTERNAL_SIZE = 40 + sizeof(long);
#			endif
#	elif LETHE_OS_BSD
	static const size_t LOCK_INTERNAL_SIZE = sizeof(void *);
#	else
#		if LETHE_CPU_X86
#			if LETHE_64BIT
	static const size_t LOCK_INTERNAL_SIZE = 40;
#			else
	static const size_t LOCK_INTERNAL_SIZE = 32;
#			endif
#		else
	// FIXME: support more platforms later
	static const size_t LOCK_INTERNAL_SIZE = 24;
#		endif
#	endif
	ULong internal[(LOCK_INTERNAL_SIZE + sizeof(ULong)-1) / sizeof(ULong)];
#endif
	int enabledFlag;

public:
	struct Recursive{};

	explicit Mutex(int enabled = 1);
	// recursive variant - avoid at all costs!
	explicit Mutex(Recursive, int enabled = 1);
	~Mutex();

	bool TryLock();
	void Lock();
	void Unlock();
};

// RW mutex, preferring multiple reads and single write
class LETHE_API RWMutex
{
public:
	RWMutex();

	void LockRead();
	void UnlockRead();

	void LockWrite();
	void UnlockWrite();

private:
	Mutex writeMutex;
	SpinMutex readMutex;
	Int counter;
};

class LETHE_API MutexLock
{
protected:
	Mutex *mref;		// mutex refptr
public:
	MutexLock(Mutex &m, bool nolock = 0);
	~MutexLock();
};

class LETHE_API SpinMutexLock
{
protected:
	SpinMutex *mref;	// spinmutex refptr
public:
	SpinMutexLock(SpinMutex &m, bool nolock = 0);
	~SpinMutexLock();
};

class LETHE_API ReadMutexLock
{
protected:
	RWMutex *mref;
public:
	ReadMutexLock(RWMutex &m);
	~ReadMutexLock();
};

class LETHE_API WriteMutexLock
{
protected:
	RWMutex *mref;
public:
	WriteMutexLock(RWMutex &m);
	~WriteMutexLock();
};

class FakeMutex
{
public:
	inline void Lock() {}
	inline void Unlock() {}
};

class FakeMutexLock
{
public:
	inline FakeMutexLock(FakeMutex &, bool nolock = 0)
	{
		(void)nolock;
	}
};

}
