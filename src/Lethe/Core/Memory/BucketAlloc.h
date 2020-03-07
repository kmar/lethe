#pragma once

#include "AlignedAlloc.h"
#include "../Sys/Assert.h"

namespace lethe
{

#define LETHE_BUCKET_ALLOC_OVERRIDE(obj) LETHE_BUCKET_ALLOC_COMMON(obj, override)
#define LETHE_BUCKET_ALLOC(obj) LETHE_BUCKET_ALLOC_COMMON(obj,)

#define LETHE_BUCKET_ALLOC_COMMON(obj, ovr) \
public: \
	void *operator new(size_t sz); \
	void operator delete(void *ptr); \
	friend void CustomDeleter_(const obj *ptr); \
private: \
	using CustomDeleterFunc = void(*)(const void *); \
	/* maybe override */ \
	CustomDeleterFunc GetCustomDeleter() const ovr;

#define LETHE_BUCKET_ALLOC_DEF(obj) LETHE_BUCKET_ALLOC_DEF_SIZE(obj, 16)

#define LETHE_BUCKET_ALLOC_DEF_SIZE(obj, bsize) \
	void * obj ::operator new(size_t sz) \
	{ \
		(void)sz; \
		LETHE_ASSERT(sz == sizeof(obj)); \
		return lethe::BucketAllocator.CallAlloc(sz, alignof(obj)); \
	} \
	void obj ::operator delete(void *ptr) { \
		CustomDeleter_((const obj *)ptr); \
	} \
	void CustomDeleter_(const obj *ptr) { \
		lethe::BucketAllocator.CallFree((void *)ptr); \
	} \
	obj ::CustomDeleterFunc obj ::GetCustomDeleter() const \
	{ \
		return [](const void *ptr) {return CustomDeleter_((const obj *)ptr);}; \
	}

// inline version:

#define LETHE_BUCKET_ALLOC_INLINE(obj) \
	LETHE_BUCKET_ALLOC_DEF_SIZE_INCLINE_COMMON(obj, 16,)

#define LETHE_BUCKET_ALLOC_SIZE_INLINE(obj, size) \
	LETHE_BUCKET_ALLOC_DEF_SIZE_INLINE_COMMON(obj, size,)

#define LETHE_BUCKET_ALLOC_INLINE_OVERRIDE(obj) \
	LETHE_BUCKET_ALLOC_DEF_SIZE_INLINE_COMMON(obj, 16, override)

#define LETHE_BUCKET_ALLOC_SIZE_INLINE_OVERRIDE(obj, size) \
	LETHE_BUCKET_ALLOC_DEF_SIZE_INLINE_COMMON(obj, size, override)

#define LETHE_BUCKET_ALLOC_DEF_SIZE_INLINE_COMMON(obj, bsize, ovr) \
private: \
	using CustomDeleterFunc = void(*)(const void *); \
public: \
	void *operator new(size_t sz) \
	{ \
		(void)sz; \
		LETHE_ASSERT(sz == sizeof(obj)); \
		return lethe::BucketAllocator.CallAlloc(sz, alignof(obj)); \
	} \
	void operator delete(void *ptr) { \
		CustomDeleter_((const obj *)ptr); \
	} \
	static void CustomDeleter_(const obj *ptr) { \
		lethe::BucketAllocator.CallFree((void *)ptr); \
	} \
	CustomDeleterFunc GetCustomDeleter() const ovr \
	{ \
		return [](const void *ptr) {return CustomDeleter_((const obj *)ptr);}; \
	} \
private:

}
