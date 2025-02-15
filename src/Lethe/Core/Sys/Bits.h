#pragma once

#include "Endian.h"

// Bits: bit twiddling routines

namespace lethe
{

struct Bits
{
	// get mask for low n bits (0 = none)
	template< typename T > static inline T GetLowMask(Byte bits)
	{
		LETHE_ASSERT(bits <= 8*sizeof(T));
		return (bits >= 8*sizeof(T)) ? ~(T)0 : ((T)1 << bits)-1;
	}

	// generic bit reversal
	// make sure to use only unsigned integral types here!
	template< typename T > static inline void Reverse(T &x)
	{
		T tmp = 0;

		for (size_t i=0; i<sizeof(T)*8; i++)
		{
			tmp <<= 1;
			tmp |= (x & ((T)1 << i)) != 0;
		}

		x = tmp;
	}

	// bit-size version
	template< typename T > static inline void Reverse(T &x, Byte bits)
	{
		LETHE_ASSERT(bits > 0 && !(x & ~GetLowMask<T>(bits)));
		Reverse(x);
		x >>= 8*sizeof(T) - bits;
	}

	// bit scan
	static inline int GetMsb32(UInt ui)
	{
		LETHE_ASSERT(ui);
#if LETHE_COMPILER_MSC_ONLY
		unsigned long index;
		LETHE_VERIFY(_BitScanReverse(&index, ui));
#else
		int index = 8*sizeof(UInt) - 1 - __builtin_clz(ui);
#endif
		return (int)index;
	}

#if LETHE_64BIT
	static inline int GetMsb64(ULong ui)
	{
		LETHE_ASSERT(ui);
#if LETHE_COMPILER_MSC_ONLY
		unsigned long index;
		LETHE_VERIFY(_BitScanReverse64(&index, ui));
#else
		int index = 8*sizeof(ULong) - 1 - __builtin_clzll(ui);
#endif
		return (int)index;
	}
#else
	static inline int GetMsb64(ULong ui)
	{
		UInt up = (UInt)(ui>>32);
		return up ? (32 + GetMsb32(up)) : GetMsb32((UInt)ui);
	}
#endif

	static inline int GetLsb32(UInt ui)
	{
		LETHE_ASSERT(ui);
#if LETHE_COMPILER_MSC_ONLY
		unsigned long index;
		LETHE_VERIFY(_BitScanForward(&index, ui));
#else
		int index = __builtin_ctz(ui);
#endif
		return (int)index;
	}

	template< typename T >
	static inline int GetMsb(T ui)
	{
		LETHE_COMPILE_ASSERT(sizeof(T) <= sizeof(ULong));
		return sizeof(T) <= sizeof(UInt) ? GetMsb32((UInt)ui) : GetMsb64(ui);
	}

#if LETHE_64BIT
	static inline int GetLsb64(ULong ui)
	{
		LETHE_ASSERT(ui);
#if LETHE_COMPILER_MSC_ONLY
		unsigned long index;
		LETHE_VERIFY(_BitScanForward64(&index, ui));
#else
		int index = __builtin_ctzll(ui);
#endif
		return (int)index;
	}
#else
	static inline int GetLsb64(ULong ui)
	{
		UInt dn = (UInt)(ui);
		return dn ? (GetLsb32(dn)) : 32+GetLsb32((UInt)(ui >> 32));
	}
#endif

	template< typename T >
	static inline int GetLsb(T ui)
	{
		LETHE_COMPILE_ASSERT(sizeof(T) <= sizeof(ULong));
		return sizeof(T) <= sizeof(UInt) ? GetLsb32((UInt)ui) : GetLsb64(ui);
	}

	template< typename T >
	static inline int PopLsb(T &ui)
	{
		int res = GetLsb(ui);
		ui &= ui - 1u;
		return res;
	}

	template< typename T >
	static inline T IsolateLsb(T ui)
	{
		return ui & ((T)0 - ui);
	}

	template<typename T>
	static inline int PopCount(T val);
};

// with the help of http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv - too lazy

template<> inline void Bits::Reverse<Byte>(Byte &x)
{
	// swap odd and even bits
	x = ((x >> 1) & 0x55u) | ((x & 0x55u) << 1);
	// swap consecutive pairs
	x = ((x >> 2) & 0x33u) | ((x & 0x33u) << 2);
	// swap nibbles
	x = ((x >> 4) & 0x0fu) | ((x & 0x0fu) << 4);
}

template<> inline void Bits::Reverse<UShort>(UShort &x)
{
	// swap odd and even bits
	x = ((x >> 1) & 0x5555u) | ((x & 0x5555u) << 1);
	// swap consecutive pairs
	x = ((x >> 2) & 0x3333u) | ((x & 0x3333u) << 2);
	// swap nibbles
	x = ((x >> 4) & 0x0f0fu) | ((x & 0x0f0fu) << 4);
	// swap bytes
	Endian::ByteSwap(x);
}

template<> inline void Bits::Reverse<UInt>(UInt &x)
{
	// swap odd and even bits
	x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
	// swap consecutive pairs
	x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
	// swap nibbles
	x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
	// swap bytes
	Endian::ByteSwap(x);
}

template<> inline void Bits::Reverse<ULong>(ULong &x)
{
	// swap odd and even bits
	x = ((x >> 1) & LETHE_CONST_ULONG(0x5555555555555555)) | ((x & LETHE_CONST_ULONG(0x5555555555555555)) << 1);
	// swap consecutive pairs
	x = ((x >> 2) & LETHE_CONST_ULONG(0x3333333333333333)) | ((x & LETHE_CONST_ULONG(0x3333333333333333)) << 2);
	// swap nibbles
	x = ((x >> 4) & LETHE_CONST_ULONG(0x0f0f0f0f0f0f0f0f)) | ((x & LETHE_CONST_ULONG(0x0f0f0f0f0f0f0f0f)) << 4);
	// swap bytes
	Endian::ByteSwap(x);
}

template<typename T>
inline Int Bits::PopCount(T x)
{
	// TODO: better!
	Int res = 0;

	while (x)
	{
		PopLsb(x);
		++res;
	}

	return res;
}

}
