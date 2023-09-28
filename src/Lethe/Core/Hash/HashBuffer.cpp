#include "HashBuffer.h"
#include "../Sys/Endian.h"
#include "../Sys/Assert.h"
#include "../Sys/Platform.h"

#if LETHE_COMPILER_MSC
#	include <intrin.h>
#endif

namespace lethe
{

// my fast hash, based on excellent MurmurHash3 (x86_32) by Austin Appleby

// "random" keys
// primes are essential for multiplicative keys
static const UInt KEY1 = 0xdb91908du;	// prime
static const UInt KEY2 = 0x6be5be6fu;	// prime
static const UInt KEY3 = 0x53a28fc9u;	// prime

// rotate right
static inline UInt Rotate(UInt u, Byte amount)
{
	LETHE_ASSERT(amount && amount < 32);
#if LETHE_COMPILER_MSC
	return _rotr(u, amount);
#else
	return (u >> amount) | (u << (Byte)(32-amount));
#endif
}

static inline UInt MyHashUInt(UInt u, UInt h)
{
	u = KEY2 * Rotate(u*KEY1, 17);
	return 3 * Rotate(h - (u^KEY3), 16);
}

template< typename T >
static inline UInt MyHash(const void *buf, size_t len, UInt seed = 0)
{
	const Byte *b = static_cast<const Byte *>(buf);
	const Int tsize = sizeof(T);
	LETHE_ASSERT(len < tsize || !((UIntPtr)b & (tsize-1)));
	const T *u = static_cast<const T *>(buf);
	size_t ulen = len/4;

	UInt h = KEY3 ^ seed;

	b += ulen*4;
	len &= 3;

	while (ulen--)
	{
		h = MyHashUInt(Endian::ReadUInt(u), h);
		u += 4/tsize;
	}

	// process remaining data if any:
	UInt last = 0;

	switch(len)
	{
	case 3:
		last = (UInt)b[2] << 16;
		// fall through
	case 2:
		last |= (UInt)b[1] << 8;
		// fall through
	case 1:
		last |= (UInt)b[0];
		last ^= len * KEY3;
		h = MyHashUInt(last, h);
		// fall through
	default:
		break;
	}

	// finalize (avalanche)
	h ^= h >> 15;
	h *= KEY1;
	h ^= h >> 16;
	h *= KEY2;
	h ^= h >> 17;
	return h;
}

UInt HashUInt(UInt v)
{
	auto h = v;
	h ^= h >> 15;
	h *= KEY1;
	h ^= h >> 16;
	h *= KEY2;
	h ^= h >> 17;
	return h;
}

UInt HashULong(ULong v)
{
	auto h = v;
	h ^= h >> 15;
	h *= KEY1;
	h ^= h >> 16;
	h *= KEY2;
	h ^= h >> 17;
	return (UInt)h;
}

UInt HashBuffer(const void *buf, size_t sz)
{
	return MyHash<UInt>(buf, sz);
}

UInt HashBufferWordAligned(const void *buf, size_t sz)
{
	return !((UIntPtr)buf & 3) ? MyHash<UInt>(buf, sz) : MyHash<UShort>(buf, sz);
}

UInt HashBufferUnaligned(const void *buf, size_t sz)
{
	return !((UIntPtr)buf & 3) ? MyHash<UInt>(buf, sz) :
		   (!((UIntPtr)buf & 1) ? MyHash<UShort>(buf, sz) : MyHash<Byte>(buf, sz));
}

// merge two hashes for incremental hashing
UInt HashMerge(UInt h1, UInt h2)
{
	return (Rotate(h1, 9) * 7) ^ h2;
}

}
