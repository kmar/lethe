#pragma once

#include "Platform.h"

#if LETHE_COMPILER_MSC || LETHE_COMPILER_ICC
#	define LETHE_ALIGN(x) __declspec(align(x))
#else
#	define LETHE_ALIGN(x) __attribute((aligned(x)))
#endif
