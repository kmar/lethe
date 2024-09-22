#pragma once

#include "../Collect/Array.h"
#include "../Sys/NoCopy.h"
#include "../Sys/Likely.h"
#include "../Ptr/RefCounted.h"
#include "../Memory/BucketAlloc.h"

namespace lethe
{

// helper macro
#define LETHE_INJECT_STREAM() using lethe::Stream::Read;	\
	using lethe::Stream::Write;

struct SerializeContext;

LETHE_API_BEGIN

// note: Streams are NOT thread-safe by design!
class LETHE_API Stream : NoCopy, public RefCounted
{
public:
	// stream bit flags
	enum StreamFlags
	{
		SF_OPEN = 1,
		SF_READABLE = 2,
		SF_WRITABLE = 4,
		SF_SEEKABLE = 8,
		SF_APPENDABLE = 16,
		SF_BUFFERED = 32
	};
	// stream seek mode
	enum SeekMode
	{
		SM_SET = 0,
		SM_BEG = 0,
		SM_CUR = 1,
		SM_END = 2
	};

	Stream();
	virtual ~Stream();
	// if buf is null or size <0, read is undefined (may crash!)
	// should read fail, nread contents are undefined
	virtual bool Read(void *buf, Int size, Int &nread);
	// if buf is null or size <0, write is undefined (may crash!)
	// write of 0 bytes is implementation defined and may succeed even for read-only streams!
	// should write fail, nwritten contents are undefined
	virtual bool Write(const void *buf, Int size, Int &nwritten);

	// require_success_version
	// only succeeds if size bytes are read
	inline bool Read(void *buf, Int size)
	{
		Int nr;
		return Read(buf, size, nr) && nr == size;
	}

	// require_success_version
	// only succeeds if size bytes are read
	inline bool Write(const void *buf, Int size)
	{
		Int nw;
		return Write(buf, size, nw) && nw == size;
	}

	// skip bytes (read)
	bool SkipRead(Long bytes);
	// skip bytes (write, writes zeros)
	bool SkipWrite(Long bytes);

	virtual bool Close();

	// flags
	virtual UInt GetFlags() const;
	inline bool IsOpen() const
	{
		return (GetFlags() & SF_OPEN) != 0;
	}
	inline bool IsReadable() const
	{
		return (GetFlags() & SF_READABLE) != 0;
	}
	inline bool IsWritable() const
	{
		return (GetFlags() & SF_WRITABLE) != 0;
	}
	inline bool IsSeekable() const
	{
		return (GetFlags() & SF_SEEKABLE) != 0;
	}
	inline bool IsAppendable() const
	{
		return (GetFlags() & SF_APPENDABLE) != 0;
	}

	// seeking:
	virtual bool Rewind();
	virtual bool SeekEnd();
	// returns 0 on error
	virtual bool Seek(Long pos, SeekMode mode = SM_SET);
	// returns -1 on error
	virtual Long Tell() const;
	// GetSize not const because it's implemented using Seek/Tell by default
	// returns -1 on error
	virtual Long GetSize();

	// returns 0 on error
	virtual bool Flush();
	// truncate at current position
	// returns 0 on error
	virtual bool Truncate();

	// is end of file? - returns 0 for non-seekable streams
	virtual bool IsEof();

	// create in independent clone (useful especially for files)
	// returns 0 if cannot be cloned, otherwise creates a new instance (must be deleted by caller)
	virtual Stream *Clone() const;

	// helpers:

	// returns -1 on error/eof
	virtual Int GetByte();
	inline virtual void UngetByte(Int b)
	{
		// unget EOF is ok
		if (LETHE_LIKELY(b >= 0))
		{
			ungetBuf.Add(b);
			column--;
		}
	}
	// support for text parsers (GetByte)
	inline void SetColumn(Int ncolumn)
	{
		column = ncolumn;
	}
	inline Int GetColumn() const
	{
		return column;
	}

	// read whole stream, uses local buffer if not seekable, otherwise reads whole stream
	// (and repositions to the beginning)
	// limit: if stream contains more than limit bytes, ReadAll will fail
	// extraReserve: reserve this many extra bytes
	// note: extraReserve only impacts seekable streams
	bool ReadAll(Array<Byte> &buf, Int extraReserve = 0, Int limit = 0x7fffffff);
	// append stream at current position, uses local 8k buffer
	// length: optional length to append
	bool Append(Stream &s, Long length = -1);

	// custom context to simplify serialization
	inline SerializeContext *GetContext()
	{
		return context;
	}
	inline void SetContext(SerializeContext *ctx)
	{
		context = ctx;
	}

protected:
	// context refptr
	SerializeContext *context;
	Array<Int> ungetBuf;
	Int column;
};

LETHE_API_END

}
