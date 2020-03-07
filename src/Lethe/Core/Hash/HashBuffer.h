#pragma once

#include "../Sys/Types.h"
#include "../Sys/Platform.h"

namespace lethe
{

// primary hashing function
// beware of floats! -0 is different from +0!
// this one requires 4-byte aligned buffer
UInt LETHE_API HashBuffer(const void *buf, size_t sz);
// unaligned version of the above
// (calls fast aligned version if aligned)
UInt LETHE_API HashBufferUnaligned(const void *buf, size_t sz);
// if we know input is word-aligned (faster than unaligned but slower than default)
UInt LETHE_API HashBufferWordAligned(const void *buf, size_t sz);
// merge two hashes for incremental hashing
UInt LETHE_API HashMerge(UInt h1, UInt h2);
// fast specialized hashers
UInt LETHE_API HashUInt(UInt v);
UInt LETHE_API HashULong(ULong v);

#if LETHE_CPP11
#	include "Inline/HashString.inl"
#else
// hashes ansi string
UInt LETHE_API HashAnsiString(const char *str);
#endif

}
