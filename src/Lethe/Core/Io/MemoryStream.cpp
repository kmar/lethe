#include "MemoryStream.h"
#include "../Memory/Memory.h"
#include "../String/String.h"

namespace lethe
{

// MemoryStream

// dynamic ctor
MemoryStream::MemoryStream()
	: refBuf(nullptr)
	, cpos(0)
	, refBufSize(0)
{
}

// const buffer ctor
MemoryStream::MemoryStream(const void *rbuff, Int rbSize)
	: refBuf(static_cast<const Byte *>(rbuff))
	, cpos(0)
	, refBufSize(rbSize)
{
}

MemoryStream::MemoryStream(const char *str)
	: refBuf(nullptr)
	, cpos(0)
	, refBufSize(0)
{
	if (LETHE_UNLIKELY(!str))
		return;

	const char *tmp = str;

	while (*tmp)
		tmp++;

	refBuf = reinterpret_cast< const Byte * >(str);
	refBufSize = (Int)(tmp - str);
}

UInt MemoryStream::GetFlags() const
{
	UInt res = SF_OPEN | SF_READABLE | SF_SEEKABLE;

	if (!refBuf)
		res |= SF_WRITABLE;

	return res;
}

// open (either dynamic or const buffer)
void MemoryStream::Open(const void *buf, Int sz)
{
	refBuf = static_cast<const Byte *>(buf);
	refBufSize = sz;
	data.Clear();
}

Long MemoryStream::GetSize()
{
	return refBuf ? refBufSize : data.GetSize();
}

bool MemoryStream::Seek(Long pos, SeekMode mode)
{
	switch(mode)
	{
	case SM_CUR:
		pos += cpos;
		break;

	case SM_END:
		pos += GetSize();
		break;

	default:
		;
	}

	cpos = (Int)pos;

	if (pos != cpos)
	{
		// overflow
		return 0;
	}

	// never allow negative seek
	cpos = Max((Int)0, cpos);
	return 1;
}

Long MemoryStream::Tell() const
{
	return cpos;
}

// truncate at current position
bool MemoryStream::Truncate()
{
	if (LETHE_UNLIKELY(refBuf))
		return false;

	data.Resize(cpos);
	return true;
}

const void *MemoryStream::GetData() const
{
	return refBuf ? refBuf : data.GetData();
}

Array<Byte> &MemoryStream::GetInternalBuffer()
{
	return data;
}

bool MemoryStream::Read(void *buf, Int size, Int &nread)
{
	LETHE_ASSERT(buf && size >= 0);
	Int avail = (Int)GetSize() - cpos;
	Int nr = Max((Int)0, Min(size, avail));
	nread = nr;

	if (nr)
		MemCpy(buf, (const Byte *)GetData() + cpos, (size_t)nr);

	cpos += nr;
	return 1;
}

bool MemoryStream::Write(const void *buf, Int size, Int &nwritten)
{
	LETHE_ASSERT(buf && size >= 0 && !refBuf);
	LETHE_ASSERT(cpos + size > cpos);
	// potential new size
	Int potNewSize = cpos + size;

	if (data.GetSize() < potNewSize)
	{
		data.EnsureCapacity(potNewSize);
		data.Resize(potNewSize);
	}

	if (size)
		MemCpy(data.GetData() + cpos, buf, (size_t)size);

	cpos += size;
	nwritten = size;
	return true;
}

// reserve (only valid in dynamic mode)
bool MemoryStream::Reserve(Int res)
{
	if (refBuf)
		return false;

	data.Reserve(res);
	return true;
}

bool MemoryStream::Resize(Int res)
{
	if (refBuf)
		return false;

	data.Resize(res);
	cpos = Max(data.GetSize(), cpos);
	return true;
}

bool MemoryStream::Clear()
{
	if (refBuf)
		return false;

	data.Clear();
	cpos = 0;
	return true;
}

bool MemoryStream::Reset()
{
	if (refBuf)
		return false;

	data.Reset();
	cpos = 0;
	return true;
}

String MemoryStream::ToString() const
{
	const char *ptr = static_cast<const char *>(GetData());
	Int sz = refBuf ? refBufSize : data.GetSize();

	if (sz <= 0)
		return String();

	return String(ptr, ptr + sz);
}

Stream *MemoryStream::Clone() const
{
	auto res = new MemoryStream;
	res->refBuf = refBuf;
	res->data = data;
	res->cpos = cpos;
	res->refBufSize = refBufSize;
	return res;
}

}
