#pragma once

#include "../Common.h"

// note: we do not include platform.h here

#if !defined(_MSC_VER)
#	include <stdint.h>			// uintptr_t, ...
#else
typedef signed __int8		int8_t;
typedef unsigned __int8		uint8_t;
typedef signed __int16		int16_t;
typedef unsigned __int16	uint16_t;
typedef signed __int32		int32_t;
typedef unsigned __int32	uint32_t;
typedef signed __int64		int64_t;
typedef unsigned __int64	uint64_t;
#endif

#include <stddef.h>			// size_t, ...
#include <wchar.h>
#include <new>				// placement new

namespace lethe
{

// probably more useful aliases
typedef uint8_t				b8;		// fake bool 8
typedef int8_t				i8;
typedef uint8_t				u8;
typedef int16_t				i16;
typedef uint16_t			u16;
typedef int32_t				i32;
typedef uint32_t			u32;
typedef int64_t				i64;
typedef uint64_t			u64;
typedef float				f32;
typedef double				f64;

typedef unsigned int		uint;

// I'm slowly fed up with using i32 and stuff like that, maybe these might be a replacement?
typedef bool Bool;
typedef i8 SByte;
typedef u8 Byte;
typedef i16 Short;
typedef u16 UShort;
typedef i32 Int;
typedef u32 UInt;
typedef i64 Long;
typedef u64 ULong;
typedef f32 Float;
typedef f64 Double;
typedef char Char;
typedef wchar_t WChar;

typedef intptr_t IntPtr;
typedef uintptr_t UIntPtr;

// use atomic int types, this should be useful when switching to C++11
typedef volatile SByte AtomicSByte;
typedef volatile Byte AtomicByte;
typedef volatile Int AtomicInt;
typedef volatile UInt AtomicUInt;
typedef volatile Short AtomicShort;
typedef volatile UShort AtomicUShort;
typedef volatile Long AtomicLong;
typedef volatile ULong AtomicULong;

}
