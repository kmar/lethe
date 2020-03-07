#include "Fs.h"

#include "../String/String.h"
#include "../Thread/Lock.h"
#include "../Io/File.h"
#include "Platform.h"
#include "Path.h"
#include "Dir.h"

#if LETHE_OS_WINDOWS
#	include <windows.h>
#	if LETHE_COMPILER_MSC
#		pragma warning(push)
#		pragma warning(disable: 4091) // typedef ignored when on left of ... when no variable is declared
#	endif
#	include <shlobj.h>
#	if LETHE_COMPILER_MSC
#		pragma warning(pop)
#	endif
#else
#	include <unistd.h>
#	include <sys/stat.h>
#	include <stdio.h>
#endif

namespace lethe
{

// Fs

// get current working directory
String Fs::GetCwd()
{
#if LETHE_OS_WINDOWS
	static SpinMutex cwdMutex;
	// not thread-safe => using global mutex
	SpinMutexLock _(cwdMutex);
	DWORD req = GetCurrentDirectoryW(0, 0);
	LETHE_ASSERT(req > 0);
	wchar_t *buf = new wchar_t[req];
	GetCurrentDirectoryW(req, buf);
	String res(buf, buf + req - 1);
	delete[] buf;
	// we want pretty output
	Path::Normalize(res);
	return res;
#else
	char buf[8192];
	buf[0] = 0;

	if (!getcwd(buf, sizeof(buf)))
		return String();

	String res(buf);
	// we want pretty output
	Path::Normalize(res);
	return res;
#endif
}

// change current working directory
// returns true on success
bool Fs::ChDir(const String &newDir)
{
#if LETHE_OS_WINDOWS
	// not thread-safe => using global mutex
	static SpinMutex chdirMutex;
	SpinMutexLock _(chdirMutex);
	WideCharBuffer wbuf;
	return SetCurrentDirectoryW(newDir.ToWide(wbuf)) != FALSE;
#else
	return chdir(newDir.Ansi()) == 0;
#endif
}

// make directory
// returns true on success (attempt to create an existing directory will fail)
bool Fs::MkDir(const String &newDir)
{
#if LETHE_OS_WINDOWS
	WideCharBuffer wbuf;
	return CreateDirectoryW(newDir.ToWide(wbuf), 0) != FALSE;
#else
	// FIXME: better mode?
	return mkdir(newDir.Ansi(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

// remove directory
// returns true on success (attempt to remove non-empty or non-existent directory will fail)
bool Fs::RmDir(const String &oldDir)
{
#if LETHE_OS_WINDOWS
	WideCharBuffer wbuf;
	return RemoveDirectoryW(oldDir.ToWide(wbuf)) != FALSE;
#else
	return rmdir(oldDir.Ansi()) == 0;
#endif
}

// remove file
bool Fs::Unlink(const String &fname)
{
#if LETHE_OS_WINDOWS
	WideCharBuffer wbuf;
	return DeleteFileW(fname.ToWide(wbuf)) != FALSE;
#else
	return unlink(fname.Ansi()) == 0;
#endif
}

// rename file
bool Fs::Rename(const String &oldfn, const String &newfn)
{
#if LETHE_OS_WINDOWS
	WideCharBuffer wbuf1, wbuf2;
	return MoveFileExW(oldfn.ToWide(wbuf1), newfn.ToWide(wbuf2), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != FALSE;
#else
	return rename(oldfn.Ansi(), newfn.Ansi()) == 0;
#endif
}

bool Fs::IsDir(const String &fnm)
{
	Dir d;
	return d.Open(fnm);
}

bool Fs::IsFile(const String &fnm)
{
#if !LETHE_OS_WINDOWS
	// stupid open() seems to succeed on directories?
	// so we use inverse logic here
	LETHE_RET_FALSE(!IsDir(fnm));
#endif
	File f(fnm);
	return f.IsOpen();
}

bool Fs::Exists(const String &fnm)
{
	return GetAttributes(fnm) != INVALID_ATTRIBUTES;
}

Fs::Attributes Fs::GetAttributes(const String &fn)
{
#if LETHE_OS_WINDOWS
	WideCharBuffer wbuf;
	DWORD res = GetFileAttributesW(fn.ToWide(wbuf));
	return res == INVALID_FILE_ATTRIBUTES ? INVALID_ATTRIBUTES : Attributes(res);
#else
	struct stat s;

	if (stat(fn.Ansi(), &s) == -1)
		return INVALID_ATTRIBUTES;

	return s.st_mode;
#endif
}

Array<Fs::MountedDevice> Fs::GetMountedDevices()
{
	Array<MountedDevice> res;
#if LETHE_OS_WINDOWS
	DWORD mask = GetLogicalDrives();

	for (wchar_t i='A'; i<='Z'; i++)
	{
		if (mask & 1)
		{
			MountedDevice md;
			wchar_t wc[2] = { i, 0 };
			String tmp = wc;
			tmp += ':';
			String otmp = tmp;
			// new: try to query volume name
			WCHAR volname[MAX_PATH+1];
			String vi = tmp;
			vi += '\\';
			WideCharBuffer wbuf;

			if (GetVolumeInformationW(vi.ToWide(wbuf), volname, DWORD(sizeof(volname)/sizeof(WCHAR)), 0, 0, 0, 0, 0)
					!= FALSE)
			{
				String vname = volname;

				if (!vname.IsEmpty())
				{
					tmp += " [";
					tmp += vname;
					tmp += "]";
				}
			}

			tmp.Shrink();
			md.name = tmp;
			tmp = otmp;
			tmp += '/';
			tmp.Shrink();
			md.path = tmp;
			res.Add(md);
		}

		mask >>= 1;
	}

#else
	// FIXME: right now we just add root
	MountedDevice md;
	md.name = md.path = "/";
	res.Add(md);
#endif
	return res;
}

#if LETHE_OS_WINDOWS
static void AddWinSpecialFolder(const char *name, int idl, Array<Fs::MountedDevice> &res)
{
	WCHAR path[MAX_PATH+1];
	Fs::MountedDevice sf;

	if (SHGetFolderPathW(0, idl, 0, 0, path) == S_OK)
	{
		sf.name = name;
		sf.path = Path(path);

		if (!sf.path.IsEmpty())
			res.Add(sf);
	}
}
#endif

Array<Fs::MountedDevice> Fs::GetSpecialFolders()
{
	Array<MountedDevice> res;
#if LETHE_OS_WINDOWS
	AddWinSpecialFolder("Documents", CSIDL_MYDOCUMENTS, res);
	AddWinSpecialFolder("Pictures", CSIDL_MYPICTURES, res);
	AddWinSpecialFolder("Desktop", CSIDL_DESKTOPDIRECTORY, res);
#endif
	return res;
}

}
