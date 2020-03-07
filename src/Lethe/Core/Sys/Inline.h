#pragma once

#include "Platform.h"

#if LETHE_COMPILER_MSC || LETHE_COMPILER_ICC
// msc/icc
#	define LETHE_NOINLINE __declspec(noinline)
#	define LETHE_FORCEINLINE inline __forceinline
#else
// gcc/clang
#	define LETHE_NOINLINE __attribute__((noinline))
#	define LETHE_FORCEINLINE inline __attribute__((always_inline))
#endif
