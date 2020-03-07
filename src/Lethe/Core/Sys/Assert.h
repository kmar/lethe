#pragma once

#include "../Common.h"
#include "Likely.h"

#if LETHE_DEBUG
#	include <cassert>
#endif

// compile-time assert
#define LETHE_COMPILE_ASSERT(cond) do { struct CompileTimeAssert { char arr[(cond) ? 1 : -1]; } tmp_; tmp_.arr[0]=0;(void)tmp_; } while(false)

// runtime assert (even in release)
#define LETHE_RUNTIME_ASSERT(cond) do { \
		if (LETHE_UNLIKELY(!(cond))) { \
			lethe::AbortProgramAssert(#cond, __FILE__, __LINE__);  \
		} \
	} while(false)

// assert/verify
#if !LETHE_DEBUG
#	define LETHE_ASSERT(cond) do {} while(false)
#	define LETHE_VERIFY(cond) ((void)(cond))
#else
#	define LETHE_ASSERT(cond) assert(cond)
#	define LETHE_VERIFY(cond) assert(cond)
#endif

#if !defined(assert)
#	define assert LETHE_ASSERT
#endif

#define LETHE_ASSERT_ALWAYS(msg) LETHE_ASSERT(false && #msg)
#define LETHE_NOT_IMPLEMENTED(func) LETHE_ASSERT(false && "Not implemented: " #func)

namespace lethe
{

// trap into debugger (where available)
void LETHE_API DebugerTrap();
// aborts program
void LETHE_API AbortProgram(const char *msg = nullptr, int exitCode = 1);
void LETHE_API AbortProgramAssert(const char *msg, const char *file, int line, int exitCode = 1);
// abort assert( aborts program)
inline void AbortAssert(bool cond, const char *desc = nullptr, int exitCode = 1)
{
	if (LETHE_UNLIKELY(!cond))
		AbortProgram(desc, exitCode);
}

}
