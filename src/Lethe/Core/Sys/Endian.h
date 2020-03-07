#pragma once

#include "Types.h"
#include "Platform.h"
#include "Assert.h"

#if LETHE_COMPILER_MSC_ONLY
#	include <intrin.h>
#endif

namespace lethe
{

struct Endian
{
	// generic byte swap (assuming that for 1-byte types compiler will optimize this to nop)
	template< typename T > static inline void ByteSwap(T &v)
	{
		Byte * cv = reinterpret_cast<Byte *>(&v);

		for (size_t i=0; i<sizeof(T)/2; i++)
		{
			Byte tmp = cv[i];
			cv[i] = cv[sizeof(T)-1-i];
			cv[sizeof(T)-1-i] = tmp;
		}
	}

	// returns true if host is little endian
	static inline bool IsLittle()
	{
		UShort x = 1;
		return *reinterpret_cast<const Byte *>(&x) == 1;
	}

	// returns true if host is big endian
	static inline bool IsBig()
	{
		return !IsLittle();
	}

	// generic function to convert from litle endian to host (native)
	template< typename T > static inline void FromLittle(T &v)
	{
		if (IsBig())
			ByteSwap(v);
	}

	// generic function to convert from big endian to host (native)
	template< typename T > static inline void FromBig(T &v)
	{
		if (IsLittle())
			ByteSwap(v);
	}

	// generic function to convert from host (native) to little endian
	template< typename T > static inline void ToLittle(T &v)
	{
		return FromLittle(v);
	}

	// generic function to convert from host (native) to big endian
	template< typename T > static inline void ToBig(T &v)
	{
		return FromBig(v);
	}

	// swap between host (native) and little endian
	template< typename T > static inline void SwapLittle(T &v)
	{
		return FromLittle(v);
	}

	// swap between host (native) and big endian
	template< typename T > static inline void SwapBig(T &v)
	{
		return FromBig(v);
	}

	// (un)aligned fetches
	static inline UShort ReadUShort(const Byte *b)
	{
		return IsLittle() ?
			   (UShort)b[0] | ((UShort)b[1] << 8) :
			   ((UShort)b[0] << 8) | (UShort)b[1];
	}
	static inline UInt ReadUShort(const UShort *u)
	{
		return *u;
	}
	static inline UInt ReadUInt(const Byte *b)
	{
		return IsLittle() ?
			   (UInt)b[0] | ((UInt)b[1] << 8) | ((UInt)b[2] << 16) | ((UInt)b[3] << 24) :
			   ((UInt)b[0] << 24) | ((UInt)b[1] << 16) | ((UInt)b[2] << 8) | (UInt)b[3];
	}
	static inline UInt ReadUInt(const UShort *u)
	{
		return IsLittle() ?
			   (UInt)u[0] | ((UInt)u[1] << 16) :
			   ((UInt)u[0] << 16) | (UInt)u[1];
	}
	static inline UInt ReadUInt(const UInt *u)
	{
		return *u;
	}
	static inline void WriteUInt(Byte *b, UInt val)
	{
		if (IsLittle())
		{
			b[0] = (Byte)val;
			b[1] = (Byte)(val >> 8);
			b[2] = (Byte)(val >> 16);
			b[3] = (Byte)(val >> 24);
		}
		else
		{
			b[0] = (Byte)(val >> 24);
			b[1] = (Byte)(val >> 16);
			b[2] = (Byte)(val >> 8);
			b[3] = (Byte)val;
		}
	}
	static inline void WriteULong(Byte *b, ULong val)
	{
		if (IsLittle())
		{
			b[0] = (Byte)val;
			b[1] = (Byte)(val >> 8);
			b[2] = (Byte)(val >> 16);
			b[3] = (Byte)(val >> 24);
			b[4] = (Byte)(val >> 32);
			b[5] = (Byte)(val >> 40);
			b[6] = (Byte)(val >> 48);
			b[7] = (Byte)(val >> 56);
		}
		else
		{
			b[0] = (Byte)(val >> 56);
			b[1] = (Byte)(val >> 48);
			b[2] = (Byte)(val >> 40);
			b[3] = (Byte)(val >> 32);
			b[4] = (Byte)(val >> 24);
			b[5] = (Byte)(val >> 16);
			b[6] = (Byte)(val >> 8);
			b[7] = (Byte)val;
		}
	}
	static inline void WriteUIntPtr(Byte *b, UIntPtr val)
	{
		if (sizeof(val) == 4)
			WriteUInt(b, (UInt)val);
		else
			WriteULong(b, (ULong)val);
	}
};

template<> inline void Endian::ByteSwap<Byte>(Byte &)
{
}

template<> inline void Endian::ByteSwap<SByte>(SByte &)
{
}

#if LETHE_COMPILER_MSC_ONLY
// FIXME: do these intrinsics require VS2012 compiler? => no problem for me!
// specialized templates for most common elementary types
// assuming Microsoft has sizeof long = 4
template<> inline void Endian::ByteSwap<UShort>(UShort &v)
{
	LETHE_ASSERT(sizeof(UShort) == sizeof(unsigned short));
	v = _byteswap_ushort(v);
}
template<> inline void Endian::ByteSwap<Short>(Short &v)
{
	LETHE_ASSERT(sizeof(Short) == sizeof(unsigned short));
	*(UShort *)(&v) = _byteswap_ushort(*(UShort *)(&v));
}
template<> inline void Endian::ByteSwap<UInt>(UInt &v)
{
	LETHE_ASSERT(sizeof(UInt) == sizeof(unsigned long));
	v = _byteswap_ulong(v);
}
template<> inline void Endian::ByteSwap<Int>(Int &v)
{
	LETHE_ASSERT(sizeof(Int) == sizeof(unsigned long));
	*(UInt *)(&v) = _byteswap_ulong(*(UInt *)(&v));
}
template<> inline void Endian::ByteSwap<Float>(Float &v)
{
	LETHE_ASSERT(sizeof(Float) == sizeof(unsigned long));
	*(UInt *)&v = _byteswap_ulong(*(UInt *)(&v));
}
template<> inline void Endian::ByteSwap<ULong>(ULong &v)
{
	LETHE_ASSERT(sizeof(ULong) == sizeof(unsigned __int64));
	v = _byteswap_uint64(v);
}
template<> inline void Endian::ByteSwap<Long>(Long &v)
{
	LETHE_ASSERT(sizeof(Long) == sizeof(__int64));
	*(ULong *)&v = _byteswap_uint64(*(ULong *)&v);
}
template<> inline void Endian::ByteSwap<Double>(Double &v)
{
	LETHE_ASSERT(sizeof(Double) == sizeof(__int64));
	*(ULong *)&v = _byteswap_uint64(*(ULong *)&v);
}
#else
// specialized templates for most common elementary types
// note __bultin_bswap16 is missing
template<> inline void Endian::ByteSwap<UInt>(UInt &v)
{
	v = __builtin_bswap32(v);
}
template<> inline void Endian::ByteSwap<Int>(Int &v)
{
	*(UInt *)&v = __builtin_bswap32(*(UInt *)&v);
}
template<> inline void Endian::ByteSwap<Float>(Float &v)
{
	LETHE_ASSERT(sizeof(Float) == sizeof(uint32_t));
	*(UInt *)&v = __builtin_bswap32(*(UInt *)&v);
}
template<> inline void Endian::ByteSwap<ULong>(ULong &v)
{
	v = __builtin_bswap64(v);
}
template<> inline void Endian::ByteSwap<Long>(Long &v)
{
	*(ULong *)&v = __builtin_bswap64(*(ULong *)&v);
}
template<> inline void Endian::ByteSwap<Double>(Double &v)
{
	LETHE_ASSERT(sizeof(Double) == sizeof(uint64_t));
	*(ULong *)&v = __builtin_bswap64(*(ULong *)&v);
}
#endif

}
