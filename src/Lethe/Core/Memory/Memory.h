#pragma once

#include "../Sys/Types.h"
#include "../Sys/Assert.h"
#include <memory.h>

namespace lethe
{

// important note: it's undefined if memset/memcpy/memcmp (and I suppose memove as well)
// is being called with null pointer even if size is zero!
// must fix my program wherever this happens... maybe I'm lucky that I've exceptions off?

inline void MemSet(void *dst, int value, size_t count)
{
	LETHE_ASSERT(dst);
	memset(dst, value, count);
}

inline void MemCpy(void *dst, const void *src, size_t count)
{
	LETHE_ASSERT(dst && src);
	memcpy(dst, src, count);
}

inline void MemMove(void *dst, const void *src, size_t count)
{
	LETHE_ASSERT(dst && src);
	memmove(dst, src, count);
}

inline int MemCmp(const void *src0, const void *src1, size_t count)
{
	LETHE_ASSERT(src0 && src1);
	return memcmp(src0, src1, count);
}

inline void MemSwap(void *dst, void *src, size_t count)
{
	auto cnt = count / sizeof(void *);
	count %= sizeof(void *);

	void *tmp;

	auto d = static_cast<Byte *>(dst);
	auto s = static_cast<Byte *>(src);

	while (cnt--)
	{
		memcpy(&tmp, d, sizeof(void *));
		memcpy(d, s, sizeof(void *));
		memcpy(s, &tmp, sizeof(void *));
		d += sizeof(void *);
		s += sizeof(void *);
	}

	LETHE_COMPILE_ASSERT(sizeof(void *) <= 8);

	// handle the rest
	if (sizeof(void *) > 4 && count >= 4)
	{
		memcpy(&tmp, d, 4); memcpy(d, s, 4); memcpy(s, &tmp, 4);
		d += 4;
		s += 4;
		count -= 4;
	}

	if (count >= 2)
	{
		memcpy(&tmp, d, 2); memcpy(d, s, 2); memcpy(s, &tmp, 2);
		d += 2;
		s += 2;
		count -= 2;
	}

	if (count)
	{
		memcpy(&tmp, d, 1); memcpy(d, s, 1); memcpy(s, &tmp, 1);
	}
}

// still useful StrLen
size_t LETHE_API StrLen(const char *str);
size_t LETHE_API StrLen(const wchar_t *str);

}
