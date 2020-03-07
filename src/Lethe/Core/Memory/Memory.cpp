#include "Memory.h"

#include <memory.h>
#include <stdlib.h>

namespace lethe
{

template< typename T >
size_t StrLenTemplate(const T *str)
{
	if (!str)
		return 0;

	const T *old = str;

	while (*str++);

	return size_t(str - old - 1);
}

size_t StrLen(const char *str)
{
	return StrLenTemplate(str);
}

size_t StrLen(const wchar_t *str)
{
	return StrLenTemplate(str);
}

}
