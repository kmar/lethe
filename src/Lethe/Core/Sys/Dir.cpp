#include "Dir.h"
#include "Path.h"
#include "Platform.h"
#include "Fs.h"
#include "../Thread/Lock.h"

#if LETHE_OS_WINDOWS
#	include <windows.h>
#else
#	include <unistd.h>
#	include <sys/stat.h>
#	include <dirent.h>
#endif

namespace lethe
{

// Dir

#if LETHE_OS_WINDOWS
static WIN32_FIND_DATAW *FromHandle(void *h)
{
	return (WIN32_FIND_DATAW *)h;
}

static void *ToHandle(WIN32_FIND_DATAW *h)
{
	return (void *)h;
}

static HANDLE FromHandle2(void *h)
{
	return (HANDLE)h;
}

static void *ToHandle2(HANDLE h)
{
	return (void *)h;
}
#else
static DIR *FromHandle(void *h)
{
	return (DIR *)h;
}

static void *ToHandle(DIR *h)
{
	return (void *)h;
}
#endif

Dir::Dir()
	: handle(0)
	, handle2(0)
	, flags(0)
	, valid(0)
	, root(0)
	, isVirtual(0)
{
	(void)root;
#if LETHE_OS_WINDOWS
	handle2 = ToHandle2(INVALID_HANDLE_VALUE);
	handle = ToHandle(new WIN32_FIND_DATAW);
#endif
}

Dir::Dir(const String &pth)
	: handle(0)
	, handle2(0)
	, flags(0)
	, valid(0)
	, root(0)
	, isVirtual(0)
{
#if LETHE_OS_WINDOWS
	handle2 = ToHandle2(INVALID_HANDLE_VALUE);
	handle = ToHandle(new WIN32_FIND_DATAW);
#endif
	Open(pth);
}

Dir::~Dir()
{
	Close();
#if LETHE_OS_WINDOWS
	delete FromHandle(handle);
#endif
}

#if LETHE_OS_WINDOWS

typedef struct _REPARSE_DATA_BUFFER
{
	ULONG  ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union
	{
		struct
		{
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			ULONG  Flags;
			WCHAR  PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct
		{
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			WCHAR  PathBuffer[1];
		} MountPointReparseBuffer;
		struct
		{
			UCHAR DataBuffer[1];
		} GenericReparseBuffer;
	};
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

static bool WinResolveReparsePointDir(String &dnm)
{
	HANDLE th = INVALID_HANDLE_VALUE;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &th) == FALSE)
		return 0;

	LUID luid;
	{
		String sname = SE_BACKUP_NAME;
		WideCharBuffer snamew;

		if (LookupPrivilegeValueW(0, sname.ToWide(snamew), &luid) == FALSE)
		{
			CloseHandle(th);
			return 0;
		}
	}
	TOKEN_PRIVILEGES tp, oldtp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	DWORD oldlen = sizeof(TOKEN_PRIVILEGES);
	bool adjusted = AdjustTokenPrivileges(th, 0, &tp, sizeof(tp), &oldtp, &oldlen) != FALSE;

	if (!adjusted)
	{
		CloseHandle(th);
		return 0;
	}

	// opening reparse point, first arg must be 0
	WideCharBuffer wbuf;
	HANDLE hf = CreateFileW(dnm.ToWide(wbuf), 0, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
							OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT, 0);

	AdjustTokenPrivileges(th, 0, &oldtp, oldlen, 0, 0);
	CloseHandle(th);

	if (hf == INVALID_HANDLE_VALUE)
		return 0;

	Array<Byte> buf(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	DWORD bytesRet = 0;

	// warning: specifying null ptr for bytesRet causes a crash under W7!
	if (DeviceIoControl(hf, FSCTL_GET_REPARSE_POINT, 0, 0,
						buf.GetData(), DWORD(buf.GetSize()), &bytesRet, 0) == FALSE || !bytesRet)
	{
		CloseHandle(hf);
		return 0;
	}

	const REPARSE_DATA_BUFFER *rpb = reinterpret_cast<const REPARSE_DATA_BUFFER *>(buf.GetData());

	if (rpb->ReparseTag == IO_REPARSE_TAG_SYMLINK)
	{
		// note: this is for files only, not really needed here?
		const WCHAR *src = rpb->SymbolicLinkReparseBuffer.PathBuffer + rpb->SymbolicLinkReparseBuffer.SubstituteNameOffset;
		Int len = rpb->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
		dnm = String(src, src + len);
	}
	else if (rpb->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
	{
		const WCHAR *src = rpb->MountPointReparseBuffer.PathBuffer + rpb->MountPointReparseBuffer.SubstituteNameOffset;
		Int len = rpb->MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
		dnm = String(src, src + len);
	}
	else
		return 0;

	if (dnm.StartsWith("\\??\\"))
		dnm.Erase(0, 4);

	CloseHandle(hf);
	return 1;
}
#endif

// open virtual directory
bool Dir::OpenVirtual(const String &pth, UShort nflags)
{
	Close();
	flags = nflags;
	path.Make(pth);
	isVirtual = 1;
#if !LETHE_OS_WINDOWS
	const String &pname = path.Get();
	root = pname == "/";
#endif
	return 1;
}

// open directory
bool Dir::Open(const String &pth, UShort nflags)
{
	Close();
	isVirtual = 0;
	flags = nflags;
	path.Make(pth);
#if LETHE_OS_WINDOWS
	Path tmp(path);
	tmp.Append("*");
	WideCharBuffer tmpw;
	HANDLE h = FindFirstFileW(tmp.Get().ToWide(tmpw), FromHandle(handle));

	if (h == INVALID_HANDLE_VALUE)
	{
		DWORD attr = GetFileAttributesW(path.Get().ToWide(tmpw));

		if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			// thank you!!!
			WIN32_FIND_DATAW fd;
			HANDLE sh = FindFirstFileW(path.Get().ToWide(tmpw), &fd);

			if (sh != INVALID_HANDLE_VALUE)
			{
				FindClose(sh);
				DWORD tag = fd.dwReserved0;

				if (tag == IO_REPARSE_TAG_MOUNT_POINT)
				{
					// got symlink but HOW the fuck do I actually resolve it?!
					String str = path.Get();

					if (WinResolveReparsePointDir(str))
					{
						tmp.Make(str);
						path = tmp;
						tmp.Append("*");
						h = FindFirstFileW(tmp.Get().ToWide(tmpw), FromHandle(handle));
					}
				}
			}
		}
	}

	valid = h != INVALID_HANDLE_VALUE;
	handle2 = ToHandle2(h);
	return valid;
#else
	const String &pname = path.Get();
	root = pname == "/";
	struct stat st;

	if (lstat(pname.Ansi(), &st) != -1)
	{
		if (S_ISLNK(st.st_mode))
		{
			// symlink
			Array<char> buf((Int) st.st_size + 1);
			ssize_t lsz = readlink(pname.Ansi(), buf.GetData(), buf.GetSize());

			if (lsz >= 0)
			{
				path.Make(buf.GetData());
				String tname = path.Get();
				root = tname == "/";
				// link requires special handling
				handle = ToHandle(opendir(tname.IsEmpty() ? "." : tname.Ansi()));
				return handle != 0;
			}
		}
	}

	handle = ToHandle(opendir(pname.IsEmpty() ? "." : pname.Ansi()));
	return handle != 0;
#endif
}

// enumerate next entry
bool Dir::Next(DirEntry &de)
{
	// note: automatically skips .
#if LETHE_OS_WINDOWS
	bool cont;

	do
	{
		if (!valid)
			valid = FindNextFileW(FromHandle2(handle2), FromHandle(handle)) != FALSE;

		if (!valid)
			return 0;

		WIN32_FIND_DATAW *fd = FromHandle(handle);
		de.name = fd->cFileName;
		DWORD attr = fd->dwFileAttributes;
		de.isDirectory = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		de.isLink = (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		valid = 0;
		cont = de.isLink && !(flags & DIR_ENUM_LINKS);
	}
	while (cont || (de.isDirectory && (de.name == "." || (!(flags & DIR_ENUM_DOTS) && de.name == ".."))));

	return 1;
#else

	// readdir isn't thread-safe
	if (!handle)
		return 0;

	MutexLock _(Mutex::Get());
	bool cont;

	do
	{
		cont = 0;
		struct dirent *d = readdir(FromHandle(handle));

		if (!d)
			return 0;

		de.name = d->d_name;
		de.isDirectory = d->d_type == DT_DIR;
		de.isLink = d->d_type == DT_LNK;

		if (de.isLink)
		{
			if (!(flags & DIR_ENUM_LINKS))
			{
				cont = 1;
				continue;
			}

			String fname = GetFullName(de);
			struct stat st;

			if (stat(fname.Ansi(), &st) != -1 && S_ISDIR(st.st_mode))
			{
				;
				de.isDirectory = 1;
			}
		}

		// on Unix we want to skip .. for root to unify behavior with Windows
	}
	while (cont || (de.isDirectory && (de.name == "." || ((root || !(flags & DIR_ENUM_DOTS)) && de.name == ".."))));

	return 1;
#endif
}

// close directory
bool Dir::Close()
{
	valid = 0;
	path.Clear();
#if LETHE_OS_WINDOWS

	if (FromHandle2(handle2) != INVALID_HANDLE_VALUE)
	{
		bool res = FindClose(FromHandle2(handle2)) != FALSE;
		handle2 = ToHandle2(INVALID_HANDLE_VALUE);
		return res;
	}

#else

	if (handle)
	{
		bool res = closedir(FromHandle(handle)) == 0;
		handle = 0;
		return res;
	}

	// silence unused warning
	(void)handle2;
#endif
	return 0;
}

// get full pathname (if possible)
String Dir::GetFullName(const DirEntry &e)
{
	Path tmp;

	if (!isVirtual && path.IsRelative())
		tmp.Make(Fs::GetCwd());

	tmp.Append(path);
	tmp.Append(e.name);
	return tmp;
}

String Dir::GetRelativeName(const DirEntry &e, const String &refPath)
{
	Path tmp;

	if (!isVirtual && path.IsRelative())
		tmp.Make(Fs::GetCwd());

	Path rp = refPath;

	if (!isVirtual && rp.IsRelative())
	{
		tmp.Make(Fs::GetCwd());
		tmp.Append(rp);
		rp = tmp;
	}

	tmp.Append(path);
	tmp.Append(e.name);
	tmp.MakeRelative(rp.Get());
	return tmp;
}

}
