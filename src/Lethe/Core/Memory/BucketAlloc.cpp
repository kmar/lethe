#include "BucketAlloc.h"
#include "../Memory/Memory.h"
#include "../Thread/Thread.h"
#include "../String/String.h"

namespace lethe
{

// BucketAlloc

BucketAlloc *BucketAlloc::head = nullptr;
BucketAlloc *BucketAlloc::tail = nullptr;
Mutex *BucketAlloc::listMutex = nullptr;

BucketAlloc::BucketAlloc(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize, Int nalign, Int nflags)
{
	Construct(nobjName, nelemSize, nbucketSize, nalign, nflags);
}

void BucketAlloc::Construct(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize, Int nalign, Int nflags)
{
	prev = next = nullptr;
	allocs = 0;
	elemSize = nelemSize;
	bucketSize = nbucketSize;
	dataOfs = 0;
	hdrSize = sizeof(BucketObject);
	bsize = 0;
	objName = nobjName;
	flags = nflags;

	LETHE_ASSERT(nelemSize > 0 && nbucketSize > 0);
	IntPtr u = sizeof(UIntPtr);
	elemAlign = Max<IntPtr>(u, nalign);

	Bucket dummy;
	bsize = (Int)(dummy.data - reinterpret_cast<const Byte *>(&dummy));

	dataOfs = Int((bsize + (elemAlign-1))/elemAlign * elemAlign - bsize);
	hdrSize = Int((sizeof(BucketObject) - sizeof(void *) + (elemAlign-1))/elemAlign * elemAlign);
	elemSizeHdr = ((hdrSize + nelemSize) + elemAlign-1)/elemAlign * elemAlign;
	freeHead = freeTail = nullptr;
	bucketHead = bucketTail = nullptr;
	lastEmptyBucket = nullptr;

	if (!listMutex)
		listMutex = new Mutex;

	MutexLock _(*listMutex);

	LETHE_DLIST_ADD(this, head, tail, prev, next)
}

BucketAlloc::~BucketAlloc()
{
	Reset();

	if (flags & FLG_AUTODELETE)
		LETHE_DLIST_UNLINK(this, head, tail, prev, next);
}

void BucketAlloc::Reset()
{
	MutexLockType _(mutex);
	LETHE_ASSERT(!allocs && "leaks found in bucket!");

	Bucket *b = bucketHead;

	while (b)
	{
		Bucket *nb = b->next;
		AlignedAlloc::Free(b);
		b = nb;
	}

	freeHead = freeTail = nullptr;
	bucketHead = bucketTail = nullptr;
	lastEmptyBucket = nullptr;
	allocs = 0;
}

void *BucketAlloc::AllocInternal()
{
	MutexLockType _(mutex);

	for (;;)
	{
		// first try very fast allocation
		Bucket *curBucket = freeTail;

		if (LETHE_LIKELY(curBucket))
		{
			Bucket &cb = *curBucket;

			if (cb.ptr < bucketSize)
			{
				// very fast alloc
				if (lastEmptyBucket == curBucket)
					lastEmptyBucket = nullptr;

				++allocs;

				if (++cb.allocs >= bucketSize)
				{
					LETHE_ASSERT(cb.allocs == bucketSize);
					LETHE_DLIST_UNLINK(curBucket, freeHead, freeTail, prevFree, nextFree);
				}

				return cb.data + elemSizeHdr * (cb.ptr++) + dataOfs + hdrSize;
			}
		}

		// if not possible, examine freelist
		if (freeTail)
		{
			if (lastEmptyBucket == freeTail)
				lastEmptyBucket = nullptr;

			// unlink free object
			BucketObject *resObj = freeTail->tail;
			LETHE_ASSERT(resObj);
			freeTail->allocs++;
#if !defined(__clang_analyzer__)
			BucketObject *nextObj = resObj->next;
			freeTail->tail = nextObj;

			if (!nextObj)
			{
				// unlink bucket
				LETHE_DLIST_UNLINK(freeTail, freeHead, freeTail, prevFree, nextFree);
			}

#endif
			allocs++;
			return reinterpret_cast<char *>(resObj) + hdrSize;
		}

		// everything failed => add new bucket
		Bucket *b = reinterpret_cast<Bucket *>(AlignedAlloc::Alloc((size_t)bsize + dataOfs + (size_t)elemSizeHdr*bucketSize, elemAlign));
		b->allocs = 0;
		b->ptr = 0;
		b->tail = nullptr;
		b->prevFree = b->nextFree = nullptr;
		b->prev = b->next = nullptr;
		b->allocator = this;
		LETHE_DLIST_ADD(b, bucketHead, bucketTail, prev, next);
		LETHE_DLIST_ADD(b, freeHead, freeTail, prevFree, nextFree);
		LETHE_ASSERT(!lastEmptyBucket);
		// assign buckets
		Byte *tmp = b->data + dataOfs;

		for (Int i=0; i<bucketSize; i++)
		{
			BucketObject *bobj = reinterpret_cast<BucketObject *>(tmp);
			bobj->bucket = b;
			bobj->next = nullptr;
			tmp += elemSizeHdr;
		}
	}
}

void BucketAlloc::Free(void *ptr)
{
	if (!ptr)
		return;

	MutexLockType _(mutex);
	// always add to freelist
	allocs--;
	BucketObject *obj = PtrToObject(ptr);

	Bucket *b = obj->bucket;
	LETHE_DLIST_ADD(b, freeHead, freeTail, prevFree, nextFree);

	if (!--b->allocs)
	{
		if (lastEmptyBucket)
		{
			// free lastEmpty
			LETHE_DLIST_UNLINK(lastEmptyBucket, freeHead, freeTail, prevFree, nextFree);
			LETHE_DLIST_UNLINK(lastEmptyBucket, bucketHead, bucketTail, prev, next);
			AlignedAlloc::Free(lastEmptyBucket);
		}

		lastEmptyBucket = b;
		b->ptr = 0;
		b->tail = nullptr;
		return;
	}

	obj->next = b->tail;
	b->tail = obj;
}

BucketAlloc::BucketObject *BucketAlloc::PtrToObject(void *ptr)
{
	return reinterpret_cast<BucketObject *>(static_cast<char *>(ptr) - hdrSize);
}

void *BucketAlloc::ThreadAlloc(BucketAlloc *allocators)
{
	Int idx = 0;/*Thread::GetCurrentWorkerId() & (THREAD_BUCKET_CACHE_COUNT-1);*/
	return allocators[idx].Alloc();
}

void BucketAlloc::ThreadFree(void *ptr, Int nalign)
{
	if (!ptr)
		return;

	LETHE_ASSERT(IsPowerOfTwo(nalign));
	auto ohdrSize = Int((sizeof(BucketObject) - sizeof(void *) + ((size_t)nalign-1)) & ~((size_t)nalign-1));
	BucketObject *bobj = reinterpret_cast<BucketObject *>(static_cast<char *>(ptr) - ohdrSize);
	bobj->bucket->allocator->Free(ptr);
}

ULong BucketAlloc::GetBucketUsage() const
{
	MutexLockType _(mutex);

	const auto *h = bucketHead;

	ULong total = 0;

	while (h)
	{
		total += (ULong)bsize + dataOfs + (ULong)elemSizeHdr*bucketSize + elemAlign;
		h = h->next;
	}

	return total + sizeof(*this);
}

ULong BucketAlloc::GetUsage()
{
	MutexLock _(*listMutex);

	auto *tmp = head;

	ULong total = 0;

	while (tmp)
	{
		total += tmp->GetBucketUsage() + sizeof(BucketAlloc);
		tmp = tmp->next;
	}

	return total;
}

void BucketAlloc::StaticInit()
{
	if (!listMutex)
		listMutex = new Mutex;
}

void BucketAlloc::StaticDone()
{
	BucketAlloc *cur = tail;

	while (cur)
	{
		auto *prev = cur->prev;
		cur->Reset();

		if (cur->flags & FLG_AUTODELETE)
			delete cur;

		cur = prev;
	}

	delete listMutex;
	listMutex = nullptr;
}

// PrivateBucketAllocArray

PrivateBucketAllocArray::PrivateBucketAllocArray(const char *nobjName, IntPtr nelemSize, IntPtr nbucketSize, Int nalign, Int nflags)
{
	for (int i=0; i<BucketAlloc::THREAD_BUCKET_CACHE_COUNT; i++)
		arr[i].Construct(nobjName, nelemSize, nbucketSize, nalign, nflags);
}

}
