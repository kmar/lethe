#pragma once

#include "../Common.h"
#include "../Collect/Array.h"

#include <stdarg.h>

namespace lethe
{

class String;
class StringRef;

LETHE_API_BEGIN

// temporary string buffer (useful for building strings)
// do NOT use this as struct/class members
class LETHE_API StringBuilder
{
public:

	StringBuilder(const char *ini = nullptr);
	StringBuilder(const char *start, const char *end);

	void Reserve(Int nreserve);
	void Clear();
	void Reset();

	inline Int GetLength() const {return data.GetSize()-trailingZero;}

	StringBuilder &Prepend(const StringRef &sr);

	StringBuilder &operator +=(Int ch);
	StringBuilder &operator +=(const char *c);
	StringBuilder &operator +=(const WChar *c);
	StringBuilder &operator +=(const StringRef &sr);
	StringBuilder &operator +=(const String &str);

	inline char operator[](Int idx) const
	{
		LETHE_ASSERT((UInt)idx < (UInt)data.GetSize());
		return data[idx];
	}

	StringBuilder &AppendFormat(const char *fmt, ...) LETHE_FORMAT_ATTR_METHOD_SUFFIX;
	StringBuilder &Format(const char *fmt, ...) LETHE_FORMAT_ATTR_METHOD_SUFFIX;

	operator StringRef() const;

	StringRef Get() const;

	const char *Ansi() const;

	// note: GetData is not zero-terminated
	inline const char *GetData() const {return data.GetData();}

	// simple, ANSI-only
	StringBuilder &ToLower();
	StringBuilder &ToUpper();

private:
	mutable StackArray<char, 1025> data;
	mutable bool trailingZero;

	void PrepareForModification();
	StringBuilder &AppendFormatInternal(const char *fmt, va_list ap);
};

LETHE_API_END

}
