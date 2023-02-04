#pragma once

#include "String.h"
#include "../Hash/HashBuffer.h"

namespace lethe
{

// reference to literal or string
class LETHE_API StringRef
{
public:
	inline StringRef() : ref(""), refLen(0), refHash(0) {}
	// implicit

	template<size_t len>
	inline StringRef(const char (&str)[len])
		: ref(str), refLen((int)len-1), refHash(Hash(str)) {}

	LETHE_NOINLINE StringRef(const String &str) : ref(str.Ansi()), refLen(str.GetLength()), refHash(str.GetStoredHash()) {}
	// explicit
	explicit LETHE_NOINLINE StringRef(const char *str, int nlen=-1) : ref(str), refLen(nlen < 0 ? (int)StrLen(str) : nlen), refHash(0) {}
	LETHE_NOINLINE StringRef(const char *str, const char *strEnd) : ref(str), refLen(strEnd ? (int)(strEnd - str) : (int)StrLen(str)),
		refHash(0) {}

	String::CharIterator Begin() const;

	inline String::CharIterator begin() const
	{
		return Begin();
	}
	inline String::CharIterator cbegin() const
	{
		return Begin();
	}

	inline String::CharIterator End() const
	{
		return String::endIterator;
	}
	inline String::CharIterator end() const
	{
		return End();
	}
	inline String::CharIterator cend() const
	{
		return End();
	}

	inline bool operator ==(const StringRef &o) const
	{
		return refLen == o.refLen && (refLen == 0 || MemCmp(ref, o.ref, refLen * sizeof(char)) == 0);
	}

	inline bool operator !=(const StringRef &o) const
	{
		return !(*this == o);
	}

	inline char operator[](Int idx) const
	{
		LETHE_ASSERT((UInt)idx < (UInt)refLen);
		return ref[idx];
	}

	inline int GetLength() const
	{
		return refLen;
	}

	inline bool IsEmpty() const
	{
		return !refLen;
	}

	inline const char *Ansi() const
	{
		// make sure it's zero-terminated
		LETHE_ASSERT(!ref[refLen]);
		return ref;
	}

	String ToString() const
	{
		return String(ref, ref + refLen);
	}

	// convert to wide char buffer
	// returns wbuf.GetData()
	const wchar_t *ToWide(Array<wchar_t> &wbuf) const;

	friend inline UInt Hash(const StringRef &sr)
	{
		if (!sr.refHash)
			sr.refHash = HashBufferUnaligned(sr.ref, sr.refLen * sizeof(char));

		return sr.refHash;
	}

	inline const char *GetData() const
	{
		return ref;
	}

	bool StartsWith(const StringRef &o) const;
	bool EndsWith(const StringRef &o) const;

private:
	const char *ref;
	int refLen;
	mutable UInt refHash;
};

}
