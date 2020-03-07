#include "Assert.h"
#include "Likely.h"
#include "Platform.h"
#include "../String/String.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#if LETHE_OS_WINDOWS
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#	include <Windows.h>
#endif

namespace lethe_DebugLog
{

void DebugLog(const char *msg);

}

namespace lethe
{

// trap into debugger (where available)
void DebuggerTrap()
{
#if LETHE_OS_WINDOWS
	::DebugBreak();
#endif
}

void AbortProgramAssert(const char *msg, const char *file, int line, int exitCode)
{
	char buf[4096];
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC
	sprintf_s(buf, sizeof(buf), "Assertion failed: %s at line %d in file %s", msg, line, file);
	buf[sizeof(buf)-1] = 0;
#else
	sprintf(buf, "Assertion failed: %s at line %d in file %s", msg, line, file);
#endif
	AbortProgram(buf, exitCode);
}

// aborts program
void AbortProgram(const char *msg, int exitCode)
{
	printf("%s\n", msg);

#if LETHE_DEBUG
	DebuggerTrap();
#endif
	::exit(exitCode);
}

}
