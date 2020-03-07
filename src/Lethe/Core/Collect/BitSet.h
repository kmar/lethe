#pragma once

#include "Array.h"
#include "../Sys/Bits.h"
#include "../Io/Stream.h"

namespace lethe
{

template< typename T = size_t, typename S = Int, typename A = HAlloc<T> >
class BitSetBase
{
public:
	inline BitSetBase();
	inline explicit BitSetBase(S initialSize);

	inline bool TestBit(S bit) const;
	inline BitSetBase &SetBit(S bit);
	inline BitSetBase &SetBit(S bit, bool value);
	inline BitSetBase &ClearBit(S bit);
	// alias
	inline BitSetBase &ResetBit(S bit);

	inline BitSetBase &FlipBit(S bit);

	// bit scan, starting at idx, -1 if no more non-zero bits found
	S Scan(S idx = 0) const;

	// new: useful stuff: clear/set range and test range
	// range: <from, to), to not included!
	bool TestRange(S from, S to) const;
	BitSetBase &SetRange(S from, S to);
	inline BitSetBase &SetRange(S from, S to, bool value);
	BitSetBase &ClearRange(S from, S to);
	BitSetBase &FlipRange(S from, S to);

	// set-tests
	bool TestAnd(const BitSetBase &o);
	bool TestNAnd(const BitSetBase &o);

	// get size (in bits!)
	inline S GetSize() const;

	// resize
	BitSetBase &Resize(S newSizeBits);

	// clear: clears all bits
	BitSetBase &Clear();
	BitSetBase &Reset();
	BitSetBase &Shrink();

	// in-place logical set ops
	BitSetBase &operator &=(const BitSetBase &o);
	BitSetBase &operator |=(const BitSetBase &o);
	BitSetBase &operator ^=(const BitSetBase &o);

	S GetPopCount() const;
	// alias
	inline S GetBitCount() const;

	// convert to little endian byte array for portability
	const BitSetBase &ToByteArray(Array<Byte, S> &barr) const;
	BitSetBase &FromByteArray(const Array<Byte, S> &barr);

	inline void SwapWith(BitSetBase &o);

	size_t GetMemUsage() const;

private:
	static const Int BIT_MASK = 8*sizeof(T)-1;
	static const Int BIT_DIV = 8*sizeof(T);

	Array< T, S, A > data;
	S size;
};

typedef BitSetBase<size_t, Int> BitSet;
typedef BitSetBase<size_t, Long> BigBitSet;

#include "Inline/BitSet.inl"

}
