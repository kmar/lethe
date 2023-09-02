#pragma once

#include "../Sys/NoCopy.h"
#include "../Thread/Lock.h"
#include "../Memory/Heap.h"

namespace lethe
{

struct PrivateBucketAllocArray;

// can only be used for constant-sized objects
class LETHE_API BucketAlloc : public NoCopy
{
	friend struct PrivateBucketAllocArray;

	// funny, using Mutex seems better for Heap but SpinLock seems better for BucketAlloc
	// even funnier: Heap is slightly faster than Bucket under heavy contention BUT
	// Bucket is almost 4x faster when single-threaded!
	typedef SpinMutex MutexType;
	typedef SpinMutexLock MutexLockType;

	inline BucketAlloc() {}
	void Construct(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize = 16, Int nalign = 8, Int nflags = 0);

public:
	enum Flags
	{
		FLG_AUTODELETE = 1
	};

	// allocates n buckets to be cached and used among threads
	// MUST be a power of 2
	static const Int THREAD_BUCKET_CACHE_COUNT = 1;

	BucketAlloc(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize = 16, Int nalign = 8, Int nflags = 0);
	~BucketAlloc();

	inline void *Alloc()
	{
		return AllocInternal();
	}
	void Free(void *ptr);

	void Reset();

	static void *ThreadAlloc(BucketAlloc *allocators);
	static void ThreadFree(void *obj, Int nalign);

	static ULong GetUsage();

	static void StaticInit();
	static void StaticDone();

protected:
	void *AllocInternal();

	struct Bucket;

	// object header
	struct BucketObject
	{
		Bucket *bucket;
		// freelist chain
		BucketObject *next;
	};

	struct Bucket
	{
		// fast-alloc pointer
		IntPtr ptr;
		// number of allocs in this bucket
		IntPtr allocs;
		// bucket freelist chain
		Bucket *prevFree, *nextFree;
		// object freelist chain
		BucketObject *tail;
		// bucket chain
		Bucket *prev, *next;
		// parent allocator
		BucketAlloc *allocator;
		Byte data[1];
	};

	mutable MutexType mutex;

	Byte pad[LETHE_CACHELINE_SIZE];

	Bucket *freeHead, *freeTail;
	Bucket *bucketHead, *bucketTail;
	Bucket *lastEmptyBucket;
	// tracks allocation count
	IntPtr allocs;
	// for alloc
	IntPtr elemSize;
	IntPtr elemAlign;
	// elem size including header
	IntPtr elemSizeHdr;
	IntPtr bucketSize;
	// offset to first aligned bucketObj
	Int dataOfs;
	// object header size + padding
	Int hdrSize;
	// bucket size (excluding data)
	Int bsize;

	ULong GetBucketUsage() const;

private:
	BucketObject *PtrToObject(void *ptr);

	const char *objName;
	// static chain of bucket allocators
	BucketAlloc *prev;
	BucketAlloc *next;
	Int flags;

	static BucketAlloc *head;
	static BucketAlloc *tail;
	static Mutex *listMutex;
};

struct LETHE_API PrivateBucketAllocArray
{
	BucketAlloc arr[BucketAlloc::THREAD_BUCKET_CACHE_COUNT];

	PrivateBucketAllocArray(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize = 16, Int nalign = 8, Int nflags = 0);
};

#if LETHE_DISABLE_FANCY_ALLOCATORS

#define LETHE_BUCKET_ALLOC_OVERRIDE(obj)
#define LETHE_BUCKET_ALLOC(obj)
#define LETHE_BUCKET_ALLOC_COMMON(obj, ovr)
#define LETHE_BUCKET_ALLOC_DEF(obj)
#define LETHE_BUCKET_ALLOC_DEF_SIZE(obj, bsize)

#define LETHE_BUCKET_ALLOC_INLINE(obj)
#define LETHE_BUCKET_ALLOC_SIZE_INLINE(obj, size)
#define LETHE_BUCKET_ALLOC_INLINE_OVERRIDE(obj)
#define LETHE_BUCKET_ALLOC_SIZE_INLINE_OVERRIDE(obj, size)
#define LETHE_BUCKET_ALLOC_DEF_SIZE_INLINE_COMMON(obj, bsize, ovr)

#else

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
	CustomDeleterFunc GetCustomDeleter() const ovr; \
	static lethe::PrivateBucketAllocArray privateAllocArr;

#define LETHE_BUCKET_ALLOC_DEF(obj) LETHE_BUCKET_ALLOC_DEF_SIZE(obj, 16)

#define LETHE_BUCKET_ALLOC_DEF_SIZE(obj, bsize) \
	lethe::PrivateBucketAllocArray obj ::privateAllocArr(\
		#obj, (IntPtr)sizeof(obj), (IntPtr)(bsize), (Int)lethe::AlignOf<obj>::align); \
	void * obj ::operator new(size_t sz) \
	{ \
		(void)sz; \
		LETHE_ASSERT(sz == sizeof(obj)); \
		return lethe::BucketAlloc::ThreadAlloc(privateAllocArr.arr); \
	} \
	void obj ::operator delete(void *ptr) { \
		CustomDeleter_((const obj *)ptr); \
	} \
	void CustomDeleter_(const obj *ptr) { \
		lethe::BucketAlloc::ThreadFree((void *)ptr, (Int)lethe::AlignOf<obj>::align); \
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
		static lethe::PrivateBucketAllocArray privateAllocArr(\
			#obj, (IntPtr)sizeof(obj), (IntPtr)(bsize), (Int)lethe::AlignOf<obj>::align); \
		(void)sz; \
		LETHE_ASSERT(sz == sizeof(obj)); \
		return lethe::BucketAlloc::ThreadAlloc(privateAllocArr.arr); \
	} \
	void operator delete(void *ptr) { \
		CustomDeleter_((const obj *)ptr); \
	} \
	static void CustomDeleter_(const obj *ptr) { \
		lethe::BucketAlloc::ThreadFree((void *)ptr, (Int)lethe::AlignOf<obj>::align); \
	} \
	CustomDeleterFunc GetCustomDeleter() const ovr \
	{ \
		return [](const void *ptr) {return CustomDeleter_((const obj *)ptr);}; \
	} \
private:

// !LETHE_DISABLE_FANCY_ALLOCATORS
#endif

}
