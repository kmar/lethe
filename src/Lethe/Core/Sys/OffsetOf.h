#pragma once

#include "Types.h"
#include "Platform.h"

namespace lethe
{

// portable offsetof
// reference: graphitemaster/WORKING_AROUND_OFFSETOF_LIMITATIONS.MD (gist) [modified to use local copy]
template <typename T1, typename T2>
struct offset_of_impl
{
	static size_t offset(T1 T2::*member)
	{
#if LETHE_COMPILER_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
		ULong tmp[sizeof(T2)/sizeof(ULong)+1];
		T2 &obj = (T2 &)tmp;
		return size_t(&(obj.*member)) -
		size_t(&obj);
#if LETHE_COMPILER_GCC
#	pragma GCC diagnostic pop
#endif
	}
};

template <typename T1, typename T2>
inline size_t offset_of(T1 T2::*member)
{
	return offset_of_impl<T1, T2>::offset(member);
}

}
