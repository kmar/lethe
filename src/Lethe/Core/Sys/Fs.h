#pragma once

#include "Types.h"
#include "../String/String.h"
#include "../Collect/Array.h"

namespace lethe
{

// FileSystem
struct LETHE_API Fs
{
	// attributes or permissions; different meaning on Windows/Unix
	typedef UInt Attributes;
	static const Attributes INVALID_ATTRIBUTES = 0xffffffffu;

	// get current working directory
	static String GetCwd();
	// change current working directory
	// returns true on success
	static bool ChDir(const String &newDir);
	// make directory
	// returns true on success (attempt to create an existing directory will fail)
	static bool MkDir(const String &newDir);
	// remove directory
	// returns true on success (attempt to remove non-empty or non-existent directory will fail)
	static bool RmDir(const String &oldDir);

	// is file?
	static bool IsFile(const String &fnm);
	// is directory?
	static bool IsDir(const String &fnm);
	// file(dir) exists test
	static bool Exists(const String &fnm);
	// remove file/link
	static bool Unlink(const String &fnm);
	// rename file (don't use this to move file across file systems)
	static bool Rename(const String &oldfn, const String &newfn);
	// attributes/permissions
	static Attributes GetAttributes(const String &fn);
};

}
