#pragma once

#include "ScriptBaseObject.h"

namespace lethe
{

// script class instance ptr helpers (strong/weak)

class SPtr
{
	ScriptBaseObject *pointer;

public:
	inline SPtr()
		: pointer(nullptr)
	{
	}

	inline SPtr(ScriptBaseObject *o)
		: pointer(nullptr)
	{
		o->AddRef();
		pointer = o;
	}

	inline SPtr(const SPtr &o)
		: pointer(nullptr)
	{
		if (o.pointer)
		{
			o.pointer->AddRef();
			pointer = o.pointer;
		}
	}

	~SPtr()
	{
		if (pointer)
			pointer->Release();
	}

	inline ScriptBaseObject *Get() const
	{
		return pointer;
	}

	ScriptBaseObject *Detach()
	{
		ScriptBaseObject *res = pointer;
		pointer = nullptr;
		return res;
	}

	inline void Clear()
	{
		if (pointer)
			pointer->Release();

		pointer = nullptr;
	}

	inline bool IsEmpty() const
	{
		return !pointer;
	}

	inline operator ScriptBaseObject *() const
	{
		return pointer;
	}

	inline ScriptBaseObject * operator *() const
	{
		return pointer;
	}

	inline ScriptBaseObject *operator ->() const
	{
		return pointer;
	}

	SPtr &operator =(ScriptBaseObject * o)
	{
		if (o == pointer)
			return *this;

		if (o)
			o->AddRef();

		if (pointer)
			pointer->Release();

		pointer = o;

		return *this;
	}

	SPtr &operator =(const SPtr &o)
	{
		return *this = o.pointer;
	}

	void SwapWith(SPtr &o)
	{
		Swap(pointer, o.pointer);
	}

	friend UInt Hash(const SPtr &p)
	{
		return UInt(UIntPtr(p.pointer) & 0xffffffffu);
	}
};

// weak pointer
class WPtr
{
	mutable ScriptBaseObject *pointer;

public:
	inline WPtr()
		: pointer(nullptr)
	{
	}

	inline WPtr(ScriptBaseObject *o)
		: pointer(nullptr)
	{
		if (o && o->HasStrongRef())
		{
			o->AddWeakRef();
			pointer = o;
		}
	}

	inline WPtr(const WPtr &o)
		: pointer(nullptr)
	{
		*this = o;
	}


	inline WPtr(const SPtr &o)
		: pointer(nullptr)
	{
		*this = o;
	}

	~WPtr()
	{
		if (pointer)
			pointer->ReleaseWeak();
	}

	inline void Clear()
	{
		if (pointer)
			pointer->ReleaseWeak();

		pointer = nullptr;
	}

	inline bool IsEmpty() const
	{
		return pointer == nullptr || !pointer->HasStrongRef();
	}

	SPtr Lock() const
	{
		if (pointer && !pointer->HasStrongRef())
		{
			pointer->ReleaseWeak();
			pointer = nullptr;
		}

		return SPtr(pointer);
	}

	WPtr &operator =(ScriptBaseObject * o)
	{
		if (o == pointer)
			return *this;

		if (pointer)
			pointer->ReleaseWeak();

		pointer = o && o->HasStrongRef() ? o : nullptr;

		if (pointer)
			pointer->AddWeakRef();

		return *this;
	}

	WPtr &operator =(const WPtr &o)
	{
		return *this = o.Lock();
	}

	WPtr &operator =(const SPtr &o)
	{
		if (pointer == o.Get())
			return *this;

		if (pointer)
			pointer->ReleaseWeak();

		pointer = o.Get();

		if (pointer)
			pointer->AddWeakRef();

		return *this;
	}

	bool operator ==(const ScriptBaseObject *o) const
	{
		return Lock() == o;
	}

	bool operator ==(const WPtr &o) const
	{
		return Lock() == o.Lock();
	}

	friend bool operator ==(const ScriptBaseObject *o, const WPtr &a)
	{
		return a.Lock() == o;
	}

	void SwapWith(WPtr &o)
	{
		Swap(pointer, o.pointer);
	}

	friend UInt Hash(const WPtr &p)
	{
		return UInt(UIntPtr(p.pointer) & 0xffffffffu);
	}
};

}
