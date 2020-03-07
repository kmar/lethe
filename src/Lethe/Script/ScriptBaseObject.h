#pragma once

#include "../Core/Common.h"
#include "../Core/Sys/Types.h"

namespace lethe
{

class DataType;

}

namespace lethe
{

#ifdef LETHE_CUSTOM_BASE_OBJECT

using ScriptBaseObject = LETHE_CUSTOM_BASE_OBJECT;

#else

class LETHE_API ScriptBaseObject
{
public:
	// FIXME: nonportable!
	static const Int OFS_VTBL = sizeof(void *);
	static const Int OFS_REFC = OFS_VTBL + sizeof(void *);

	inline ScriptBaseObject()
		: scriptVtbl(nullptr)
		, strongRefCount(0)
		, weakRefCount(1)
	{
	}

	inline virtual ~ScriptBaseObject() {}

	// class type is disguised in vtbl as -1st pointer
	void **scriptVtbl;
	mutable AtomicUInt strongRefCount;
	mutable AtomicUInt weakRefCount;

	inline const lethe::DataType *GetScriptClassType() const
	{
		if (!scriptVtbl)
			return nullptr;

		return ((const lethe::DataType **)(scriptVtbl))[-1];
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
};

#endif

struct ScriptDelegateBase
{
	void *instancePtr = nullptr;
	void *funcPtr = nullptr;
};

}
