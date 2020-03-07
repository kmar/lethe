#include "BufferedStream.h"
#include "../Memory/Memory.h"

namespace lethe
{

// dirty flags
static const UInt DIRTY_READ = 65536;
static const UInt DIRTY_WRITE = 131072;

// BufferedStream

BufferedStream::BufferedStream() : cpos(-1), flags(0), stream(0), buffSize(8192),
	rdBuffPtr(0), rdBuffTop(0), rdBuffSize(0), wrBuffPtr(0), wrBuffSize(0)
{
}

BufferedStream::BufferedStream(Stream &refs, Int nbuffSize) : cpos(-1), flags(0), stream(0), buffSize(nbuffSize),
	rdBuffPtr(0), rdBuffTop(0), rdBuffSize(0), wrBuffPtr(0), wrBuffSize(0)
{
	SetStream(refs);
}

BufferedStream::~BufferedStream()
{
	Flush();
}

bool BufferedStream::SetStream(Stream &refs)
{
	flags = 0;

	if (LETHE_UNLIKELY(stream && !Flush()))
		return 0;

	stream = &refs;
	flags = refs.GetFlags();
	cpos = -1;

	if (flags & SF_SEEKABLE)
		cpos = stream->Tell();

	return SetBufferSize(buffSize);
}

bool BufferedStream::SetBufferSize(Int nbuffSize)
{
	LETHE_ASSERT(nbuffSize > 0);

	if (LETHE_UNLIKELY(!Flush() || nbuffSize < 0))
		return 0;

	if (LETHE_UNLIKELY(!stream))
	{
		// nothing to do yet
		return 1;
	}

	if (flags & SF_READABLE)
	{
		rdBuffer.Resize(buffSize);
		rdBuffSize = buffSize;
		rdBuffPtr = rdBuffTop = 0;
	}
	else
		rdBuffer.Clear();

	if (flags & SF_WRITABLE)
	{
		wrBuffer.Resize(buffSize);
		wrBuffSize = buffSize;
		wrBuffPtr = 0;
	}
	else
		wrBuffer.Clear();

	return 1;
}

UInt BufferedStream::GetFlags() const
{
	return flags & ~(DIRTY_READ|DIRTY_WRITE);
}

bool BufferedStream::Read(void *buf, Int size, Int &nread)
{
	LETHE_ASSERT(stream && buf && size >= 0);

	// flush if necessary
	if (LETHE_UNLIKELY((flags & DIRTY_WRITE) && !Flush()))
		return 0;

	// let Write know
	flags |= DIRTY_READ;

	// take fast path first if possible
	if (LETHE_LIKELY(rdBuffPtr + size <= rdBuffTop))
	{
		// fast-copy!
		MemCpy(buf, rdBuffer.GetData() + rdBuffPtr, (size_t)size);
		rdBuffPtr += size;
		nread = size;
		cpos += nread;
		return 1;
	}

	// slow path (loop)
	Int nr = 0;
	Byte *dst = Cast<Byte *>(buf);
	bool res = 1;

	while (size > 0)
	{
		if (LETHE_UNLIKELY(rdBuffPtr >= rdBuffTop))
		{
			// read next chunk of data
			LETHE_ASSERT(stream);

			if (LETHE_UNLIKELY(!stream))
			{
				res = 0;
				break;
			}

			rdBuffPtr = rdBuffTop = 0;

			if (LETHE_UNLIKELY(!stream->Read(rdBuffer.GetData(), rdBuffSize, rdBuffTop)))
			{
				res = 0;
				break;
			}
		}

		Int avail = rdBuffTop - rdBuffPtr;
		avail = Min(avail, size);

		if (LETHE_UNLIKELY(!avail))
			break;

		MemCpy(dst, rdBuffer.GetData() + rdBuffPtr, (size_t)avail);
		nr += avail;
		dst += avail;
		size -= avail;
		rdBuffPtr += avail;
	}

	nread = nr;
	cpos += nread;
	return res;
}

bool BufferedStream::Write(const void *buf, Int size, Int &nwritten)
{
	LETHE_ASSERT(stream && buf && size >= 0);

	// flush if necessary
	if (LETHE_UNLIKELY((flags & DIRTY_READ) && !Flush()))
		return 0;

	// let Read know
	flags |= DIRTY_WRITE;

	// take fast path first if possible
	if (LETHE_LIKELY(wrBuffPtr + size <= wrBuffSize))
	{
		// fast-copy!
		MemCpy(wrBuffer.GetData() + wrBuffPtr, buf, (size_t)size);
		wrBuffPtr += size;
		nwritten = size;
		cpos += nwritten;
		return 1;
	}

	// slow path (loop)
	Int nw = 0;
	const Byte *src = Cast<const Byte *>(buf);
	bool res = 1;

	while (size > 0)
	{
		if (LETHE_UNLIKELY(wrBuffPtr >= wrBuffSize))
		{
			// flush write buffer
			// note: not calling Flush here because we want to keep the flag plus it may be slightly faster
			LETHE_ASSERT(stream);

			if (LETHE_UNLIKELY(!stream))
			{
				res = 0;
				break;
			}

			if (LETHE_UNLIKELY(!stream->Write(wrBuffer.GetData(), wrBuffPtr)))
			{
				res = 0;
				break;
			}

			wrBuffPtr = 0;
		}

		Int avail = wrBuffSize - wrBuffPtr;
		avail = Min(avail, size);

		if (LETHE_UNLIKELY(!avail))
			break;

		MemCpy(wrBuffer.GetData() + wrBuffPtr, src, (size_t)avail);
		nw += avail;
		src += avail;
		size -= avail;
		wrBuffPtr += avail;
	}

	nwritten = nw;
	cpos += nwritten;
	return res;
}

bool BufferedStream::Seek(Long pos, SeekMode mode)
{
	LETHE_RET_FALSE(stream);

	if (!wrBuffPtr)
	{
		auto newPos = cpos;

		// try to move forward in a fast way in read-only mode
		switch(mode)
		{
		case SM_CUR:
			newPos += pos;
			break;
		case SM_END:
			{
				auto sz = stream->GetSize();
				LETHE_RET_FALSE(sz >= 0);
				newPos += sz;
			}
			break;
		default:
			newPos = pos;
		}

		auto delta = newPos - cpos;

		if (delta > 0 && delta <= rdBuffTop - rdBuffPtr)
		{
			rdBuffPtr += (Int)delta;
			cpos += delta;
			return true;
		}
	}

	if (LETHE_UNLIKELY(!Flush()))
		return 0;

	if (LETHE_UNLIKELY(!stream->Seek(cpos, SM_BEG) || !stream->Seek(pos, mode)))
		return 0;

	cpos = stream->Tell();
	return 1;
}

Long BufferedStream::Tell() const
{
	return (flags & SF_SEEKABLE) ? cpos : -1;
}

bool BufferedStream::Close()
{
	if (!Flush())
		return 0;

	stream = 0;
	return 1;
}

bool BufferedStream::Flush()
{
	// flushing read buffer is simple
	rdBuffPtr = rdBuffTop = 0;

	// flushing write buffer is slightly more complicated
	if (wrBuffPtr > 0)
	{
		LETHE_ASSERT(stream);

		if (LETHE_UNLIKELY(!stream->Write(wrBuffer.GetData(), wrBuffPtr)))
			return 0;
	}

	wrBuffPtr = 0;
	flags &= ~(DIRTY_READ | DIRTY_WRITE);
	return 1;
}

bool BufferedStream::Truncate()
{
	LETHE_ASSERT(stream);

	if (LETHE_UNLIKELY(!Flush()))
		return 0;

	if (flags & SF_SEEKABLE)
	{
		// update physical pointer
		if (LETHE_UNLIKELY(!stream->Seek(cpos)))
			return 0;
	}

	return stream->Truncate();
}

}
