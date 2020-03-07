#include "RefCounted.h"
#include "../Memory/Heap.h"

namespace lethe
{

void LETHE_NOINLINE RefCounted::ReleaseAfterStrongZero() const
{
	Finalize();

	auto cdel = GetCustomDeleter();

	// postponing delete
	// destroy
	this->~RefCounted();

	// and abuse vtable for custom deleter!
	*(CustomDeleterFunc *)this = cdel;

	ReleaseWeak();
}

void LETHE_NOINLINE RefCounted::CustomDeleteObjectSkeleton() const
{
	(*(CustomDeleterFunc *)this)(this);
}

RefCounted::CustomDeleterFunc RefCounted::GetCustomDeleter() const
{
	return [](const void *ptr){delete reinterpret_cast<const Byte *>(ptr);};
}

}
