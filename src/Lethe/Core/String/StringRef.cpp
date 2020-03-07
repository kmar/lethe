#include "StringRef.h"

namespace lethe
{

// StringRef

String::CharIterator StringRef::Begin() const
{
	String::CharIterator res;
	res.ptr = reinterpret_cast<const Byte *>(ref);
	res.top = res.ptr + GetLength();

	if (res.ptr >= res.top)
		return End();

	res.ch = -1;
	res.charPos = res.bytePos = res.byteSize = 0;
	auto optr = res.ptr;
	res.ch = CharConv::DecodeUTF8(res.ptr, res.top);
	res.byteSize = int(res.ptr - optr);
	res.ptr = optr;
	return res;
}

const wchar_t *StringRef::ToWide(Array<wchar_t> &wbuf) const
{
	if (IsEmpty())
	{
		wbuf.Resize(1);
		wbuf[0] = 0;
		return L"";
	}

	int req = CharConv::AnsiToWide(Ansi(), GetLength(), nullptr, 0);
	wbuf.Resize(req+1);
	CharConv::AnsiToWide(Ansi(), GetLength(), wbuf.GetData(), wbuf.GetSize());
	wbuf[req] = 0;

	return wbuf.GetData();
}

bool StringRef::StartsWith(const StringRef &o) const
{
	LETHE_RET_FALSE(o.GetLength() <= GetLength());
	return IsEmpty() ? true : MemCmp(GetData(), o.GetData(), o.GetLength()) == 0;
}

bool StringRef::EndsWith(const StringRef &o) const
{
	LETHE_RET_FALSE(o.GetLength() <= GetLength());
	return IsEmpty() ? true : MemCmp(GetData() + GetLength() - o.GetLength(), o.GetData(), o.GetLength()) == 0;
}

}
