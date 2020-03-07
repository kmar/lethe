#include "Stream.h"
#include "../Memory/Memory.h"

namespace lethe
{

// Stream

Stream::Stream()
	: context(nullptr)
	, column(0)
{
}

Stream::~Stream()
{
}

bool Stream::Close()
{
	return 1;
}

UInt Stream::GetFlags() const
{
	return 0;
}

bool Stream::Rewind()
{
	return Seek(0, SM_BEG);
}

bool Stream::SeekEnd()
{
	return Seek(0, SM_END);
}

// returns 0 on error
bool Stream::Seek(Long pos, SeekMode mode)
{
	(void)pos;
	(void)mode;
	return 0;
}

// returns -1 on error
Long Stream::Tell() const
{
	return -1;
}

// returns -1 on error
Long Stream::GetSize()
{
	Long old = Tell();

	if (old < 0 || !Seek(0, SM_END))
		return -1;

	Long res = Tell();
	Seek(old);
	return res;
}

// returns 0 on error
bool Stream::Flush()
{
	return 1;
}

// truncate at current position
// returns 0 on error
bool Stream::Truncate()
{
	return 0;
}

// is end of file? - returns 0 for non-seekable streams
bool Stream::IsEof()
{
	Long cpos = Tell();

	if (LETHE_UNLIKELY(cpos < 0))
	{
		// assume not seekable
		return 0;
	}

	return GetSize() == cpos;
}

// create in independent clone (useful especially for files)
// returns 0 if cannot be cloned
Stream *Stream::Clone() const
{
	return 0;
}

bool Stream::Read(void *buf, Int size, Int &nread)
{
	(void)buf;
	(void)size;
	(void)nread;
	return 0;
}

bool Stream::Write(const void *buf, Int size, Int &nwritten)
{
	(void)buf;
	(void)size;
	(void)nwritten;
	return 0;
}

// read whole stream
bool Stream::ReadAll(Array<Byte> &buf, Int extraReserve, Int limit)
{
	LETHE_RET_FALSE(extraReserve >= 0);
	buf.Clear();

	if (LETHE_UNLIKELY(!IsSeekable()))
	{
		Byte lbuf[8192];
		Int bsz = sizeof(lbuf);
		Long curSz = 0;

		for (;;)
		{
			Int nr;

			if (LETHE_UNLIKELY(!Read(lbuf, bsz, nr)))
				return 0;

			if (LETHE_UNLIKELY(!nr))
				break;

			curSz += nr;
			LETHE_RET_FALSE(curSz <= limit);
			buf.Append(lbuf, nr);
		}

		return true;
	}

	Long sz = GetSize();
	LETHE_RET_FALSE(sz + extraReserve <= limit);
	LETHE_ASSERT((Int)sz == sz);
	Rewind();
	buf.Reserve((Int)sz + extraReserve);
	buf.Resize((Int)sz);
	// empty is ok, avoid potential problems
	return !sz || Read(buf.GetData(), (Int)sz);
}

bool Stream::Append(Stream &s, Long length)
{
	Byte buf[8192];
	Int bsz = sizeof(buf);

	if (length >= 0)
	{
		if (LETHE_UNLIKELY(!length))
			return 1;

		for (;;)
		{
			Int sz = bsz;

			if (length < sz)
				sz = (Int)length;

			length -= sz;

			if (LETHE_UNLIKELY(!s.Read(buf, sz)))
				return 0;

			if (LETHE_UNLIKELY(!Write(buf, sz)))
				return 0;

			if (LETHE_UNLIKELY(length <= 0))
				return 1;
		}
	}

	for (;;)
	{
		Int nr;

		if (LETHE_UNLIKELY(!s.Read(buf, bsz, nr)))
			return 0;

		if (LETHE_UNLIKELY(!nr))
			break;

		if (LETHE_UNLIKELY(!Write(buf, nr)))
			return 0;
	}

	return 1;
}

// skip bytes (read)
bool Stream::SkipRead(Long bytes)
{
	Byte buf[8192];

	if (bytes < 0)
		return Seek(bytes, SM_CUR);

	while (bytes > 0)
	{
		Int tmp = (Int)Min(bytes, (Long)sizeof(buf));

		if (!Read(buf, tmp))
			return 0;

		bytes -= tmp;
	}

	return 1;
}

// skip bytes (write, writes zeros)
bool Stream::SkipWrite(Long bytes)
{
	Byte buf[8192];
	MemSet(buf, 0, sizeof(buf));

	if (bytes < 0)
		return Seek(bytes, SM_CUR);

	while (bytes > 0)
	{
		Int tmp = (Int)Min(bytes, (Long)sizeof(buf));

		if (!Write(buf, tmp))
			return 0;

		bytes -= tmp;
	}

	return 1;
}

Int Stream::GetByte()
{
	column++;

	if (LETHE_UNLIKELY(!ungetBuf.IsEmpty()))
	{
		Int res = ungetBuf.Back();
		ungetBuf.Pop();
		return res;
	}

	Byte b;

	if (LETHE_UNLIKELY(!Read(&b, 1)))
	{
		column--;
		return -1;
	}

	return b;
}

}
