#pragma once

#include "../Core/Common.h"
#include "../Core/Sys/Types.h"
#include "../Core/Classes/ObjectHeap.h"
#include "../Core/Thread/Atomic.h"

namespace lethe
{

class DataType;

#ifdef LETHE_CUSTOM_BASE_OBJECT

using ScriptBaseObject = LETHE_CUSTOM_BASE_OBJECT;

#else

class LETHE_API ScriptBaseObject
{
public:
	// FIXME: nonportable!
	static constexpr Int OFS_VTBL = sizeof(void *);
	static constexpr Int OFS_REFC = OFS_VTBL + sizeof(void *);

	inline ScriptBaseObject()
		: scriptVtbl(nullptr)
		, strongRefCount(0)
		, weakRefCount(1)
	{
	}

	// force vtable
	inline virtual ~ScriptBaseObject() {}

	void *operator new(size_t sz)
	{
		return ObjectHeap::Get().Alloc(sz);
	}

	void operator delete(void *ptr)
	{
		return ObjectHeap::Get().Dealloc(ptr);
	}

	// class type is disguised in vtbl as -1st pointer
	void **scriptVtbl;
	mutable AtomicUInt strongRefCount;
	mutable AtomicUInt weakRefCount;

	inline const DataType *GetScriptClassType() const
	{
		if (!scriptVtbl)
			return nullptr;

		return ((const DataType **)(scriptVtbl))[-1];
	}

	// try to destroy using script dtor call; returns true if script object
	inline bool DestroyScriptObject() const
	{
		if (!scriptVtbl)
			return false;

		auto deleter = reinterpret_cast<void (*)(const void *)>(scriptVtbl[-2]);
		deleter(this);
		return true;
	}

	// reference counting support
	UInt AddRef() const
	{
		auto res = Atomic::Increment(strongRefCount);
		LETHE_ASSERT(res);
		return res;
	}

	UInt Release() const
	{
		auto res = DecRefCount();

		if (!res)
		{
			DestroyScriptObject();
			ReleaseWeak();
		}

		return res;
	}

	// careful with this one
	UInt DecRefCount() const
	{
		auto res = Atomic::Decrement(strongRefCount);
		LETHE_ASSERT(res != 0xffffffffu);
		return res;
	}

	UInt AddWeakRef() const
	{
		LETHE_ASSERT(strongRefCount != 0);
		LETHE_ASSERT(weakRefCount != 0);
		auto res = Atomic::Increment(weakRefCount);
		LETHE_ASSERT(res);
		return res;
	}

	UInt ReleaseWeak() const
	{
		auto res = Atomic::Decrement(weakRefCount);
		LETHE_ASSERT(res != 0xffffffffu);

		if (!res)
		{
			LETHE_ASSERT(!HasStrongRef());
			ObjectHeap::Get().Dealloc((void *)this);
		}

		return res;
	}

	// careful here as well
	bool HasStrongRef() const
	{
		return Atomic::Load(strongRefCount) != 0;
	}
};

#endif

struct ScriptDelegateBase
{
	// note: raw pointer
	void *instancePtr = nullptr;
	void *funcPtr = nullptr;
};

}
