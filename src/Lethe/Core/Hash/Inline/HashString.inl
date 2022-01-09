namespace ConstHash
{

// my fast hash, based on excellent MurmurHash3 (x86_32) by Austin Appleby

// "random" keys
// primes are essential for multiplicative keys
static constexpr const UInt CONST_MYHASH_KEY1 = 0xdb91908du;	// prime
static constexpr const UInt CONST_MYHASH_KEY2 = 0x6be5be6fu;	// prime
static constexpr const UInt CONST_MYHASH_KEY3 = 0x53a28fc9u;	// prime

// rotate right
LETHE_FORCEINLINE constexpr UInt ConstRotate(UInt u, Byte amount)
{
	return (u >> amount) | (u << (Byte)(32-amount));
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashUInt(UInt u, UInt h)
{
	return 3 * ConstRotate(h - ((CONST_MYHASH_KEY2 * ConstRotate(u*CONST_MYHASH_KEY1, 17))^CONST_MYHASH_KEY3), 16);
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashFinishRest(UInt len, UInt last, UInt h)
{
	return ConstMyHashUInt(last ^ (len * CONST_MYHASH_KEY3), h);
}

template<UInt len>
LETHE_FORCEINLINE constexpr UInt ConstMyHashFinish(const Byte *buf, UInt h)
{
	return
		len == 3 ?
		ConstMyHashFinishRest(3,
							  ((UInt)buf[2] << 16) +
							  ((UInt)buf[1] << 8) +
							  (UInt)buf[0], h
							 ) :
		(len == 2 ?
		 ConstMyHashFinishRest(2,
							   ((UInt)buf[1] << 8) +
							   (UInt)buf[0], h
							  ) :
		 (len == 1 ?
		  ConstMyHashFinishRest(1,
								(UInt)buf[0], h
							   ) : h));
}

template<>
LETHE_FORCEINLINE constexpr UInt ConstMyHashFinish<0>(const Byte *, UInt h)
{
	return h;
}

template<UInt len>
LETHE_FORCEINLINE constexpr UInt ConstMyHashInner(const Byte *buf, UInt h)
{
	return len >= 4 ?
		   ConstMyHashInner<len-4>(buf + 4,
			Endian::IsLittle() ?
				   ConstMyHashUInt(
					   (UInt)buf[0] +
					   ((UInt)buf[1] << 8) +
					   ((UInt)buf[2] << 16) +
					   ((UInt)buf[3] << 24),
					   h
				   )
			:
				   ConstMyHashUInt(
					   (UInt)buf[3] +
					   ((UInt)buf[2] << 8) +
					   ((UInt)buf[1] << 16) +
					   ((UInt)buf[0] << 24),
					   h
				   )
		   )
		   :
		   ConstMyHashFinish<len>(buf, h);
}

template<>
LETHE_FORCEINLINE constexpr UInt ConstMyHashInner<(UInt)0>(const Byte *, UInt h)
{
	return h;
}

template<>
LETHE_FORCEINLINE constexpr UInt ConstMyHashInner<(UInt)-1>(const Byte *, UInt h)
{
	return h;
}

template<>
LETHE_FORCEINLINE constexpr UInt ConstMyHashInner<(UInt)-2>(const Byte *, UInt h)
{
	return h;
}

template<>
LETHE_FORCEINLINE constexpr UInt ConstMyHashInner<(UInt)-3>(const Byte *, UInt h)
{
	return h;
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashFinalize0(UInt h)
{
	return (h ^ (h >> 15)) * CONST_MYHASH_KEY1;
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashFinalize1(UInt h)
{
	return (h ^ (h >> 16)) * CONST_MYHASH_KEY2;
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashFinalize2(UInt h)
{
	return h ^ (h >> 17);
}

LETHE_FORCEINLINE constexpr UInt ConstMyHashFinalize(UInt h)
{
	// finalize (avalanche)
	return ConstMyHashFinalize2(ConstMyHashFinalize1(ConstMyHashFinalize0(h)));
}

template<size_t len>
LETHE_FORCEINLINE constexpr UInt ConstMyHash(const Byte *buf, UInt seed = 0)
{
	return ConstMyHashFinalize(ConstMyHashInner<(UInt)len>(buf, CONST_MYHASH_KEY3 ^ seed));
}

}

template<size_t len>
static LETHE_FORCEINLINE constexpr UInt HashAnsiString(const char (&str)[len])
{
	return ConstHash::ConstMyHash<len-1>(reinterpret_cast<const Byte *>(static_cast<const char *>(str)));
}
