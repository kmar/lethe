#pragma once

#include "../Sys/Types.h"
#include "../Sys/Singleton.h"
#include "../Collect/HashMap.h"

#include "NameTableNum.h"

namespace lethe
{

// global name table
// this is a singleton...
// I wanted to avoid these at all costs but... names are global
// what about name filling?
// consider loading tons (dozens) of custom levels
// this will be a problem IF we use stuff like Light1, ... => NOT AT ALL!!!

struct Name;
class String;

class LETHE_API NameTable
{
	friend struct Name;

	NameTableNum names;
	mutable HashMap<ULong, String> stringCache;

	mutable Mutex mutex;

	LETHE_SINGLETON(NameTable)

public:
	NameTable();
	~NameTable();

	String GetString(const Name &name) const;
	template<typename Cont>
	void ToCharBuffer(const Name &name, Cont &nbuf, bool zeroTerminated = true) const;

	String GetStringPrefix(const Name &name) const;

	// get size (string part only since number part is now encoded within name value)
	int GetSize() const;

	// add name (this must be locked)
	Name Add(const char *name);
	Name Add(const StringRef &name);

	// fixup string
	void FixupString(String &str);

	// clear name table (DON'T USE ON GLOBAL nametable!)
	NameTable &Clear();

	// get stable (serializable) hash for name
	UInt GetStableHash(Name n) const;
};

// name: index into global name table
struct LETHE_API Name
{
private:
	friend class NameTable;
	// name value (0 should be empty string)
	// hi 32 bits: number+1, lo 32 bits: string index => table
	ULong value;
public:

	inline Name() : value(0) {}
	Name(const char *str);
	Name(const String &str);

	Name &operator =(const char *str);
	Name &operator =(const String &str);
	operator String () const;

	inline bool IsEmpty() const
	{
		return value == 0;
	}

	inline ULong GetValue() const
	{
		return value;
	}

	// note: use this at your own risk!
	inline void SetValue(ULong val)
	{
		value = val;
	}

	// -1 = none
	inline Int GetNumber() const
	{
		return (Int)((UInt)(value >> 32)-1);
	}

	void SetNumber(Int num);

	String ToString() const;

	String ToStringPrefix() const;

	template<typename Cont>
	void ToCharBuffer(Cont &buf, bool zeroTerminated = true) const;

	inline bool operator ==(const Name &o) const
	{
		return value == o.value;
	}
	inline bool operator !=(const Name &o) const
	{
		return value != o.value;
	}
	inline bool operator <(const Name &o) const
	{
		return value < o.value;
	}
	// fast hash version, unstable
	friend inline UInt Hash(const Name &n)
	{
		return (UInt)Hash(n.value);
	}

	// stable version, to be used for serialization
	friend UInt LETHE_API StableHash(const Name &n);

	inline Name &Clear()
	{
		value = 0;
		return *this;
	}

	// split name to base and index part
	// idx -1 means none
	void Split(String &base, Int &idx) const;
};

template<typename Cont>
void NameTable::ToCharBuffer(const Name &name, Cont &nbuf, bool zeroTerminated) const
{
	MutexLock _(mutex);
	names.ToCharBuffer(name.value, nbuf, zeroTerminated);
}

template<typename Cont>
void Name::ToCharBuffer(Cont &nbuf, bool zeroTerminated) const
{
	return NameTable::Get().ToCharBuffer(*this, nbuf, zeroTerminated);
}

}
