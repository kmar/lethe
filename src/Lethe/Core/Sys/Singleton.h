#pragma once

#include "Assert.h"
#include "Inline.h"

// OK, definitely switching to indirect singletons - caused segfaults on Linux with gcc

namespace lethe
{

// must create instance in cpp
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

}
