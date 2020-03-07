#pragma once

#include "../Common.h"

namespace lethe
{

// Inherit from this to prevent copying
class LETHE_API NoCopy
{
	inline NoCopy(NoCopy &) {}
	inline NoCopy &operator =(const NoCopy &)
	{
		return *this;
	}
public:
	inline NoCopy() {}
};

}
