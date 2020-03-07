#include "StringBuilder.h"
#include "CharConv.h"
#include "String.h"
#include "StringRef.h"
#include "../Sys/Platform.h"

#include <stdlib.h>
#include <stdio.h>

namespace lethe
{

// StringBuilder

StringBuilder::StringBuilder(const char *ini)
	: trailingZero(false)
{
	// we don't want to reallocate too often
	data.Reserve(1025);

	if (ini)
		*this += ini;
}

StringBuilder::StringBuilder(const char *start, const char *end)
	: trailingZero(false)
{
	// we don't want to reallocate too often
	data.Reserve(1025);

	if (start && end)
		*this += StringRef(start, end);
}

void StringBuilder::Reserve(Int nreserve)
{
	data.Reserve(nreserve);
}

void StringBuilder::Clear()
{
	data.Clear();
	trailingZero = false;
}

void StringBuilder::Reset()
{
	data.Reset();
	trailingZero = false;
}

StringBuilder &StringBuilder::operator +=(Int ch)
{
	PrepareForModification();

	char encoded[32];
	Int nch = CharConv::EncodeUTF8(ch, encoded, encoded+32);

	for (Int i=0; i<nch; i++)
		data.Add(encoded[i]);

	return *this;
}

StringBuilder &StringBuilder::operator +=(const char *c)
{
	PrepareForModification();

	while (c && *c)
		data.Add(*c++);

	return *this;
}

StringBuilder &StringBuilder::operator +=(const WChar *c)
{
	while (c && *c)
		*this += (Int)*c++;

	return *this;
}

StringBuilder &StringBuilder::operator +=(const StringRef &sr)
{
	PrepareForModification();

	for (Int i=0; i<sr.GetLength(); i++)
		data.Add(sr[i]);

	return *this;
}

StringBuilder &StringBuilder::operator +=(const String &str)
{
	return *this += StringRef(str);
}

StringBuilder &StringBuilder::AppendFormat(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return AppendFormatInternal(fmt, ap);
}

StringBuilder &StringBuilder::Format(const char *fmt, ...)
{
	Clear();
	va_list ap;
	va_start(ap, fmt);
	return AppendFormatInternal(fmt, ap);
}

StringBuilder &StringBuilder::AppendFormatInternal(const char *fmt, va_list ap)
{
	PrepareForModification();

	// maximum format string buffer size in chars
	constexpr int maxFmtSize = 64000;

	char buf[maxFmtSize];
#ifdef LETHE_COMPILER_MSC
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
#else
	vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
	va_end(ap);
	buf[maxFmtSize - 1] = 0;
	return *this += buf;
}

StringBuilder::operator StringRef() const
{
	return Get();
}

StringRef StringBuilder::Get() const
{
	if (data.IsEmpty())
		return StringRef();

	auto *ptr = Ansi();
	return StringRef(ptr, data.GetSize() - trailingZero);
}

const char *StringBuilder::Ansi() const
{
	if (!trailingZero)
	{
		data.Add(0);
		trailingZero = true;
	}

	return data.GetData();
}

void StringBuilder::PrepareForModification()
{
	if (trailingZero)
	{
		trailingZero = false;
		data.Pop();
	}
}

StringBuilder &StringBuilder::Prepend(const StringRef &sr)
{
	PrepareForModification();

	if (!sr.IsEmpty())
		data.Insert(0, sr.GetData(), sr.GetLength());

	return *this;
}

StringBuilder &StringBuilder::ToLower()
{
	PrepareForModification();

	for (auto &it : data)
	{
		if (it >= 'A' && it <= 'Z')
			it |= 32;
	}

	return *this;
}

StringBuilder &StringBuilder::ToUpper()
{
	PrepareForModification();

	for (auto &it : data)
	{
		if (it >= 'a' && it <= 'z')
			it &= ~32;
	}

	return *this;
}

}
