#pragma once

#include "Platform.h"

#define LETHE_RET_FALSE_UNLIKELY(x) do { if (LETHE_UNLIKELY(!(x))) return 0; } while(0)
#define LETHE_RET_TRUE_UNLIKELY(x) do { if (LETHE_UNLIKELY((x))) return 0; } while(0)
#define LETHE_RET_FALSE_LIKELY(x) do { if (LETHE_LIKELY(!(x))) return 0; } while(0)
#define LETHE_RET_TRUE_LIKELY(x) do { if (LETHE_LIKELY((x))) return 0; } while(0)

#if LETHE_COMPILER_MSC
#	pragma warning(disable:4127)	// disable silly conditional expression is constant warning
#endif

#if !defined(LETHE_LIKELY)
#	if LETHE_COMPILER_MSC
#		define LETHE_LIKELY(x) x
#		define LETHE_UNLIKELY(x) x
#	else
#		define LETHE_LIKELY(x) __builtin_expect(!!(x),1)
#		define LETHE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#	endif
#endif
