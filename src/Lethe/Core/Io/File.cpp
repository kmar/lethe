#include "File.h"
#include "../Sys/Platform.h"
#include "../Sys/Fs.h"

#if LETHE_OS_WINDOWS
#	include <windows.h>
#else
#	if !defined(_LARGEFILE_SOURCE)
#		define _LARGEFILE_SOURCE
#	endif
#	if !defined(_LARGEFILE64_SOURCE)
#		define _LARGEFILE64_SOURCE
#	endif
#	if !defined(_LARGEFILE_API)
#		define _LARGEFILE_API
#	endif
#	include <fcntl.h>
#	include <unistd.h>
#	include <sys/stat.h>

#	if LETHE_OS_ANDROID
#		define open64 open
#		include <android/asset_manager.h>
#	elif LETHE_OS_APPLE || LETHE_OS_IOS || LETHE_OS_BSD
#		define open64 open
#		define lseek64 lseek
#		define ftruncate64 ftruncate
#	endif

#endif

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(File)

// File

int File::FromHandle(const void *h)
{
	return (int)(intptr_t)h-1;
}

void *File::ToHandle(int h)
{
	return (void *)((intptr_t)h+1);
}

File::File() : handle(0), flags(0), tempIndex(0), fileMode(0), fileSMode(0)
{
}

File::File(const String &fnm, int mode, int smode) : handle(0), flags(0), tempIndex(0), fileMode(0), fileSMode(0)
{
	Open(fnm, mode, smode);
}

File::File(const String &fnm, const char *legacyMode) : handle(0), flags(0), tempIndex(0), fileMode(0), fileSMode(0)
{
	Open(fnm, legacyMode);
}

File::~File()
{
	Close();
}

// helper for creating (write-only) files, truncates if existing, creates new otherwise
bool File::Create(const String &fnm, int smode)
{
	return Open(fnm, OM_WRITE_DEFAULT, smode);
}

bool File::CreateSafe(const String &fnm, int smode)
{
	return Open(fnm, OM_WRITE_SAFE, smode);
}

bool File::Open(const String &fnm, int mode, int smode)
{
	if (LETHE_UNLIKELY(IsOpen() && !Close()))
		return 0;

	if (!(mode & (OM_CREATE|OM_WRITE)) || (mode & (OM_READ | OM_APPEND)))
		mode &= ~OM_CREATE_USING_TEMP;

	const bool usingTemp = (mode & OM_CREATE_USING_TEMP) != 0;

	// when using temp, try to look for a valid non-existent filename (for max safety)
	while (usingTemp && Fs::Exists(GetTempName(fnm)))
		++tempIndex;

#if LETHE_OS_WINDOWS
	DWORD access = 0;

	if ((mode & OM_READ) != 0)
	{
		access |= GENERIC_READ;
		flags |= SF_READABLE;
	}

	if ((mode & OM_WRITE) != 0)
	{
		access |= GENERIC_WRITE;
		flags |= SF_WRITABLE;
	}

	DWORD share = 0;

	if ((smode & SM_READ) != 0)
		share |= FILE_SHARE_READ;

	if ((smode & SM_WRITE) != 0)
		share |= FILE_SHARE_WRITE;

	if ((smode & SM_DELETE) != 0)
		share |= FILE_SHARE_DELETE;

	DWORD creat = OPEN_EXISTING;

	if ((mode & OM_CREATE) != 0)
		creat = CREATE_NEW;

	if ((mode & OM_TRUNCATE) != 0)
		creat = creat == CREATE_NEW ? CREATE_ALWAYS : TRUNCATE_EXISTING;

	WideCharBuffer wbuf;
	handle = (void *)CreateFileW(usingTemp ? GetTempName(fnm).ToWide(wbuf) : fnm.ToWide(wbuf), access, share, 0, creat, FILE_ATTRIBUTE_NORMAL, 0);

	if (LETHE_UNLIKELY(handle == INVALID_HANDLE_VALUE))
	{
		flags = 0;
		return 0;
	}

	flags |= SF_OPEN | SF_SEEKABLE;

	if ((mode & OM_APPEND) != 0)
		flags |= SF_APPENDABLE;

	fileMode = mode;
	fileSMode = smode;
	filename = fnm;
	return 1;
#else
	int oflag;

	switch(mode & (OM_READ|OM_WRITE))
	{
	case OM_READ:
		oflag = O_RDONLY;
		flags |= SF_READABLE;
		break;

	case OM_WRITE:
		oflag = O_WRONLY;
		flags |= SF_WRITABLE;
		break;

	case OM_READ|OM_WRITE:
		oflag = O_RDWR;
		flags |= SF_READABLE | SF_WRITABLE;
		break;

	default:
		// illegal
		return 0;
	}

	// now handle create/trunc
	if ((mode & OM_CREATE) != 0)
		oflag |= O_CREAT | O_EXCL;

	if ((mode & OM_TRUNCATE) != 0)
	{
		oflag &= ~O_EXCL;
		oflag |= O_TRUNC;
	}

	// sharing mode ignored on non-Windows OSes
	(void)smode;
	int fd = open64(usingTemp ? GetTempName(fnm).Ansi() : fnm.Ansi(), oflag, S_IRUSR|S_IWUSR);

#if LETHE_OS_ANDROID
	startOff = endOff = curOff = -1;

	if (fd < 0 && !(mode & OM_WRITE))
	{
		fd = GetFdForFile(fnm.Ansi(), startOff, endOff);

		if (fd >= 0)
		{
			endOff += startOff;
			lseek64(fd, (off64_t)startOff, SEEK_SET);
			curOff = startOff;
		}
	}

#endif

	if (fd < 0)
	{
		flags = 0;
		return 0;
	}

	flags |= SF_OPEN | SF_SEEKABLE;

	if ((mode & OM_APPEND) != 0)
		flags |= SF_APPENDABLE;

	handle = ToHandle(fd);
	fileMode = mode;
	fileSMode = smode;
	filename = fnm;
	return 1;
#endif
}

bool File::Open(const String &fnm, const char *legacyMode)
{
	int mode;
	int smode;

	if (!ConvertMode(legacyMode, mode, smode))
		return 0;

	if (mode & OM_APPEND)
	{
		if (Fs::Exists(fnm))
			mode &= ~(UInt)OM_CREATE;
	}

	return Open(fnm, mode, smode);
}

bool File::Read(void *buf, Int size, Int &nread)
{
	LETHE_ASSERT(buf && size >= 0);
#if LETHE_OS_WINDOWS
	DWORD nr = 0;
	bool res = ReadFile((HANDLE)handle, (LPVOID)buf, (DWORD)size, &nr, 0) != FALSE;

	if (LETHE_LIKELY(res))
		nread = nr;

	return res;
#else
#	if LETHE_OS_ANDROID

	if (startOff >= 0 && curOff + size > endOff)
		size = Int(endOff - curOff);

#	endif
	ssize_t res = read(FromHandle(handle), buf, (size_t)size);

	if (LETHE_LIKELY(res >= 0))
	{
		nread = (Int)res;
#	if LETHE_OS_ANDROID

		if (startOff >= 0)
			curOff += nread;

#	endif
	}

	return res >= 0;
#endif
}

bool File::WriteInternal(void *fhandle, const void *buf, Int size, Int &nwritten)
{
#if LETHE_OS_WINDOWS
	DWORD nw = 0;
	bool res = WriteFile((HANDLE)fhandle, (LPCVOID)buf, (DWORD)size, &nw, 0) != FALSE;

	if (LETHE_LIKELY(res))
		nwritten = (Int)nw;

	return res;
#else
	ssize_t res = write(FromHandle(fhandle), buf, (size_t)size);

	if (res >= 0)
		nwritten = (Int)res;

	return res >= 0;
#endif
}

String File::GetTempName(const String &fnm) const
{
	return fnm + String::Printf("_#temp_%d", tempIndex);
}

bool File::Write(const void *buf, Int size, Int &nwritten)
{
	LETHE_ASSERT(buf && size >= 0);

	if ((flags & SF_APPENDABLE) != 0)
	{
		// handle append mode
		Long cur = Tell();
		bool res = 0;

		if (SeekEnd())
			res = WriteInternal(handle, buf, size, nwritten);

		Seek(cur);
		return res;
	}

	return WriteInternal(handle, buf, size, nwritten);
}

bool File::Close()
{
	// ok to close closed file
	if (LETHE_UNLIKELY(!(flags & SF_OPEN)))
		return 1;

#if LETHE_OS_WINDOWS

	if (CloseHandle((HANDLE)handle) == FALSE)
		return 0;

#else

	if (close(FromHandle(handle)) != 0)
		return 0;

#endif

	bool res = true;

	if (fileMode & OM_CREATE_USING_TEMP)
	{
		if (!Fs::Rename(GetTempName(filename), filename))
		{
			Fs::Unlink(GetTempName(filename));
			res = false;
		}
	}

	flags = 0;
	filename.Clear();
	return res;
}

bool File::Abort()
{
	if (!(flags & SF_OPEN) || !(fileMode & OM_CREATE_USING_TEMP))
		return true;

	fileMode &= ~OM_CREATE_USING_TEMP;

	bool res = Close();

	if (!Fs::Unlink(GetTempName(filename)))
		res = false;

	return res;
}

UInt File::GetFlags() const
{
	return flags;
}

bool File::Seek(Long pos, SeekMode mode)
{
#if LETHE_OS_WINDOWS
	LARGE_INTEGER delta;
	delta.QuadPart = pos;
	DWORD mm;

	switch(mode)
	{
	case SM_CUR:
		mm = FILE_CURRENT;
		break;

	case SM_END:
		mm = FILE_END;
		break;

	default:
		mm = FILE_BEGIN;
	}

	return SetFilePointerEx((HANDLE)handle, delta, 0, mm) != FALSE;
#else
	int mm;

	switch(mode)
	{
	case SM_CUR:
		mm = SEEK_CUR;
		break;

	case SM_END:
		mm = SEEK_END;
		break;

	default:
		mm = SEEK_SET;
	}

#	if LETHE_OS_ANDROID

	if (startOff >= 0)
	{
		switch(mm)
		{
		default:
			pos = startOff + pos;
			break;

		case SEEK_CUR:
			pos = curOff + pos;
			break;

		case SEEK_END:
			pos = endOff + pos;
		}

		curOff = pos = Clamp(pos, startOff, endOff);
	}

#	endif
	return lseek64(FromHandle(handle), pos, mm) >= 0;
#endif
}

Long File::Tell() const
{
#if LETHE_OS_WINDOWS
	LARGE_INTEGER delta;
	delta.QuadPart = 0;
	LARGE_INTEGER newp;
	newp.QuadPart = 0;
	bool res = SetFilePointerEx((HANDLE)handle, delta, &newp, FILE_CURRENT) != FALSE;

	if (LETHE_UNLIKELY(!res))
		return -1;

	return (Long)newp.QuadPart;
#else
#	if LETHE_OS_ANDROID

	if (startOff >= 0)
		return curOff - startOff;

#	endif
	return lseek64(FromHandle(handle), 0, SEEK_CUR);
#endif
}

bool File::Flush()
{
	if (!IsWritable())
		return 1;

#if LETHE_OS_WINDOWS
	return FlushFileBuffers((HANDLE)handle) != FALSE;
#else
	return fsync(FromHandle(handle)) == 0;
#endif
}

// truncate at current position
bool File::Truncate()
{
	if (!IsWritable())
		return 1;

#if LETHE_OS_WINDOWS
	return SetEndOfFile((HANDLE)handle) != FALSE;
#else
	return ftruncate64(FromHandle(handle), Tell()) == 0;
#endif
}

// convert from str to standard modes
bool File::ConvertMode(const char *legacyMode, int &mode, int &smode)
{
	if (LETHE_UNLIKELY(!legacyMode))
		return 0;

	mode = 0;
	smode = SM_ALL;

	while (*legacyMode)
	{
		char c = *legacyMode++;

		switch(c)
		{
		case 'r':
			mode |= OM_READ;
			break;

		case 'w':
			mode |= OM_WRITE | OM_CREATE | OM_TRUNCATE;
			break;

		case '+':
			if ((mode & OM_APPEND) != 0)
				mode &= ~OM_TRUNCATE;

			mode |= OM_READ | OM_WRITE;
			break;

		case 'b':
			break;

		case 'a':
			mode |= OM_READ | OM_WRITE | OM_APPEND | OM_CREATE;
			break;

		case 'x':
			mode &= ~OM_TRUNCATE;
			break;
		}
	}

	return 1;
}

// open duplicate (using new handle)
bool File::Reopen(const File &f)
{
	return Open(f.filename, f.fileMode, f.fileSMode);
}

Stream *File::Clone() const
{
	File *res = new File;
	Long pos = Tell();

	if (!res->Reopen(*this) || !res->Seek(pos, SM_BEG))
	{
		// unexpected failure
		delete res;
		return 0;
	}

	return res;
}

#if LETHE_OS_ANDROID
void *File::amgr = 0;

int File::GetFdForFile(const char *fnm, Long &nstart, Long &nlen)
{
	AAsset *asset = AAssetManager_open(static_cast<AAssetManager *>(File::amgr), fnm, AASSET_MODE_UNKNOWN);

	if (!asset)
		return -1;

	off_t start, len;
	int fd = AAsset_openFileDescriptor(asset, &start, &len);

	if (fd >= 0)
	{
		nstart = start;
		nlen = len;
	}

	return fd;
}

void File::SetAndroidAssetManager(void *namgr)
{
	amgr = namgr;
}
#endif

}
