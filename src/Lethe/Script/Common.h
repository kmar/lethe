#pragma once

namespace lethe
{
static constexpr bool nanAware = true;
}

// allow the possibility of building a dynamic library
#if defined(_WIN32) && defined(LETHE_DYNAMIC)
#	if defined(LETHE_BUILD)
#		define LETHE_API __declspec(dllexport)
#	else
#		define LETHE_API __declspec(dllimport)
#	endif
#else
#	define LETHE_API
#endif
