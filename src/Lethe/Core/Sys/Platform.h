#pragma once

#include "../Common.h"

// here we: detect compiler (msc vs gcc vs clang vs unknown)
// detect operating system (windows, linux, osx, bsd)
// detect 32 vs 64 bit version
// TODO: simplify/refactor

namespace lethe
{

#if defined(__APPLE__)
#	include <TargetConditionals.h>
#endif

#if (defined(__GNUC__) && (defined(__LP64__) || defined(__x86_64__) || defined(__aarch64__))) ||\
	(defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#	define LETHE_64BIT				1
static const int BITS = 64;
#else
// FIXME: just guessing here....
#	define LETHE_32BIT				1
static const int BITS = 32;
#endif

// CPU
#if (defined(__GNUC__) && defined(__x86_64__)) || (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#	define LETHE_CPU_AMD64			1
#	define LETHE_CPU_X86				1
#	define LETHE_CPU					"amd64"
#elif (defined(__GNUC__) && (defined(__aarch64__) || defined(__arm64__) || defined(__arm__) || defined(__thumb__))) || \
	(defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARMT)))
#	define LETHE_CPU_ARM				1
#	if defined(__GNUC__) && (defined(__aarch64__) || defined(__arm64__))
#		define LETHE_CPU_ARM64		1
#		define LETHE_CPU				"arm64"
#	else
#		define LETHE_CPU				"arm"
#	endif
#elif (defined(__GNUC__) && (defined(__i386__) || defined(__i386))) || (defined(_MSC_VER) && defined(_M_IX86))	\
	|| defined(_X86_) || defined(__X86__) || defined(__I86__)
#	define LETHE_CPU					"x86"
#	define LETHE_CPU_X86				1
#else
#	define LETHE_CPU					"unknown"
#endif

#if defined(_MSC_VER)
#	define LETHE_COMPILER_MSC		1
#	if defined(__INTEL_COMPILER)
#		define LETHE_COMPILER_ICC	1
#		define LETHE_COMPILER		"icc"
#	else
#		define LETHE_COMPILER		"msvc"
#	endif
#elif defined(__GNUC__)
#	define LETHE_COMPILER_GCC		1
#	define LETHE_COMPILER			"gcc"
#else
#	define LETHE_COMPILER_UNKNOWN	1
#	define LETHE_COMPILER			"unknown"
#endif

#if defined(__MINGW32_VERSION) || defined(__MINGW32_MAJOR_VERSION)
#	define LETHE_COMPILER_MINGW		1
#endif

#if defined(__clang__)
#	define LETHE_COMPILER_CLANG		1
#	undef LETHE_COMPILER
#	define LETHE_COMPILER			"clang"
#endif

#if defined(_WIN32)
#	define LETHE_OS_WINDOWS			1
#	define LETHE_OS_FAMILY_WINDOWS	1
#	define LETHE_OS					"windows"
#	define LETHE_OS_FAMILY			"windows"
#elif defined(__ANDROID__)
#	define LETHE_OS_ANDROID			1
#	define LETHE_OS_FAMILY_ANDROID	1
#	define LETHE_OS_FAMILY_UNIX		1
#	define LETHE_OS					"android"
#	define LETHE_OS_FAMILY			"android"
#elif defined(__linux__)
#	define LETHE_OS_LINUX			1
#	define LETHE_OS_FAMILY_UNIX		1
#	define LETHE_OS					"linux"
#	define LETHE_OS_FAMILY			"unix"
#elif defined(__FreeBSD__)
#	define LETHE_OS_BSD				1
#	define LETHE_OS_FAMILY_UNIX		1
#	define LETHE_OS_FAMILY_BSD		1
#	define define LETHE_OS			"bsd"
#	define LETHE_OS_FAMILY			"unix"
#elif defined(__APPLE__) && (TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
#	define LETHE_OS_IOS				1
#	define LETHE_OS_FAMILY_IOS		1
#	define LETHE_OS_FAMILY_UNIX		1
#	define LETHE_OS					"ios"
#	define LETHE_OS_FAMILY			"ios"
#elif defined(__APPLE__)
#	define LETHE_OS_APPLE			1
#	define LETHE_OS_OSX				1
#	define LETHE_OS_FAMILY_UNIX		1
#	define LETHE_OS_FAMILY_BSD		1
#	define LETHE_OS					"apple"
#	define LETHE_OS_FAMILY			"unix"
#else
#	define LETHE_OS_UNKNOWN			1
#	define LETHE_OS_FAMILY_UNKNOWN	1
#	define LETHE_OS					"unknown"
#	define LETHE_OS_FAMILY			"unknown"
#endif

#if defined(__EMSCRIPTEN__)
#	define LETHE_PLATFORM_EMSCRIPTEN	1
#endif

// require C++98
#if __cplusplus < 199711L && __cplusplus != 1L
#	error need at least C++98 compiler
#endif

// C++11 compliant?
#if __cplusplus >= 201103L
#	define LETHE_CPP11				1
#endif

// platform-dependent macros for 64-bit long integer constants (assuming 32+-bit compiler so no need for 32-bit version)
#define LETHE_CONST_LONG(x) x##ll
#define LETHE_CONST_ULONG(x) x##ull

// format string macros for LONG/ULONG
#if LETHE_OS_WINDOWS
#	define LETHE_FORMAT_LONG "%I64d"
#	define LETHE_FORMAT_ULONG "%I64u"
#	define LETHE_FORMAT_ULONG_HEX "%I64x"
#	define LETHE_FORMAT_SIZE "%Iu"
#	define LETHE_FORMAT_INTPTR "%Id"
#	define LETHE_FORMAT_UINTPTR "%Iu"
#	define LETHE_FORMAT_LONG_SUFFIX "I64d"
#	define LETHE_FORMAT_ULONG_SUFFIX "I64u"
#	define LETHE_FORMAT_ULONG_HEX_SUFFIX "I64x"
#	define LETHE_FORMAT_SIZE_SUFFIX "Iu"
#	define LETHE_FORMAT_INTPTR_SUFFIX "Id"
#	define LETHE_FORMAT_UINTPTR_SUFFIX "Iu"
#else
	// FIXME: where was this condition supposed to work?!
#	if 0 && LETHE_64BIT && !LETHE_OS_OSX
#		define LETHE_FORMAT_LONG "%ld"
#		define LETHE_FORMAT_ULONG "%lu"
#		define LETHE_FORMAT_ULONG_HEX "%lx"
#		define LETHE_FORMAT_LONG_SUFFIX "ld"
#		define LETHE_FORMAT_ULONG_SUFFIX "lu"
#		define LETHE_FORMAT_ULONG_HEX_SUFFIX "lx"
#	else
#		define LETHE_FORMAT_LONG "%lld"
#		define LETHE_FORMAT_ULONG "%llu"
#		define LETHE_FORMAT_ULONG_HEX "%llx"
#		define LETHE_FORMAT_LONG_SUFFIX "lld"
#		define LETHE_FORMAT_ULONG_SUFFIX "llu"
#		define LETHE_FORMAT_ULONG_HEX_SUFFIX "llx"
#	endif
#	define LETHE_FORMAT_SIZE "%zu"
#	define LETHE_FORMAT_INTPTR "%zd"
#	define LETHE_FORMAT_UINTPTR "%zu"
#	define LETHE_FORMAT_SIZE_SUFFIX "zu"
#	define LETHE_FORMAT_INTPTR_SUFFIX "zd"
#	define LETHE_FORMAT_UINTPTR_SUFFIX "zu"
#endif

#if LETHE_COMPILER_GCC
#	define LETHE_FORMAT_ATTR_FUNC_SUFFIX __attribute__ ((format (printf, 1, 2)))
#	define LETHE_FORMAT_ATTR_METHOD_SUFFIX __attribute__ ((format (printf, 2, 3)))
#else
#	define LETHE_FORMAT_ATTR_FUNC_SUFFIX
#	define LETHE_FORMAT_ATTR_METHOD_SUFFIX
#endif

#if LETHE_COMPILER_GCC
// this fixes annoying clang warnings
#	define LETHE_VISIBLE __attribute__((visibility("default")))
#else
#	define LETHE_VISIBLE
#endif

// because special clang now runs under VS
// (it's just that I prefer gcc builtins when available)
// note: Intel compiler is treated as full replacement for msc
#if LETHE_COMPILER_MSC && !LETHE_COMPILER_GCC && !LETHE_COMPILER_CLANG
#	define LETHE_COMPILER_MSC_ONLY	1
#endif

// unreachable helps a lot in switches where we don't want to range-check (at least in msc)
#if !LETHE_DEBUG
#	if LETHE_COMPILER_MSC_ONLY
#		define LETHE_UNREACHABLE __assume(0)
#	else
#		define LETHE_UNREACHABLE __builtin_unreachable()
#	endif
#else
#	define LETHE_UNREACHABLE
#endif

// allows to concat expandable macros
#define LETHE_CONCAT_MACROS__(x, y) x##y
#define LETHE_CONCAT_MACROS(x, y) LETHE_CONCAT_MACROS__(x, y)

// defer lambda (or "scope exit")
#define LETHE_DEFER(f) struct LETHE_CONCAT_MACROS(lethe_defer__, __LINE__) { inline ~LETHE_CONCAT_MACROS(lethe_defer__, __LINE__)(){f();} } LETHE_CONCAT_MACROS(lethe_defer_, __LINE__)

// fake pre-C++11 stuff for pre-vs2012
#if LETHE_COMPILER_MSC_ONLY
#	if _MSC_VER < 1900 && !defined(_ALLOW_KEYWORD_MACROS)
#		define _ALLOW_KEYWORD_MACROS	1
#	endif
#	if _MSC_VER < 1700 && !defined(override)
#		define override
#	endif
#	if _MSC_VER < 1700 && !defined(nullptr)
#		define nullptr ((void *)0)
#	endif
#	if _MSC_VER < 1900 && !defined(noexcept)
#		define noexcept throw()
#	endif
#	if _MSC_VER >= 1900 && !defined(LETHE_CPP11)
#		define LETHE_CPP11	1
#	endif
#elif !defined(LETHE_CPP11)
#	define LETHE_CPP11	1
#endif

#define LETHE_COMPILER_NOT_MSC (LETHE_COMPILER_GCC || LETHE_COMPILER_CLANG || LETHE_COMPILER_MINGW)

struct LETHE_API Platform
{
	// get number of physical processor cores; 0 if unknown
	static inline int GetNumberOfPhysicalCores()
	{
		return physCores;
	}
	// get number of logical processor cores; 0 if unknown
	static inline int GetNumberOfLogicalCores()
	{
		return logCores;
	}

	// returns true if this is a 32-bit app running 64-bit OS, only works on Windows
	static bool Is32BitProcessOn64BitOS();

	// one time static init
	static void Init();
	static void Done();

private:
	friend class CoreModule;
	static void AdjustForHyperthreading();

	static int physCores;
	static int logCores;
};

// cache line size
#define LETHE_CACHELINE_SIZE 64

// note: put this after ptr or ref
#define LETHE_RESTRICT __restrict

}
