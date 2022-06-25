#pragma once

#include "../Sys/Assert.h"
#include "../Sys/Platform.h"
#include "../Memory/Memory.h"
#include "../Ptr/RefCounted.h"
#include "../Math/Templates.h"

namespace lethe
{

class LETHE_API DelegateBase
{
protected:
	struct LambdaStorageBase : public RefCounted
	{
		// pure virtual causes problems with gcc without RTTI it seems
		virtual LambdaStorageBase *Clone() {return nullptr;};
		virtual ~LambdaStorageBase() {}
	};

	class DummyClass;
	typedef int (DummyClass::*BPMem)();		// pointer to member function
	BPMem pmem;
	const void *classptr;					// class pointer
	LambdaStorageBase *lambdaStorage;		// lambda storage

	LETHE_NOINLINE void FreeStorage()
	{
		if (lambdaStorage && static_cast<void *>(lambdaStorage) != this)
		{
			lambdaStorage->Release();
			lambdaStorage = nullptr;
		}
	}
public:
	LETHE_NOINLINE DelegateBase()
		: pmem(nullptr)
		, classptr(nullptr)
		, lambdaStorage(nullptr)
	{
		MemSet(&pmem, 0, sizeof(pmem));
	}

	LETHE_NOINLINE DelegateBase(const DelegateBase &o)
		: pmem(nullptr)
		, classptr(nullptr)
		, lambdaStorage(nullptr)
	{
		MemSet(&pmem, 0, sizeof(pmem));
		*this = o;
	}

	~DelegateBase()
	{
		FreeStorage();
	}

	LETHE_NOINLINE DelegateBase &operator =(const DelegateBase &o)
	{
		if (&o == this)
			return *this;

		FreeStorage();
		MemCpy(&pmem, &o.pmem, sizeof(pmem));
		lambdaStorage = o.lambdaStorage ? reinterpret_cast<LambdaStorageBase *>(this) : nullptr;

		if (o.lambdaStorage && static_cast<void *>(o.lambdaStorage) != &o)
			lambdaStorage = o.lambdaStorage->Clone();

		classptr = o.classptr;

		if (o.classptr == &o)
			classptr = this;

		if (o.classptr == o.lambdaStorage)
			classptr = lambdaStorage;

		return *this;
	}

	// null test
	inline bool IsEmpty() const
	{
		return !lambdaStorage;
	}

	inline operator bool() const
	{
		return !IsEmpty();
	}

	// lt test to allow STL sorting
	bool operator <(const DelegateBase &o) const
	{
		if (classptr != o.classptr)
			return classptr < o.classptr;

		return MemCmp(&pmem, &o.pmem, sizeof(pmem)) < 0;
	}

	// equality test
	// sigh, this trick for storing any pointer to member seems broken in VS15.8, storing some additional junk here
	// so I have to compare the original delegate!
	// FIXME: could I do better here?
	bool operator ==(const DelegateBase &o) const
	{
		return classptr == o.classptr && MemCmp(&pmem, &o.pmem, sizeof(pmem)) == 0;
	}

	// inequality test
	bool operator !=(const DelegateBase &o) const
	{
		return !(*this == o);
	}
};

#include "Inline/Delegate11.inl"

}
