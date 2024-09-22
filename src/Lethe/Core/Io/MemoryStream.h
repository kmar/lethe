#pragma once

#include "Stream.h"
#include "../Collect/Array.h"

namespace lethe
{

class String;

LETHE_API_BEGIN

// by default, MemoryStream is backed by Array<Byte> for RW access
// note that MemoryStream is always open
// MemoryStream cannot handle more than 2G-1 bytes of data, even in 64-bit mode!
class LETHE_API MemoryStream : public Stream
{
public:
	LETHE_INJECT_STREAM()

	// dynamic ctor
	MemoryStream();
	// const buffer ctor
	MemoryStream(const void *rbuff, Int rbSize);
	// const buffer ctor from zero-terminated string
	MemoryStream(const char *str);

	// open (either dynamic or const buffer)
	void Open(const void *buf = 0, Int sz = 0);

	bool Read(void *buf, Int size, Int &nread) override;
	bool Write(const void *buf, Int size, Int &nwritten) override;

	UInt GetFlags() const override;

	bool Seek(Long pos, SeekMode mode = SM_SET) override;
	Long Tell() const override;
	Long GetSize() override;

	// truncate at current position
	bool Truncate() override;

	// get data (const buffer pointer - may be null for empty memory stream)
	const void *GetData() const;
	// allows access to internal buffer. careful here!
	Array<Byte> &GetInternalBuffer();

	// reserve/resize (only valid in dynamic mode)
	bool Reserve(Int res);
	bool Resize(Int sz);
	// Clear keeps current reserve while Reset frees up allocated memory
	bool Clear();
	bool Reset();

	String ToString() const;

	// to be able to read bytes fast
	inline Int ReadByte()
	{
		const Byte *ptr = refBuf ? refBuf : data.GetData();
		Int sz = refBuf ? refBufSize : data.GetSize();

		if (LETHE_LIKELY(sz > cpos))
			return ptr[cpos++];

		Byte b;

		if (LETHE_LIKELY(Read(&b, 1)))
			return b;

		column--;
		return -1;
	}

	inline Int GetByte() override
	{
		column++;

		if (LETHE_UNLIKELY(!ungetBuf.IsEmpty()))
		{
			Int res = ungetBuf.Back();
			ungetBuf.Pop();
			return res;
		}

		return ReadByte();
	}

	Stream *Clone() const override;

private:
	const Byte *refBuf;		// ref buffer (0 = dynamic mode)
	Array<Byte> data;		// default data (RW stream)
	Int cpos;				// current stream pos
	Int refBufSize;			// ref buffer size
};

LETHE_API_END

}
