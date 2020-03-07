#pragma once

#include "Path.h"
#include "NoCopy.h"

namespace lethe
{

struct DirEntry
{
	String name;
	bool isDirectory;
	bool isLink;
};

enum DirEnumFlags
{
	// enumerate .. (. is never enumerated)
	DIR_ENUM_DOTS		=	1,
	DIR_ENUM_LINKS		=	2,
	DIR_ENUM_DEFAULT	=	DIR_ENUM_DOTS,
	DIR_ENUM_ALL		=	DIR_ENUM_DOTS | DIR_ENUM_LINKS
};

// directory enumerator
class LETHE_API Dir : NoCopy
{
public:
	Dir();
	Dir(const String &pth);
	~Dir();
	// open directory
	bool Open(const String &pth, UShort nflags = DIR_ENUM_DEFAULT);
	// open virtual directory, only path-related functions can be used
	bool OpenVirtual(const String &pth, UShort nflags = DIR_ENUM_DEFAULT);
	// enumerate next entry
	// note: never enumerates .
	bool Next(DirEntry &de);
	// close directory
	bool Close();
	// get full pathname (if possible)
	String GetFullName(const DirEntry &e);
	// get relative pathname (if possible)
	String GetRelativeName(const DirEntry &e, const String &refPath);

private:
	void *handle;
	void *handle2;
	Path path;
	UShort flags;		// enum flags
	bool valid;			// because of Windows
	bool root;			// root dir flag on Unix
	bool isVirtual;		// virtual dir inside archive
};

}
