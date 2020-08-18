#pragma once

#include "../Sys/Assert.h"
#include "../Sys/NoCopy.h"
#include "Atomic.h"
#include "../Sys/Singleton.h"

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
	LETHE_SINGLETON(Mutex)
protected:
	void *handle;

public:
	struct Recursive{};

	explicit Mutex();
	// recursive variant - avoid at all costs!
	explicit Mutex(Recursive);
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
	SpinMutex writeMutex;
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
