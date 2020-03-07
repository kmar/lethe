#pragma once

#include "Stream.h"
#include "../Collect/Array.h"
#include "../Sys/Likely.h"

namespace lethe
{

// as usual, not thread-safe!
class LETHE_API BufferedStream : public Stream
{
public:
	LETHE_INJECT_STREAM()

	BufferedStream();
	explicit BufferedStream(Stream &refs, Int buffSize = 8192);
	~BufferedStream();

	// set reference stream
	bool SetStream(Stream &refs);
	// set buffer size
	bool SetBufferSize(Int nbuffSize);

	bool Read(void *buf, Int size, Int &nread);
	bool Write(const void *buf, Int size, Int &nwritten);

	// simply returns underlying stream flags
	UInt GetFlags() const;

	bool Close();
	bool Flush();
	bool Truncate();

	bool Seek(Long pos, SeekMode mode = SM_SET);
	Long Tell() const;

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

	inline Int GetByte()
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
};

}
