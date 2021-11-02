#include "../Collect/Array.h"
#include "../Sys/Platform.h"
#include "../Sys/Path.h"
#include "../Sys/Endian.h"
#include "../Memory/Memory.h"
#include "../Io/Stream.h"
#include "StringRef.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// this is my older code. may need review

#if LETHE_OS_WINDOWS
#	include <windows.h>
#endif

namespace lethe
{

// static helpers

namespace
{

// maximum format string buffer size in chars
const int maxFmtSize = 64000;

int wlen(const wchar_t *str)
{
	LETHE_ASSERT(str);

	if (!str)
		return 0;

	const wchar_t *start = str;

	while (*str)
		str++;

	return static_cast<int>(str - start);
}

bool isWhiteSpc(wchar_t ch)
{
	// FIXME: better
	switch(ch)
	{
	case '\t':
	case '\v':
	case '\b':
	case '\n':
	case '\r':
	case 32:
		return 1;
	}

	return 0;
}

}

// String::CharIterator

String::CharIterator &String::CharIterator::operator ++()
{
	ptr += byteSize;
	bytePos += byteSize;
	charPos++;

	if (ptr >= top)
	{
		*this = endIterator;
		return *this;
	}

	LETHE_ASSERT(ptr);
	auto optr = ptr;
	ch = CharConv::DecodeUTF8(ptr, top);

	if (ch < 0)
		ch = '?';

	byteSize = int(ptr - optr);
	byteSize = Max<int>(byteSize, 1);
	ptr = optr;

	return *this;
}

// StringData

StringData *StringData::Alloc(int n)
{
	LETHE_ASSERT(n >= 0);
	StringData *res = reinterpret_cast<StringData *>(
		StringAllocator.CallAlloc(sizeof(StringData) + n*sizeof(char), 
			Max<size_t>(8, alignof(StringData)))
	);
	LETHE_ASSERT(!((uintptr_t)res & 7));
	res->length = 0;
	res->charLength = 0;
	res->capacity = n;
	res->refCount = 1;			// pre-incremented -- no need to call addRef after alloc
	res->hash = 0;
	res->data[0] = 0;
	return res;
}

StringData *StringData::Clone()
{
	LETHE_ASSERT(refCount);
	StringData *res = Alloc(length);
	LETHE_ASSERT(res);

	for (int i=0; i<=length; i++)
		res->data[i] = data[i];

	res->length = length;
	// note: we don't copy derived information here (like hash and widelength)
	return res;
}

// String

String::CharIterator String::endIterator =
{
	nullptr,
	nullptr,
	-1,
	0,
	0,
	0
};

String::CharIterator String::Begin() const
{
	CharIterator res;
	res.ptr = reinterpret_cast<const Byte *>(Ansi());
	res.top = res.ptr + GetLength();

	if (res.ptr >= res.top)
		return endIterator;

	res.ch = -1;
	res.charPos = res.bytePos = res.byteSize = 0;
	auto optr = res.ptr;
	res.ch = CharConv::DecodeUTF8(res.ptr, res.top);
	res.byteSize = int(res.ptr - optr);
	res.ptr = optr;
	return res;
}

String::String(const StringRef &sr)
	: data(nullptr)
{
	Init(sr.GetData(), sr.GetData() + sr.GetLength());
}

String::String(const char *str) : data(0)
{
	*this = str;
}

String::String(const wchar_t *str) : data(0)
{
	*this = str;
}

String::String(const String &str) : data(0)
{
	*this = str;
}

// allow construction from path!
String::String(const Path &pth) : data(0)
{
	*this = pth.Get();
}

String::String(const char *str, int len) : data(0)
{
	Init(str, len < 0 ? static_cast<const char *>(0) : str + len);
}

String::String(const wchar_t *str, int len) : data(0)
{
	Init(str, len < 0 ? static_cast<const wchar_t *>(0) : str + len);
}

String::String(const char *str, const char *strEnd) : data(0)
{
	Init(str, strEnd);
}

String::String(const wchar_t *str, const wchar_t *strEnd) : data(0)
{
	Init(str, strEnd);
}

String String::operator +(int w) const
{
	String res(*this);
	return res += w;
}

String String::operator +(const char *str) const
{
	String res(*this);
	return res += str;
}

String String::operator +(const String &str) const
{
	String res(*this);
	return res += str;
}

// friends

String operator +(wchar_t w, const String &str1)
{
	wchar_t wa[2] = {w, 0};
	return String(wa) + str1;
}

String operator +(const char *str0, const String &str1)
{
	return String(str0) + str1;
}

String operator +(const wchar_t *str0, const String &str1)
{
	return String(str0) + str1;
}

void String::Init(const char *str, const char *strEnd, CharConv::Encoding encoding)
{
	Clear();

	if (str && !strEnd)
		strEnd = str + strlen(str);

	LETHE_ASSERT(str && strEnd);

	if (!str || strEnd == str)
		return;

	if (encoding == CharConv::encDefault)
		encoding = CharConv::defaultEncoding;

	// convert to utf8 representation
	int req = int(strEnd - str);
	StringData *ndata;

	if (encoding != CharConv::encUtf8)
	{
		int slen = req;
		req = CharConv::AnsiToUTF8(str, req, nullptr, 0, encoding);
		LETHE_ASSERT(req != (int)-1);
		ndata = StringData::Alloc(req);
		CharConv::AnsiToUTF8(str, slen, ndata->data, req, encoding);
	}
	else
	{
		ndata = StringData::Alloc(req);
		MemCpy(ndata->data, str, req);
	}

	LETHE_ASSERT(ndata);
	ndata->data[ndata->length = req] = 0;
	data = ndata;
}

void String::Init(const wchar_t *str, const wchar_t *strEnd)
{
	Clear();

	if (str && !strEnd)
		strEnd = str + wlen(str);

	LETHE_ASSERT(str && strEnd);

	if (!str || strEnd == str)
		return;

	int wlen = int(strEnd - str);
	int len = CharConv::WideToAnsi(str, wlen, nullptr, 0, CharConv::encUtf8);
	LETHE_ASSERT(len >= 0);

	if (!len)
		return;

	StringData *ndata = StringData::Alloc(len);
	LETHE_ASSERT(ndata);
	ndata->length = CharConv::WideToAnsi(str, wlen, ndata->data, len, CharConv::encUtf8);
	ndata->data[ndata->length] = 0;
	data = ndata;
}

String::~String()
{
	Reset();
#if LETHE_DEBUG
	memset(this, 0xfe, sizeof(*this));
#endif
}

String &String::ContChange()
{
	if (data)
	{
		data->hash = 0;
		data->charLength = 0;
	}

	return *this;
}

String &String::Clear()
{
#ifndef __clang_analyzer__
	if (data)
	{
		StringData *dt = data;
		data = nullptr;
		dt->Release();
	}
#endif

	return *this;
}

String &String::Reset()
{
	// at the moment, this is the same as Clear
	return Clear();
}

String &String::operator =(int w)
{
	// FIXME: better
	wchar_t wa[] = {(wchar_t)w, 0};
	return *this = wa;
}

String &String::operator =(const char *c)
{
	if (!c || !*c)
	{
		Clear();
		return *this;
	}

	const char *ce = c;

	while (*ce)
		ce++;

	Init(c, ce);
	return *this;
}

String &String::operator =(const wchar_t *c)
{
	if (!c || !*c)
	{
		Clear();
		return *this;
	}

	const wchar_t *ce = c;

	while (*ce)
		ce++;

	Init(c, ce);
	return *this;
}

String &String::operator =(const String &str)
{
	if (str.data == data)
		return *this;

	Clear();

	if (str.data)
	{
#ifndef __clang_analyzer__
		str.data->AddRef();
#endif
		data = str.data;
	}

	return *this;
}

String &String::operator =(const StringRef &str)
{
	Init(str.GetData(), str.GetData() + str.GetLength());
	return *this;
}

String &String::AppendData(int l, int sl, const char *str)
{
	LETHE_ASSERT(str);

	if (LETHE_UNLIKELY(!str || !sl))
		return *this;

	CloneData();

	LETHE_ASSERT(data);

	if (l + sl > data->capacity)
	{
		// must resize
		int cap = Max<int>(data->capacity*3 / 2, l + sl);
		StringData *nd = StringData::Alloc(cap);
		LETHE_ASSERT(nd);

		// full add
		MemCpy(nd->data, data->data, l * sizeof(char));
		MemCpy(nd->data + l, str, sl * sizeof(char));

		StringData *dt = data;
		data = nd;
		dt->Release();
	}
	else
	{
		// fast add
		MemCpy(data->data + l, str, sl * sizeof(char));
	}

	data->data[data->length = l+sl] = 0;

	return ContChange();
}

String &String::operator +=(int w)
{
	int l = GetLength();

	if (!l)
		return *this = w;

	Byte buf[7];
	int sl = CharConv::EncodeUTF8(w, buf, buf+6);
	buf[sl] = 0;

	return AppendData(l, sl, reinterpret_cast<const char *>(buf));
}

String &String::Append(const char *c, const char *cend)
{
	LETHE_ASSERT(c && cend);
	return AppendData(GetLength(), (int)(cend - c), c);
}

String &String::operator +=(const char *c)
{
	if (!c || !*c)
		return *this;

	int l = GetLength();

	if (!l)
		return *this = c;

	int sl = (int)StrLen(c);

	if (!sl)
		return *this;

	return AppendData(l, sl, c);
}

String &String::operator +=(const String &str)
{
	int s = GetLength();

	if (!s)
		return *this = str;

	int sl = str.GetLength();

	if (!sl)
		return *this;

	return AppendData(s, sl, str.Ansi());
}

int String::Comp(const char *str, const char *strEnd) const
{
	LETHE_ASSERT(str);

	if (!str)
		return -2;

	if (!strEnd)
		strEnd = str + strlen(str);

	const char *c = Ansi();
	const char *cEnd = c + GetLength();

	while (c != cEnd && str != strEnd)
	{
		if (*c != *str)
			return *reinterpret_cast<const Byte *>(c) < *reinterpret_cast<const Byte *>(str) ? -1 : 1;

		c++;
		str++;
	}

	// now, strings match only if both are at the end
	if (c == cEnd)
		return str == strEnd ? 0 : -1;

	LETHE_ASSERT(str == strEnd);
	return 1;
}

int String::Comp(const String &str) const
{
	if (data == str.data)
		return 0;

	const char *c = str.Ansi();
	return Comp(c, c + str.GetLength());
}

int String::CompNC(const char *str, const char *strEnd) const
{
	// FIXME: use locale?
	LETHE_ASSERT(str);

	if (!str)
		return -2;

	const char *c = Ansi();
	const char *cEnd = c + GetLength();

	if (!strEnd)
		strEnd = str + StrLen(str);

	while (c != cEnd && str != strEnd)
	{
		int wc0, wc1;
		wc0 = *reinterpret_cast<const Byte *>(c);
		wc1 = *reinterpret_cast<const Byte *>(str);

		// fast tolower
		if (wc0 >= 'A' && wc0 <= 'Z')
			wc0 |= 32;

		if (wc1 >= 'A' && wc1 <= 'Z')
			wc1 |= 32;

		if (wc0 != wc1)
			return wc0 < wc1 ? -1 : 1;

		c++;
		str++;
	}

	if (c == cEnd)
		return str == strEnd ? 0 : -1;

	return str == strEnd ? 1 : 0;
}

int String::CompNC(const String &str) const
{
	if (data == str.data)
		return 0;

	const char *c = str.Ansi();
	return CompNC(c, c + str.GetLength());
}

int String::Find(const char *str, const char *strEnd, int pos) const
{
	int l = GetLength();

	if (!l)
		return -1;

	LETHE_ASSERT(str);

	if (!str)
		return -1;

	if (!strEnd)
		strEnd = str + StrLen(str);

	if (str == strEnd || (int)pos >= l)
		return -1;

	const char *c = Ansi();
	const char *ce = c + GetLength();
	c += pos;

	while (c < ce)
	{
		const char *tmp = str;
		const char *src = c;

		while (tmp < strEnd && src < ce)
		{
			if (*src != *tmp)
				break;

			tmp++;
			src++;
		}

		if (tmp == strEnd)
			return pos;			// match

		c++;
		pos++;
	}

	return -1;
}

int String::FindSet(const char *str, const char *strEnd, int pos) const
{
	int l = GetLength();

	if (!l)
		return -1;

	LETHE_ASSERT(str);

	if (!str)
		return -1;

	if (!strEnd)
		strEnd = str + StrLen(str);

	if (str == strEnd || (int)pos >= l)
		return -1;

	const char *c = Ansi();
	const char *ce = c + GetLength();
	c += pos;

	while (c < ce)
	{
		const char *tmp = str;

		while (tmp < strEnd)
		{
			if (*c == *tmp)
				return pos;

			tmp++;
		}

		c++;
		pos++;
	}

	return -1;
}

String String::Tokenize(const char *str, const char *strEnd, int &pos) const
{
	// FIXME: this could be better
	String res;
	LETHE_ASSERT(str);

	if (pos < 0 || !str)
		return res;

	if (!strEnd)
		strEnd = str + StrLen(str);

	const char *c = Ansi();
	const char *ce = c + GetLength();
	c += (int)pos;

	if (c < Ansi())
		return res;

	// if tokStart is initially null, skips empty tokens (FIXME: change later to accept keepEmpty flag as well?)
	const char *tokStart = c;

	while (c < ce)
	{
		const char *tmp = str;

		while (tmp < strEnd)
		{
			if (*c == *tmp)
				break;

			tmp++;
		}

		if (tmp >= strEnd)
		{
			// not a separator
			if (!tokStart)
				tokStart = c;
		}
		else if (tokStart)
		{
			res = String(tokStart, c);
			pos = (int)(c - Ansi() + 1);
			// keep this if tokStart is initially null (=we want to skip empty strings)
			/*if ((int)pos >= GetLength()) {
				pos = -1;
			}*/
			break;
		}

		c++;
	}

	if (tokStart && c >= ce)
	{
		res = String(tokStart, ce);
		pos = -1;
	}

	return res;
}

String &String::Insert(const char *str, const char *strEnd, int pos)
{
	LETHE_ASSERT(str);

	if (!str)
		return *this;

	if (!strEnd)
		strEnd = str + StrLen(str);

	if (str == strEnd)
		return *this;

	int l = GetLength();
	int epos = (int)pos;

	if (epos >= l)
	{
		if  (!l)
			Init(str, strEnd);
		else
			AppendData(l, static_cast<int>(strEnd - str), str);

		return *this;
	}

	CloneData();

	LETHE_ASSERT(data);

	int sl = static_cast<int>(strEnd - str);

	if (l + sl > data->capacity)
	{
		int cap = Max<int>(data->capacity*3/2, l + sl);
		StringData *nd = StringData::Alloc(cap);
		LETHE_ASSERT(nd);

		MemCpy(nd->data, data->data, epos * sizeof(char));
		MemCpy(nd->data + epos, str, sl * sizeof(char));
		MemCpy(nd->data + epos + sl, data->data + epos, (data->length - epos) * sizeof(char));
		StringData *dt = data;
		data = nd;
		dt->Release();
	}
	else
	{
		// first make space
		for (int i=l - epos; i>0; i--)
			data->data[epos + sl + i - 1] = data->data[epos + i - 1];

		// then copy
		MemCpy(data->data + epos, str, sl * sizeof(char));
	}

	data->data[data->length = l+sl] = 0;

	return ContChange();
}

int String::Replace(
	const char *ostr, const char *ostrend,
	const char *nstr, const char *nstrend
)
{
	LETHE_ASSERT(ostr && nstr);

	if (!ostr || !nstr)
		return 0;

	if (!ostrend)
		ostrend = ostr + StrLen(ostr);

	if (!nstrend)
		nstrend = nstr + StrLen(nstr);

	if (ostrend == ostr)
		return 0;

	String res(*this);
	int pos = 0;
	int reps = 0;
	int osz = (int)(ostrend - ostr);

	while ((pos = res.Find(ostr, ostrend, pos)) >= 0)
	{
		res.Erase(pos, osz);
		res.Insert(nstr, nstrend, pos);
		pos += (int)(nstrend - nstr);
		reps++;
	}

	*this = res;
	return reps;
}

bool String::operator <(const char *str) const
{
	return Comp(str) < 0;
}

bool String::operator <(const String &str) const
{
	return Comp(str) < 0;
}

bool String::operator <=(const char *str) const
{
	return Comp(str) <= 0;
}

bool String::operator <=(const String &str) const
{
	return Comp(str) <= 0;
}

bool String::operator >(const char *str) const
{
	return Comp(str) > 0;
}

bool String::operator >(const String &str) const
{
	return Comp(str) > 0;
}

bool String::operator >=(const char *str) const
{
	return Comp(str) >= 0;
}

bool String::operator >=(const String &str) const
{
	return Comp(str) >= 0;
}

bool String::operator ==(const char *str) const
{
	return Comp(str) == 0;
}

bool String::operator ==(const StringRef &sr) const
{
	const auto len = GetLength();

	if (len != sr.GetLength())
		return false;

	return len == 0 || MemCmp(data->data, sr.GetData(), len) == 0;
}

bool String::operator ==(const String &str) const
{
	const auto len = GetLength();

	if (len != str.GetLength())
		return false;

	return len == 0 || MemCmp(data->data, str.data->data, len) == 0;
}

bool String::operator !=(const char *str) const
{
	return Comp(str) != 0;
}

bool String::operator !=(const String &str) const
{
	return !(*this == str);
}

bool String::operator !=(const StringRef &sr) const
{
	return !(*this == sr);
}

// conversions

const char *String::Ansi() const
{
	return !data ? "" : data->data;
}

// index operator (read only)
char String::operator[](int index) const
{
	LETHE_ASSERT(index >= 0 && index < GetLength());
	return data->data[index];
}

bool String::IsUnique() const
{
	return !data || Atomic::Load(data->refCount) == 1;
}

String &String::CloneData()
{
	// note: this is not thread-safe!
	if (IsUnique())
		return *this;

	StringData *ndata = data->Clone();
	StringData *odata = data;
	data = ndata;
	odata->Release();
	return *this;
}

String &String::Shrink()
{
	if (data && data->capacity != data->length)
	{
		StringData *ndata = data->Clone();
		StringData *odata = data;
		data = ndata;
		odata->Release();
	}

	return *this;
}

// string length in bytes
int String::GetLength() const
{
#if defined(__clang_analyzer__)
	return 0;
#else
	return !data ? 0 : data->length;
#endif
}

bool String::IsEmpty() const
{
	return !GetLength();
}

String &String::Format(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char buf[maxFmtSize];
#ifdef LETHE_COMPILER_MSC
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
#else
	vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
	va_end(ap);
	buf[maxFmtSize - 1] = 0;
	return *this = buf;
}

String String::Printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char buf[maxFmtSize];
#ifdef LETHE_COMPILER_MSC
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
#else
	vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
	va_end(ap);
	buf[maxFmtSize - 1] = 0;
	return buf;
}

int String::Find(wchar_t ch, int pos) const
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	buf[sl] = 0;
	return Find(buf, buf+sl, pos);
}

int String::Find(const char *str, int pos) const
{
	// FIXME: better?
	return Find(String(str), pos);
}

int String::Find(const String &str, int pos) const
{
	const char *c = str.Ansi();
	return Find(c, c + str.GetLength(), pos);
}

int String::FindOneOf(wchar_t ch, int pos) const
{
	return Find(ch, pos);
}

int String::FindOneOf(const char *str, int pos) const
{
	return FindSet(str, 0, pos);
}

int String::FindOneOf(const String &str, int pos) const
{
	const char *c = str.Ansi();
	return FindSet(c, c + str.GetLength(), pos);
}

int String::ReverseFind(wchar_t ch) const
{
	LETHE_ASSERT(ch >= 0 && ch <= 127);

	const char *c = Ansi();
	const char *ce = c + GetLength();

	while (--ce >= c)
	{
		if (*ce == ch)
			return (int)(ce - c);
	}

	return -1;
}

int String::ReverseFind(const String &str, int pos) const
{
	int best = -1;
	int found = -1;

	for (;;)
	{
		found = Find(str, found + 1);

		if (found < 0 || (found + str.GetLength() >= pos))
			break;

		best = found;
	}

	return best;
}

String &String::Reverse()
{
	if (IsEmpty())
		return *this;

	CloneData();

	int req = 0;

	for (auto ci : *this)
		req += ci.byteSize;

	StringData *tmp = StringData::Alloc(req);
	tmp->length = req;
	char *c = tmp->data + req;

	for (auto ci : *this)
	{
		c -= ci.byteSize;
		CharConv::EncodeUTF8(ci.ch, c, c+ci.byteSize);
	}

	auto odata = data;
	data = tmp;
	odata->Release();

	return ContChange();
}

String &String::ToUpper()
{
	if (IsEmpty())
		return *this;

	CloneData();
	LETHE_ASSERT(data);

	for (int i=0; i<data->length; i++)
	{
		char &c = data->data[i];

		if (c >= 'a' && c <= 'z')
			c ^= 32;
	}

	return ContChange();
}

String &String::ToLower()
{
	if (IsEmpty())
		return *this;

	CloneData();
	LETHE_ASSERT(data);

	for (int i=0; i<data->length; i++)
	{
		char &c = data->data[i];

		if (c >= 'A' && c <= 'Z')
			c |= 32;
	}

	return ContChange();
}

String &String::ToCapital()
{
	if (IsEmpty())
		return *this;

	CloneData();
	LETHE_ASSERT(data);
	char &c = data->data[0];

	if (c >= 'a' && c <= 'z')
		c ^= 32;

	return ContChange();
}

// returns new length
int String::Erase(int pos, int count)
{
	int l = GetLength();

	pos = Max<Int>(pos, 0);

	if (pos >= l)
		return l;

	CloneData();
	// erase...
	int index = pos;
	int ecount = count < 0 ? l : count;

	if (index + ecount > l)
		ecount = l-index;

	int cpy = l - (index + ecount);

	for (int i=0; i<cpy; i++)
		data->data[index + i] = data->data[index + ecount + i];

	data->data[data->length -= ecount] = 0;

	ContChange();

	return GetLength();
}

String String::Left(int count) const
{
	int l = GetLength();

	if (!l || count <= 0)
		return String();

	int ecount = count;

	if (ecount > l)
		ecount = l;

	return String(Ansi(), ecount);
}

String String::Right(int count) const
{
	int l = GetLength();

	if (!l || count <= 0)
		return String();

	int ecount = count;

	if (ecount > l)
		ecount = l;

	return String(Ansi() + l - ecount, ecount);
}

// if count is -1, returns all chars to the right of pos
String String::Mid(int pos, int count) const
{
	int l = GetLength();
	int epos = pos;

	if (!l || epos >= l)
		return String();

	int ecount = count < 0 ? l : count;

	if (epos + ecount > l)
		ecount = l - epos;

	return String(Ansi() + epos, ecount);
}

// remove char from string
// returns number of characters removed
int String::Remove(wchar_t ch)
{
	int l = GetLength();

	if (!l)
		return 0;

	LETHE_ASSERT(ch >= 0 && ch <= 127);

	CloneData();

	LETHE_ASSERT(data);
	char *c = data->data;

	int res = 0;

	for (int i=0; i<l; i++)
	{
		if (c[i] == ch)
		{
			for (int j=i; j+1<l; j++)
				c[j] = c[j+1];

			l--;
			i--;
			res++;
		}
	}

	c[data->length = l] = 0;

	ContChange();

	return res;
}

int String::Insert(int pos, wchar_t ch)
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	buf[sl] = 0;
	Insert(buf, buf+sl, pos);
	return GetLength();
}

int String::Insert(int pos, const char *str)
{
	LETHE_ASSERT(str);
	Insert(str, nullptr, pos);
	return GetLength();
}

int String::Insert(int pos, const String &str)
{
	const char *c = str.Ansi();
	Insert(c, c + str.GetLength(), pos);
	return GetLength();
}

int String::Replace(wchar_t oldc, wchar_t newc)
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(oldc, buf, buf+6);
	buf[sl] = 0;
	char buf2[7];
	int sl2 = CharConv::EncodeUTF8(newc, buf2, buf2+6);
	buf2[sl2] = 0;
	return Replace(buf, buf2);
}

// performance warning: string versions are slow
int String::Replace(const char *oldstr, const char *newstr)
{
	return Replace(oldstr, nullptr, newstr, nullptr);
}

int String::Replace(const String &oldstr, const String &newstr)
{
	const char *oc = oldstr.Ansi();
	const char *nc = newstr.Ansi();
	return Replace(oc, oc + oldstr.GetLength(), nc, nc + newstr.GetLength());
}

String &String::TrimLeft(void)
{
	if (IsEmpty())
		return *this;

	CloneData();

	LETHE_ASSERT(data);
	char *c = data->data;
	const char *ce = c + data->length;

	while (c < ce && isWhiteSpc(*c))
		c++;

	IntPtr delta = (IntPtr)(c - data->data);

	while (c < ce)
	{
		c[-delta] = *c;
		c++;
	}

	data->data[data->length -= (int)delta] = 0;

	return ContChange();
}

String &String::TrimRight(void)
{
	if (IsEmpty())
		return *this;

	CloneData();

	LETHE_ASSERT(data);
	const char *ce = data->data + data->length;
	char *c = const_cast<char *>(ce)-1;

	while (c >= data->data && isWhiteSpc(*c))
		c--;

	c++;
	data->data[data->length = static_cast<int>(c - data->data)] = 0;

	return ContChange();
}

String &String::Trim(void)
{
	TrimRight();
	return TrimLeft();
}

bool String::StartsWith(const wchar_t ch) const
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	return StartsWith(buf, sl);
}

bool String::StartsWith(const char *str) const
{
	return StartsWith(str, (int)StrLen(str));
}

bool String::StartsWith(const char *str, int len) const
{
	return StartsWith(str, str + len);
}

bool String::StartsWith(const char *str, const char *strEnd) const
{
	// main work is done here
	if (LETHE_UNLIKELY(!str || IsEmpty() || str >= strEnd))
		return 0;

	if (LETHE_UNLIKELY(strEnd - str > data->length))
		return 0;

	int ind = 0;

	while (str < strEnd)
	{
		if (data->data[ind++] != *str++)
			return 0;
	}

	return 1;
}

bool String::StartsWith(const String &str) const
{
	return StartsWith(str.Ansi(), str.GetLength());
}

bool String::EndsWith(const wchar_t ch) const
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	buf[sl] = 0;
	return EndsWith(buf, sl);
}

bool String::EndsWith(const char *str) const
{
	return EndsWith(str, str + StrLen(str));
}

bool String::EndsWith(const char *str, int len) const
{
	return EndsWith(str, str+len);
}

bool String::EndsWith(const char *str, const char *strEnd) const
{
	// main work is done here
	if (LETHE_UNLIKELY(!str || IsEmpty() || str >= strEnd))
		return 0;

	if (LETHE_UNLIKELY((strEnd - str) > data->length))
		return 0;

	int ind = data->length;

	while (str < strEnd)
	{
		if (data->data[--ind] != *--strEnd)
			return 0;
	}

	return 1;
}

bool String::EndsWith(const String &str) const
{
	return EndsWith(str.Ansi(), str.GetLength());
}

String String::Tokenize(wchar_t ch, int &pos) const
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	buf[sl] = 0;
	return Tokenize(buf, buf+sl, pos);
}

String String::Tokenize(const char *chset, int &pos) const
{
	return Tokenize(chset, nullptr, pos);
}

String String::Tokenize(const String &chset, int &pos) const
{
	if (IsEmpty())
		return String();

	LETHE_ASSERT(data);
	const char *c = chset.Ansi();
	return Tokenize(c, c + chset.GetLength(), pos);
}

Array<String> String::Split(const int ch, bool keepEmpty) const
{
	char buf[7];
	int sl = CharConv::EncodeUTF8(ch, buf, buf+6);
	buf[sl] = 0;
	return Split(String(buf, sl), keepEmpty);
}

Array<String> String::Split(const char *chset, bool keepEmpty) const
{
	return Split(String(chset), keepEmpty);
}

Array<String> String::Split(const String &chset, bool keepEmpty) const
{
	Array< String > res;

	if (IsEmpty())
		return res;

	int pos = 0;
	String tmp;

	while (pos >= 0)
	{
		tmp = Tokenize(chset, pos);

		if (keepEmpty || !tmp.IsEmpty())
			res.Add(tmp);
	}

	return res;
}

String String::Escape() const
{
	String res;
	bool changed = 0;

	for (Int i=0; i<GetLength(); i++)
	{
		wchar_t ch = Ansi()[i];

		// try to escape char...
		if (ch == '\\' || ch == '\'' || ch == '"')
		{
			changed = 1;
			break;
		}

		if (ch < 32 || ch > 127)
		{
			changed = 1;
			break;
		}
	}

	if (!changed)
		return *this;

	// pass 2: escaping...
	const char *HEX_CHARS = "0123456789abcdef";

	for (Int i=0; i<GetLength(); i++)
	{
		wchar_t ch = (wchar_t)(Ansi()[i] & 255u);

		if (ch == '\\' || ch == '\'' || ch == '"')
		{
			res += '\\';
			res += ch;
			continue;
		}

		if (ch >= 32 && ch <= 127)
		{
			res += ch;
			continue;
		}

		// handle special chars here...
		switch(ch)
		{
		case '\a':
			res += "\\a";
			continue;

		case '\b':
			res += "\\b";
			continue;

		case '\n':
			res += "\\n";
			continue;

		case '\r':
			res += "\\r";
			continue;

		case '\t':
			res += "\\t";
			continue;

		case '\v':
			res += "\\v";
			continue;
		};

		res += "\\x";

		char tmp[3] = { HEX_CHARS[(ch >> 4) & 15], HEX_CHARS[ch & 15], 0 };

		res += tmp;
	}

	return res;
}

String String::Unescape() const
{
	// TODO: implement
	return *this;
}

// load/save from binary stream
bool String::Load(Stream &s)
{
	Int len;
	LETHE_RET_FALSE(s.Read(&len, sizeof(len)));
	Endian::FromLittle(len);

	if (!len)
	{
		Reset();
		return 1;
	}

	if (len < 4096)
	{
		char buf[4096];
		LETHE_RET_FALSE(s.Read(buf, len));
		buf[len] = 0;
		*this = buf;
		return 1;
	}

	Array<char> tmp(len + 1);
	LETHE_RET_FALSE(s.Read(tmp.GetData(), len));
	tmp[len] = 0;
	*this = tmp.GetData();
	return 1;
}

bool String::Save(Stream &s) const
{
	Int len = GetLength();
	Endian::ToLittle(len);
	LETHE_RET_FALSE(s.Write(&len, sizeof(len)));

	if (!len)
		return 1;

	return s.Write(Ansi(), len);
}

bool String::Serialize(Stream &s)
{
	return s.IsWritable() ? Save(s) : Load(s);
}

// load/save from text stream
bool String::LoadText(Stream &s)
{
	Int ch = s.GetByte();
	LETHE_RET_FALSE(ch == '"');
	String tmp;

	for (;;)
	{
		ch = s.GetByte();

		if (ch < 0)
			return 0;

		if (ch == '"')
			break;

		if (ch != '\\')
		{
			tmp += (wchar_t)ch;
			continue;
		}

		tmp += (wchar_t)ch;
		ch = s.GetByte();

		if (ch < 0)
			return 0;

		tmp += (wchar_t)ch;
	}

	*this = tmp.Unescape();
	return 1;
}

bool String::SaveText(Stream &s) const
{
	String tmp = Escape();
	return s.Write("\"", 1) && (tmp.IsEmpty() ? true : s.Write(tmp.Ansi(), tmp.GetLength())) && s.Write("\"", 1);
}

// init from specific multibyte encoded string
String &String::FromEncoding(CharConv::Encoding enc, const char *str, const char *strEnd)
{
	if (!strEnd)
		strEnd = str+StrLen(str);

	Init(str, strEnd, enc);
	return *this;
}

// convert ansi representation to encoding
// note: reset when string gets modified!
String &String::ToEncoding(CharConv::Encoding enc)
{
	if (IsEmpty())
		return *this;

	if (enc == CharConv::encDefault)
		enc = CharConv::defaultEncoding;

	if (enc == CharConv::encUtf8)
		return *this;

	const char *c = data->data;
	int req = CharConv::UTF8ToAnsi(c, data->length, nullptr, 0, enc);
	LETHE_ASSERT(req != (int)-1);
	StringData *tmp = StringData::Alloc(req);
	LETHE_ASSERT(tmp);
	CharConv::UTF8ToAnsi(c, data->length, tmp->data, req, enc);
	tmp->data[tmp->length = req] = 0;
	auto odata = data;
	data = tmp;
	odata->Release();
	return *this;
}

UInt String::GetStoredHash() const
{
	return data ? Atomic::Load(data->hash) : 0;
}

const wchar_t *String::ToWide(Array<wchar_t> &wbuf) const
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

int String::GetByteIndex(int charIndex) const
{
	int res = -1;
	int maxRes = 0;

	for (auto ci : *this)
	{
		maxRes = ci.bytePos + ci.byteSize;

		if (ci.charPos >= charIndex)
		{
			res = ci.bytePos;
			break;
		}
	}

	return res < 0 ? maxRes : res;
}

int String::GetCharIndex(int byteIndex) const
{
	int res = -1;
	int maxRes = 0;

	for (auto ci : *this)
	{
		maxRes = ci.charPos+1;

		if (ci.bytePos >= byteIndex)
		{
			res = ci.charPos;
			break;
		}
	}

	return res < 0 ? maxRes : res;
}

int String::DecodeCharAt(int byteIndex) const
{
	if ((UInt)byteIndex >= (UInt)GetLength())
		return -1;

	const Byte *tmp = reinterpret_cast<const Byte *>(Ansi());
	const Byte *te = tmp + GetLength();
	tmp += byteIndex;
	return CharConv::DecodeUTF8(tmp, te);
}

int String::GetCharSizeAt(int byteIndex) const
{
	if ((UInt)byteIndex >= (UInt)GetLength())
		return 0;

	const Byte *tmp = reinterpret_cast<const Byte *>(Ansi());
	const Byte *te = tmp + GetLength();
	tmp += byteIndex;
	auto otmp = tmp;
	CharConv::DecodeUTF8(tmp, te);
	return int(tmp - otmp);
}

bool String::IsMultiByte() const
{
	return GetLength() != GetWideLength();
}

int String::GetWideLength() const
{
	if (data && data->charLength)
		return data->charLength;

	auto res = GetCharIndex(GetLength());

	if (data)
		data->charLength = res;

	return res;
}

UInt Hash(const String &s)
{
	if (LETHE_UNLIKELY(!s.data))
	{
		// equals to HashBuffer(0, 0);
		return 0x424c5fbfu;
	}

	LETHE_ASSERT(s.data);
	UInt res = Atomic::Load(s.data->hash);

	if (LETHE_UNLIKELY(!res))
	{
		// we don't care about the result here
		res = HashBuffer(s.data->data, (size_t)s.data->length*sizeof(char));
		Atomic::Store(s.data->hash, res);
	}

	return res;
}

Int String::AsInt() const
{
	return (Int)strtol(Ansi(), nullptr, 10);
}

}
