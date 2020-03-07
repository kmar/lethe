#pragma once

#include "../Sys/Assert.h"

namespace lethe
{

// note: not thread-safe
// ok so newly I use intrusive reference counting only; any object to be managed by SharedPtr
// must either subclass lethe::RefCounted or implement AddRef/Release
template< typename T > class SharedPtr
{
public:
	inline SharedPtr() : ptr(nullptr) {}

	inline SharedPtr(T *t)
	{
		if (t)
			t->AddRef();

		ptr = t;
	}

	inline SharedPtr(const SharedPtr &o)
	{
		if (o.ptr)
		{
			// FIXME: hacking
			const_cast<T *>(o.ptr)->AddRef();
		}

		ptr = o.ptr;
	}

	inline ~SharedPtr()
	{
		if (ptr)
			ptr->Release();
	}

	inline SharedPtr &operator =(const SharedPtr &o)
	{
		// FIXME: hacking
		return *this = const_cast<T *>(o.ptr);
	}

	inline SharedPtr &operator =(T *o)
	{
		if (LETHE_LIKELY(o != ptr))
		{
			Clear();

			if (o)
				o->AddRef();

			ptr = o;
		}

		return *this;
	}

	inline void Clear()
	{
		if (!ptr)
			return;

		T *old = ptr;
		ptr = 0;
		old->Release();
	}

	inline bool IsEmpty() const
	{
		return !ptr;
	}

	inline operator T *() const
	{
		return ptr;
	}

	inline T *operator ->() const
	{
		return ptr;
	}

	T *Detach()
	{
		T *res = ptr;
		ptr = nullptr;
		return res;
	}

	inline T *Get() const
	{
		return ptr;
	}

	inline void SwapWith(SharedPtr &o)
	{
		Swap(ptr, o.ptr);
	}

private:
	T *ptr;
};

}
