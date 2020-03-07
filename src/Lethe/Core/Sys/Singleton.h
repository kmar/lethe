#pragma once

#include "Assert.h"
#include "../Math/Templates.h"
#include "../Memory/Memory.h"
#include "Inline.h"
#include "Align.h"
#include "Platform.h"

#if 1//LETHE_OS_IOS
// OK, definitely switching to indirect singletons - caused segfaults on Linux with gcc
// FIXME: seems direct storage doesn't work on iOS due to strict aliasing... (release only)
#	define LETHE_INDIRECT_SINGLETONS 1
#endif

namespace lethe
{

// for direct singletons
static const Int LETHE_MAX_SINGLETON_SIZE = 1024;

// must create instance in cpp
#if LETHE_INDIRECT_SINGLETONS || LETHE_DEBUG
#	define LETHE_SINGLETON_INSTANCE(cls) \
	cls *cls::instance = nullptr; \
	/* static init */ \
	void cls::Init() \
	{ \
		LETHE_ASSERT(!instance); \
		instance = new cls; \
	} \
	/* static de-init */ \
	void cls::Done() \
	{ \
		LETHE_ASSERT(instance); \
		delete instance; \
		instance = nullptr; \
	}
#else
#	define LETHE_SINGLETON_INSTANCE(cls) \
		lethe::ULong cls::instance[lethe::LETHE_MAX_SINGLETON_SIZE / sizeof(lethe::ULong)] = {0};	\
		bool cls::singletonInitialized = false; \
		/* static init */ \
		void cls::Init() \
		{ \
			LETHE_ASSERT(!singletonInitialized); \
			LETHE_COMPILE_ASSERT(sizeof(cls) <= lethe::LETHE_MAX_SINGLETON_SIZE); \
			new(reinterpret_cast<cls *>(instance)) cls; \
			singletonInitialized = true; \
		} \
		/* static de-init */ \
		void cls::Done() \
		{ \
			LETHE_ASSERT(singletonInitialized); \
			Get().~cls(); \
			MemSet(instance, 0, sizeof(instance)); \
			singletonInitialized = false; \
		}
#endif

#if LETHE_INDIRECT_SINGLETONS || LETHE_DEBUG
#	define LETHE_SINGLETON(cls) \
	private: \
		static cls *instance; \
	public: \
		/* get instance */ \
		static LETHE_FORCEINLINE cls &Get() \
		{ \
			LETHE_ASSERT(instance); \
			return *instance; \
		} \
		/* get instance ptr */ \
		static LETHE_FORCEINLINE cls *GetPtr() \
		{ \
			return &Get(); \
		} \
		static void Init(); \
		static void Done();
#else
#	define LETHE_SINGLETON(cls) \
	private: \
		LETHE_ALIGN(64) static ULong instance[LETHE_MAX_SINGLETON_SIZE / sizeof(ULong)]; \
		static bool singletonInitialized; \
	public: \
		/* get instance */ \
		static LETHE_FORCEINLINE cls &Get() \
		{ \
			cls *res = reinterpret_cast<cls *>(instance); \
			return *res; \
		} \
		/* get instance ptr */ \
		static LETHE_FORCEINLINE cls *GetPtr() \
		{ \
			return &Get(); \
		} \
		static void Init(); \
		static void Done();
#endif

}
