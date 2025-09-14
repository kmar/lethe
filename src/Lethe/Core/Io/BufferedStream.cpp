#include "BufferedStream.h"
#include "../Memory/Memory.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(BufferedStream)

// BufferedStream

BufferedStream::BufferedStream()
	: cpos(-1)
	, flags(0)
	, stream(nullptr)
	, buffSize(8192)
	, rdBuffPtr(0)
	, rdBuffTop(0)
	, rdBuffSize(0)
	, wrBuffPtr(0)
	, wrBuffSize(0)
{
}

BufferedStream::BufferedStream(Stream &refs, Int nbuffSize)
	: cpos(-1)
	, flags(0)
	, stream(nullptr)
	, buffSize(nbuffSize)
	, rdBuffPtr(0)
	, rdBuffTop(0)
	, rdBuffSize(0)
	, wrBuffPtr(0)
	, wrBuffSize(0)
{
	SetStream(refs);
}

BufferedStream::~BufferedStream()
{
	Flush();
}

void BufferedStream::FlushRead()
{
	rdBuffPtr = rdBuffTop = 0;
}

bool BufferedStream::SetStream(Stream &refs)
{
	flags = 0;

	if (LETHE_UNLIKELY(stream && !Flush()))
		return false;

	FlushRead();

	stream = &refs;
	flags = refs.GetFlags();

	// note: we assume cpos is 0 for non-seekable streams instead of -1
	cpos = (flags & SF_SEEKABLE) ? stream->Tell() : 0;

	return SetBufferSize(buffSize);
}

bool BufferedStream::SetBufferSize(Int nbuffSize)
{
	LETHE_ASSERT(nbuffSize > 0);

	if (LETHE_UNLIKELY(!Flush() || nbuffSize < 0))
		return false;

	FlushRead();

	if (LETHE_UNLIKELY(!stream))
	{
		// nothing to do yet
		return true;
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

	return true;
}

UInt BufferedStream::GetFlags() const
{
	return flags | Stream::SF_BUFFERED;
}

bool BufferedStream::Read(void *buf, Int size, Int &nread)
{
	LETHE_ASSERT(stream && size >= 0 && (buf || size==0));

	if (wrBuffPtr > 0)
	{
		// we may need to write-flush here
		LETHE_ASSERT(rdBuffPtr == rdBuffTop);
		LETHE_RET_FALSE(Flush());
	}

	// take fast path first if possible
	if (LETHE_LIKELY(rdBuffPtr + size <= rdBuffTop))
	{
		// fast-copy!
		if (size)
			MemCpy(buf, rdBuffer.GetData() + rdBuffPtr, (size_t)size);

		rdBuffPtr += size;
		nread = size;
		cpos += nread;
		return true;
	}

	// slow path (loop)
	Int nr = 0;
	Byte *dst = Cast<Byte *>(buf);
	bool res = true;

	while (size > 0)
	{
		if (LETHE_UNLIKELY(rdBuffPtr >= rdBuffTop))
		{
			// read next chunk of data
			LETHE_ASSERT(stream);

			if (LETHE_UNLIKELY(!stream))
			{
				res = false;
				break;
			}

			rdBuffPtr = rdBuffTop = 0;

			if (LETHE_UNLIKELY(!stream->Read(rdBuffer.GetData(), rdBuffSize, rdBuffTop)))
			{
				res = false;
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

	if (rdBuffPtr < rdBuffTop)
	{
		// flush read buffer, must be seekable
		LETHE_RET_FALSE(stream->Seek(-(rdBuffTop - rdBuffPtr), SM_CUR));
		FlushRead();
	}

	// take fast path first if possible
	if (LETHE_LIKELY(wrBuffPtr + size <= wrBuffSize))
	{
		// fast-copy!
		if (size)
			MemCpy(wrBuffer.GetData() + wrBuffPtr, buf, (size_t)size);

		wrBuffPtr += size;
		nwritten = size;
		cpos += nwritten;
		return true;
	}

	// slow path (loop)
	Int nw = 0;
	const Byte *src = Cast<const Byte *>(buf);
	bool res = true;

	while (size > 0)
	{
		if (LETHE_UNLIKELY(wrBuffPtr >= wrBuffSize))
		{
			// flush write buffer
			// note: not calling Flush here because we want to keep the flag plus it may be slightly faster
			LETHE_ASSERT(stream);

			if (LETHE_UNLIKELY(!stream))
			{
				res = false;
				break;
			}

			if (LETHE_UNLIKELY(!stream->Write(wrBuffer.GetData(), wrBuffPtr)))
			{
				res = false;
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

bool BufferedStream::Rewind()
{
	LETHE_RET_FALSE(stream && Flush());

	FlushRead();

	auto res = stream->Rewind();

	if (res)
		cpos = 0;

	return res;
}

bool BufferedStream::SeekEnd()
{
	LETHE_RET_FALSE(stream && Flush());

	FlushRead();

	auto res = stream->SeekEnd();

	if (res)
		cpos = stream->Tell();

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
		return false;

	FlushRead();

	if (LETHE_UNLIKELY(!stream->Seek(cpos, SM_BEG) || !stream->Seek(pos, mode)))
		return false;

	cpos = stream->Tell();
	return true;
}

Long BufferedStream::Tell() const
{
	return (flags & SF_SEEKABLE) ? cpos : -1;
}

Long BufferedStream::GetSize()
{
	return stream ? stream->GetSize() : -1;
}

bool BufferedStream::Close()
{
	if (!Flush())
		return false;

	FlushRead();

	stream = nullptr;
	return true;
}

bool BufferedStream::Flush()
{
	// flushing write buffer only
	if (wrBuffPtr > 0)
	{
		LETHE_ASSERT(stream);

		if (LETHE_UNLIKELY(!stream->Write(wrBuffer.GetData(), wrBuffPtr)))
			return false;
	}

	wrBuffPtr = 0;
	return true;
}

bool BufferedStream::Truncate()
{
	LETHE_ASSERT(stream);

	if (LETHE_UNLIKELY(!Flush()))
		return false;

	if (flags & SF_SEEKABLE)
	{
		// update physical pointer
		if (LETHE_UNLIKELY(!stream->Seek(cpos)))
			return false;

		// force-flush read buffer as well
		FlushRead();
	}

	return stream->Truncate();
}

}
