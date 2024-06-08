#pragma once

#include "Stream.h"
#include "../String/String.h"
#include "../Sys/Platform.h"

namespace lethe
{

// we never use stupid text mode here
class LETHE_API File : public Stream
{
	LETHE_BUCKET_ALLOC_OVERRIDE(File)
public:
	LETHE_INJECT_STREAM()

	// open mode bit flags
	enum OpenMode
	{
		OM_READ = 1,
		OM_WRITE = 2,
		OM_CREATE = 4,
		OM_TRUNCATE = 8,
		OM_APPEND = 16,
		OM_CREATE_USING_TEMP = 32,
		OM_WRITE_DEFAULT = OM_WRITE|OM_CREATE|OM_TRUNCATE,
		OM_WRITE_SAFE = OM_WRITE_DEFAULT | OM_CREATE_USING_TEMP
	};
	// share mode bit flags
	enum ShareMode
	{
		SM_READ = 1,
		SM_WRITE = 2,
		SM_DELETE = 4,
		SM_ALL = 7
	};

	File();
	File(const String &fnm, int mode = OM_READ, int smode = SM_ALL);
	File(const String &fnm, const char *legacyMode);
	~File();

	// reopen (duplicate) (using new handle)
	// note that the duplicate manages it's own offset! (this is useful for multi-threaded streaming)
	bool Reopen(const File &f);

	virtual bool Open(const String &fnm, int mode = OM_READ, int smode = SM_ALL);
	// this is just a helper
	bool Open(const String &fnm, const char *legacyMode);
	// helper for creating (write-only) files, truncates if existing, creates new otherwise
	bool Create(const String &fnm, int smode = SM_READ);
	// helper for creating (write-only) files, creates temp file then moves it on success [if already exists]
	// note: doesn't work in append mode!
	bool CreateSafe(const String &fnm, int smode = SM_READ);

	bool Read(void *buf, Int size, Int &nread) override;
	bool Write(const void *buf, Int size, Int &nwritten) override;

	bool Close() override;

	// special method used with CreateSafe: call this to prevent moving broken file
	bool Abort();

	UInt GetFlags() const override;

	bool Seek(Long pos, SeekMode mode = SM_SET) override;
	Long Tell() const override;

	bool Flush() override;
	// truncate at current position
	bool Truncate() override;

	Stream *Clone() const override;

#if LETHE_OS_ANDROID
	static void SetAndroidAssetManager(void *amgr);
	static int GetFdForFile(const char *fnm, Long &start, Long &len);
#endif

private:
#if LETHE_OS_ANDROID
	static void *amgr;
	Long startOff;
	Long endOff;
	Long curOff;
#endif

	void *handle;		// OS handle
	String filename;	// necessary for duplicating file access
	UInt flags;
	// so that we find a valid temp index
	Int tempIndex;
	// store mode (necessary for duplicating files)
	int fileMode;
	int fileSMode;
	// convert from str to standard modes
	static bool ConvertMode(const char *legacyMode, int &mode, int &smode);

	// helpers (unity build)
	static bool WriteInternal(void *handle, const void *buf, Int size, Int &nwritten);
	static int FromHandle(const void *h);
	static void *ToHandle(int h);

	String GetTempName(const String &fnm) const;
};

}
