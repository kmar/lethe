#pragma once

#include "Stream.h"
#include "../Collect/Array.h"
#include "../Sys/Likely.h"

namespace lethe
{

// as usual, not thread-safe!
// note: for performance reasons Flush() doesn't flush read buffers, neither does Close => underlying stream's
// position won't be in sync once BufferedStream dies
class LETHE_API BufferedStream : public Stream
{
	LETHE_BUCKET_ALLOC_OVERRIDE(BufferedStream)
public:
	LETHE_INJECT_STREAM()

	BufferedStream();
	explicit BufferedStream(Stream &refs, Int buffSize = 8192);
	~BufferedStream();

	// set reference stream
	bool SetStream(Stream &refs);
	// set buffer size
	bool SetBufferSize(Int nbuffSize);

	bool Read(void *buf, Int size, Int &nread) override;
	bool Write(const void *buf, Int size, Int &nwritten) override;

	// simply returns underlying stream flags
	UInt GetFlags() const override;

	bool Close() override;
	bool Flush() override;
	bool Truncate() override;

	bool Rewind() override;
	bool SeekEnd() override;
	bool Seek(Long pos, SeekMode mode = SM_SET) override;
	Long Tell() const override;

	Long GetSize() override;

	// to be able to read bytes fast
	inline Int ReadByte()
	{
		if (LETHE_LIKELY(rdBuffPtr < rdBuffTop))
			return rdBuffer[rdBuffPtr++];

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

private:
	Long cpos;			// current stream position
	UInt flags;			// cached refStream flags
	Stream *stream;
	Int buffSize;
	// this is read buffer
	Array< Byte > rdBuffer;
	Int rdBuffPtr;
	Int rdBuffTop;
	Int rdBuffSize;
	// this is write buffer
	Array< Byte > wrBuffer;
	Int wrBuffPtr;
	Int wrBuffSize;

	void FlushRead();
};

}
