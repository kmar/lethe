#pragma once

#include "../Sys/Types.h"
#include "../Sys/Platform.h"

#if LETHE_OS_WINDOWS
#	include <intrin.h>
#endif

namespace lethe
{

template<typename T>
struct LETHE_API AtomicPointer
{
	inline AtomicPointer(T *value = nullptr)
		: data(value)
	{
	}

	inline T *Load() const;

	inline void Store(T *value);

	inline T *Exchange(T *value);

	// note: not thread-safe!
	void SwapWith(AtomicPointer &o)
	{
		auto *tmp = data;
		data = o.data;
		o.data = tmp;
	}

private:
	// copy at your own risk
	volatile T *data;
};

struct LETHE_API Atomic
{
	// atomic increment/decrement/cmpxchg
	// guaranteed to work for Int and UInt

	// generic atomic load function.
	// returns new value (after increment)
	template< typename T > static T Load(const volatile T &t);
	// generic atomic store function.
	// returns new value (after increment)
	template< typename T > static void Store(volatile T &t, T value);
	// generic atomic increment function.
	// returns new value (after increment)
	template< typename T > static T Increment(volatile T &t);
	// generic atomic decrement function.
	// returns new value (after decrement)
	template< typename T > static T Decrement(volatile T &t);
	// generic atomic add function
	// returns new value (after addition)
	template< typename T > static T Add(volatile T &t, T value);
	// generic atomic sub function
	// returns new value (after addition)
	template< typename T > static T Subtract(volatile T &t, T value);
	// bitwise atomics (return old value, unlike add/sub)
	template< typename T > static T Or(volatile T &t, T value);
	// atomic compare and swap
	// cmp = value to compare to
	// xch = value to store
	// returns true if t == cmp
	template< typename T > static bool CompareAndSwap(volatile T &t, T cmp, T xch);
	// pause processor (helps when spinning with HT on)
	static void Pause();
};

#include "Inline/Atomic.inl"

}
