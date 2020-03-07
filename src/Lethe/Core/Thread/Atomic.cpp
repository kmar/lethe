#include "Atomic.h"
#include "../Sys/Platform.h"

namespace lethe
{

// Atomic

void Atomic::Pause()
{
#if LETHE_OS_WINDOWS
#	ifndef YieldProcessor
#		define YieldProcessor()		// MinGW
#	endif
	YieldProcessor();
#else
	;		// FIXME: better?!
#endif
}

}
