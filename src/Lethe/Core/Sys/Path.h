#pragma once

#include "../String/String.h"
#include "Singleton.h"

namespace lethe
{

class LETHE_API Path
{
public:
	Path();
	Path(const String &pth);

	// split path
	Path &Split(String &drv, String &path, String &name, String &ext);
	// make path
	Path &Make(const String &drv, String &path, String &name, String &ext);
	Path &Make(const String &pth);

	// set extension
	Path &SetExt(const String &ext);
	// get extension (without .)
	String GetExt() const;

	// get filename
	String GetFilename() const;

	// clear path
	Path &Clear();
	// reset path (free allocated memory)
	Path &Reset();
	// reduce mem usage
	Path &Shrink();

	// append path (string)
	Path &Append(const String &pth);

	// get (normalized) path string
	const String &Get() const;

	// normalize path string: convert backslashes to slashes, remove trailing whitespace,
	// remove quotes, remove trailing slash etc.
	// problem: UNC paths?
	static void Normalize(String &pth);
	Path &Normalize();

	// convert to absolute path
	// returns true on success
	bool MakeAbsolute(const String &refpth);
	// convert to relative path
	// returns true on success
	bool MakeRelative(const String &refpth);

	// predicates
	bool IsAbsolute() const;
	bool IsRelative() const;
	// has drive prefix?
	bool HasPrefix() const;
	// get drive prefix, doesn't include :
	String GetPrefix() const;
	// remove drive prefix if any
	Path &RemovePrefix();

	// TODO: add some String operators and Wide/Ansi

private:
	String finalPath;

	Int FindPrefix() const;
	Int GetExtPos() const;
};

}
