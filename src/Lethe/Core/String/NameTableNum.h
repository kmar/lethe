#pragma once

#include "../Memory/Memory.h"
#include "../Collect/HashSet.h"
#include "../Thread/Lock.h"
#include "../String/String.h"

namespace lethe
{

class StringRef;

class LETHE_API NameTableSimple
{
	friend class NameTableNum;
public:
	Int Add(const String &str);
	const String &GetString(Int idx) const;
	Int GetSize() const;
	void Clear();
	void Reset();

private:
	HashSet<String> strings;
};

class LETHE_API NameTableNum
{
public:
	Int Add(const char *str, Int slen = -1);
	Int Add(const StringRef &str);
	String GetString(Int idx) const;
	// returns zero-terminated char buffer
	void ToCharBuffer(Int idx, Array<char> &nbuf) const;
	Int GetSize() const;
	void Clear();
	void Reset();

	// extract base string + index
	void Extract(Int nidx, String &base, Int &idx) const;

private:
	friend class NameTable;

	UInt GetStableHash(Int nidx) const;

	static Int GetNumDigits(Int n);

	struct ComplexName
	{
		// simple name table ref index
		Int name;
		// -1 = none
		Int num;

		inline bool operator ==(const ComplexName &o) const
		{
			return name == o.name && num == o.num;
		}
	};

	// stable hashes
	mutable RWMutex hashMutex;
	Array<UInt> hashes;

	HashSet<ComplexName> names;
	NameTableSimple strings;
};

}
