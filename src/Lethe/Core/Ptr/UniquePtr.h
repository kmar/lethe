#pragma once

namespace lethe
{

template< typename T >
class UniquePtr
{
public:
	inline UniquePtr() : ptr(0) {}
	inline UniquePtr(const UniquePtr &o)
	{
		ptr = const_cast<T *>(o.ptr);
		o.ptr = 0;
	}
	inline UniquePtr(T *v) : ptr(v) {}
	inline ~UniquePtr()
	{
		delete ptr;
	}
	UniquePtr &operator =(T *v)
	{
		if (v != ptr)
		{
			delete ptr;
			ptr = v;
		}

		return *this;
	}
	UniquePtr &operator =(const UniquePtr &o)
	{
		if (&o != this)
		{
			delete ptr;
			ptr = const_cast<T *>(o.ptr);
			o.ptr = 0;
		}

		return *this;
	}
	T *Detach()
	{
		T *res = ptr;
		ptr = 0;
		return res;
	}
	UniquePtr &Clear()
	{
		T *old = ptr;
		ptr = 0;
		delete old;
		return *this;
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

	inline T *Get() const
	{
		return ptr;
	}

	inline void SwapWith(UniquePtr &o)
	{
		Swap(ptr, o.ptr);
	}
private:
	mutable T *ptr;
};

}
