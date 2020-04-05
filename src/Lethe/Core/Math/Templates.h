#pragma once

#include "../Sys/Types.h"
#include "../Sys/Assert.h"
#include "../Hash/HashBuffer.h"
#include "../Memory/Memory.h"

// useful templates

namespace lethe
{

// fast clamp number to 0...n
template< typename T> static inline T MaxZero(const T &x)
{
	return x * (0 < x);
}

// minimum value
template< typename T > static inline T Min(const T &x, const T &y)
{
	return x < y ? x : y;
}

// maximum value
template< typename T > static inline T Max(const T &x, const T &y)
{
	return y < x ? x : y;
}

// average value
template< typename T > static inline T Avg(const T &x, const T &y)
{
	return (x+y)/2;
}

// clamp value
template< typename T> static inline T Clamp(const T &value, const T &minv, const T &maxv)
{
	return Min(maxv, Max(value, minv));
}

// saturate (clamp to 0..1)
template< typename T>
static inline T Saturate(const T &value)
{
	return Clamp(value, (T)0, (T)1);
}

// lerp value
template< typename T > static inline T Lerp(const T &x, const T &y, Float alpha)
{
	return x + (y-x)*alpha;
}

// lerp three values
template< typename T > static inline T Lerp(const T &x, const T &y, const T &z, Float alpha, Float beta)
{
	return x + (y-x)*alpha + (z-x)*beta;
}

// absolute value
template< typename T > static inline T Abs(const T &x)
{
	return x < static_cast<T>(0) ? -x : x;
}

// squared value
template< typename T > static inline T Sqr(const T &x)
{
	return x*x;
}

// returns size of static array in elements
template< size_t N, typename T > static inline constexpr size_t ArraySize(T (&)[N])
{
	return N;
}

// get alignment of native type
template< typename T > struct AlignOf
{
#if LETHE_COMPILER_MSC
#	pragma warning(push)
#	pragma warning(disable: 4324) // alignment (padding)
#	pragma warning(disable: 4510) // default constructor could not be generated
#	pragma warning(disable: 4610) // s can never be instantiated
#endif
	struct s
	{
		char c;
		T t;
	};
	static const size_t align = sizeof(s) - sizeof(T);
#if LETHE_COMPILER_MSC
#	pragma warning(pop)
#endif
};

// returns true if val is power of two
template< typename T > static inline bool IsPowerOfTwo(const T &val)
{
	return (val & (val-1)) == 0;
}

// align pointer to a power of two
static inline void *AlignPtr(void *ptr, UInt align)
{
	LETHE_ASSERT(IsPowerOfTwo(align));
	return (void *)(((UIntPtr)ptr + align-1) & ~(UIntPtr)(align-1));
}

// static cast wrapper
template< typename T, typename U > static inline T Cast(const U &val)
{
	return static_cast<T>(val);
}

// construct range of objects
template< typename T, typename S > static inline void ConstructObjectRange(T *ptr, S size = 1)
{
	while (size > 0)
	{
		LETHE_ASSERT(ptr);
		::new(ptr) T;
		size--;
		ptr++;
	}
}

// destroy range of objects
template< typename T, typename S > static inline void DestroyObjectRange(T *ptr, S size = 1)
{
	while (size > 0)
	{
		LETHE_ASSERT(ptr);
		ptr->~T();
#if LETHE_DEBUG
		// invalidate!!
		Byte *t = reinterpret_cast<Byte *>(ptr);

		for (size_t i=0; i<sizeof(T); i++)
		{
			t[i] = (i & 1) ? (Byte)0xfe : (Byte)0xee;		// FIXME: UB
		}

#endif
		size--;
		ptr++;
	}
}

// compare template
template< typename T > static inline int Compare(const T &x, const T &y)
{
	return (y<x) - (x<y);
}

// equal template
template< typename T > static inline bool Equal(const T &x, const T &y)
{
	return Compare(x, y) == 0;
}

// note that all other (except equal and compare) need not be defined for classes

// unequal template
template< typename T > static inline bool Unequal(const T &x, const T &y)
{
	return !Equal(x, y);
}

// less template
template< typename T > static inline bool Less(const T &x, const T &y)
{
	return x < y;
}

// greater template
template< typename T > static inline bool Greater(const T &x, const T &y)
{
	return y < x;
}

// less or equal template
template< typename T > static inline bool LessEqual(const T &x, const T &y)
{
	return !Greater(x, y);
}

// greater or equal template
template< typename T > static inline bool GreaterEqual(const T &x, const T &y)
{
	return !Less(x, y);
}

// predicates
template< typename T > struct ComparePredicate
{
	inline int operator()(const T &x, const T &y) const
	{
		return Compare(x, y);
	}
};

template< typename T > struct EqualPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return Equal(x, y);
	}
};

template< typename T > struct UnequalPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return Unequal(x, y);
	}
};

template< typename T > struct LessPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return Less(x, y);
	}
};

template< typename T > struct LessIndirectPredicate
{
	inline bool operator()(const T *x, const T *y) const
	{
		return Less(*x, *y);
	}
};

template< typename T > struct GreaterPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return Greater(x, y);
	}
};

template< typename T > struct LessEqualPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return LessEqual(x, y);
	}
};

template< typename T > struct GreaterEqualPredicate
{
	inline bool operator()(const T &x, const T &y) const
	{
		return GreaterEqual(x, y);
	}
};

template< typename T > UInt Hash(const T &x)
{
	return HashBuffer(&x, sizeof(x));
}

// overloads to support literal string hashing
// potentially dangerous IF we want to actually hash a char pointer (shouldn't be common though)
#if LETHE_CPP11
template<typename T, size_t len>
inline UInt Hash(const T (&str)[len])
{
	return HashAnsiString<len>(str);
}

inline UInt Hash(Int v)
{
	return HashUInt(v);
}

inline UInt Hash(UInt v)
{
	return HashUInt(v);
}

inline UInt Hash(Long v)
{
	return HashULong(v);
}

inline UInt Hash(ULong v)
{
	return HashULong(v);
}

#else
inline UInt Hash(const char *str)
{
	return HashAnsiString(str);
}
#endif

template < bool cond, typename T = void >
struct EnableIf
{
};

template< typename T >
struct EnableIf<1, T>
{
	typedef T type;
};

// lower bound (points to >= val) in sorted array
// this comes from http://www.cplusplus.com/reference/algorithm/lower_bound/ - too lazy
template <typename I, typename T>
I LowerBound(I from, const I &to, const T &key)
{
	I ci;
	Int count = (Int)(to - from);

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (*ci < key)
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

// upper bound (points to first > val) in sorted array
template <typename I, typename T>
I UpperBound(I from, const I &to, const T &key)
{
	I ci;
	Int count = (Int)(to - from);

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (!(key < *ci))
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

template <typename I, typename T>
bool BinarySearch(I from, const I &to, const T &key)
{
	from = LowerBound(from, to, key);
	return from != to && !(key < *from);
}

template <typename I, typename T, typename C>
I LowerBound(I from, const I &to, const T &key, const C &cmp)
{
	I ci;
	Int count = (Int)(to - from);

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (cmp(*ci, key))
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

// upper bound (points to first > val) in sorted array
template <typename I, typename T, typename C>
I UpperBound(I from, const I &to, const T &key, const C &cmp)
{
	I ci;
	Int count = (Int)(to - from);

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (!cmp(key, *ci))
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

template <typename I, typename T, typename C>
bool BinarySearch(I from, const I &to, const T &key, const C &cmp)
{
	from = LowerBound(from, to, key, cmp);
	return from != to && !cmp(key, *from);
}

// cmp: predicate; true = belongs first part (to be compatible with std::partition, although I think the condition
// should be reversed
template <typename I, typename C>
I Partition(I from, const I &to, const C &cmp)
{
	while (from != to && cmp(*from))
		++from;

	if (from == to)
		return from;

	I tmp = from;
	++tmp;

	while (tmp != to)
	{
		if (cmp(*tmp))
		{
			Swap(*from, *tmp);
			++from;
		}
		++tmp;
	}

	return from;
}

// note: 2**result may be less that v!
// actually, this is MSBit index
template< typename T > static inline UShort Log2Int(T v)
{
	LETHE_ASSERT(v > 0);
	// FIXME: better! can use bit scan on x86/x64
	UShort res = 0;

	while (v > 0)
	{
		res++;
		v >>= 1;
	}

	return res-1;
}

// this returns x such that (1 << x) >= v
// don't pass
template< typename T > static inline UShort Log2Size(T v)
{
	UShort res = Log2Int(v);
	return res += ((T)1 << res) < v;
}

// sign function, returns -1 if negative, 0 if zero and 1 if positive
template< typename T > static inline T Sign(T x)
{
	return (T)(0 < x) - (T)(x < 0);
}

// used to check if type has SwapWith method(s)
// reference: http://stackoverflow.com/questions/87372/check-if-a-class-has-a-member-function-of-a-given-signature
// note: doesn't work for derived classes!
template< typename T >
struct HasSwap
{
	template< typename U, void (U::*)(U &) > struct SFINAE {};
	template< typename U, U &(U::*)(U &) > struct SFINAE2 {};
	template< typename U > static char Test(SFINAE< U, &U::SwapWith >*);
	template< typename U > static char Test(SFINAE2< U, &U::SwapWith >*);
	template< typename U > static int Test(...);
	static const bool VALUE = sizeof(Test<T>(0)) == sizeof(char);
};

class Stream;

template< typename T >
struct HasSave
{
	template< typename U, bool (U::*)(Stream &) > struct SFINAE {};
	template< typename U, bool (U::*)(Stream &) const > struct SFINAE2 {};
	template< typename U > static char Test(SFINAE< U, &U::Save >*);
	template< typename U > static char Test(SFINAE2< U, &U::Save >*);
	template< typename U > static int Test(...);
	static const bool VALUE = sizeof(Test<T>(0)) == sizeof(char);
};

template< typename T >
static inline void Swap(T &x,
						typename EnableIf< HasSwap<T>::VALUE, T >::type &y)
{
	x.SwapWith(y);
}

template< typename T >
static inline void Swap(T &x,
						typename EnableIf< !HasSwap<T>::VALUE, T >::type &y)
{
	T tmp = x;
	x = y;
	y = tmp;
}

// smart swap-copy, uses Swap if available
template< typename T > static inline void SwapCopy(T &dst, T &src)
{
	if (HasSwap<T>::VALUE)
		Swap(dst, src);
	else
		dst = src;
}

// memcopy traits
template< typename T > struct MemCopyTraits
{
	static const bool VALUE = 0;
};
template<> struct MemCopyTraits<Bool>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Char>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<WChar>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<SByte>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Byte>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Short>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<UShort>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Int>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<UInt>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Long>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<ULong>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Float>
{
	static const bool VALUE = 1;
};
template<> struct MemCopyTraits<Double>
{
	static const bool VALUE = 1;
};

#define LETHE_CAN_MEMCPY(t) namespace lethe { \
	template<> struct MemCopyTraits<t> { \
		static const bool VALUE = 1; \
	}; \
}

template< typename T >
static inline void Reverse(T *ptr, const T *top)
{
	size_t len = (size_t)(top - ptr);

	for (size_t i=0; i<len/2; i++)
		Swap(ptr[i], ptr[len-i-1]);
}

// well defined cast of floating point to unsigned type
// behaves as follows: cast abs value to unsigned, then multiply by -1
// can't do directly because it's UB in C++
// assumes 2's complement, naturally
template<typename Unsignedtype, typename FloatType>
Unsignedtype WellDefinedFloatToUnsigned(FloatType f)
{
	auto sign = Sign(f);
	Unsignedtype res = (Unsignedtype)Abs(f);
	return sign < 0 ? (Unsignedtype)0-res : res;
}

}
