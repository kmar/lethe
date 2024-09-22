#pragma once

#include "../Sys/Assert.h"
#include "../Delegate/Delegate.h"
#include "../Ptr/RefCounted.h"
#include "../Ptr/SharedPtr.h"
#include "Atomic.h"
#include "Lock.h"

namespace lethe
{

class CoreModule;

// platform-independent Thread

LETHE_API_BEGIN

// thread is not run/created until the first call to Run()
class LETHE_API Thread : NoCopy, public RefCounted
{
	void *handle;				// sys-specific handle
protected:
	AtomicUInt killFlag;		// killing flag
	AtomicUInt completedFlag;	// body completed flag
public:
	typedef void *Id;

	Thread();
	// never implement dtor, implement destroy() instead
	virtual ~Thread();

	// use this instead of dtor!
	virtual void Destroy();

	// run thread (first time)
	virtual bool Run();

	// main worker procedure
	// can be overridden (or use onWork delegate)
	virtual void Work();

	// this is the alternative to overriding
	// we don't pass Thread pointer or param because this usually binds to a class member
	Delegate< void() > onWork;

	// wait for thread to terminate (=join)
	void Wait();

	// kill thread (DON'T USE DELETE!!!)
	// also waits for thread to terminate
	void Kill();

	// should terminate? - for polling
	inline bool GetKillFlag() const
	{
		return Atomic::Load(killFlag) != 0;
	}

	// true if thread body has completed
	inline bool HasCompleted() const
	{
		return Atomic::Load(completedFlag) != 0;
	}

	// sleep in ms
	static void Sleep(int ms);

	// PRIVATE!!! don't touch!
	void PrivateStartWork();
};

LETHE_API_END

}
