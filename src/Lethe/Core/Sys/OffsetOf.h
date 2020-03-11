#pragma once

#include "Types.h"

namespace lethe
{

// portable offsetof
// reference: graphitemaster/WORKING_AROUND_OFFSETOF_LIMITATIONS.MD (gist) [modified to use local copy]
template <typename T1, typename T2>
struct offset_of_impl
{
	static inline int offset(T1 T2::*member)
	{
		T2 obj;
		return (int)size_t(&(obj.*member)) -
		(int)size_t(&obj);
	}
};

template <typename T1, typename T2>
inline int offset_of(T1 T2::*member)
{
	return offset_of_impl<T1, T2>::offset(member);
}

}
