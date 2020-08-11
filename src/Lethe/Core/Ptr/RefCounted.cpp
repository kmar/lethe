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

	ReleaseWeak(this);
}

void LETHE_NOINLINE RefCounted::CustomDeleteObjectSkeleton(const RefCounted *self)
{
	(*(CustomDeleterFunc *)self)(self);
}

RefCounted::CustomDeleterFunc RefCounted::GetCustomDeleter() const
{
	return [](const void *ptr){((RefCounted *)ptr)->operator delete((void *)ptr);};
}

}
