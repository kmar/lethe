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
	mutable HashMap< Int, String > stringCache;

	// this ugly thing is here because Microsoft broke NatVis in some 2017 update... *sigh*
#if LETHE_DEBUG
	mutable Array<String> debugNames;
#endif

	mutable Mutex mutex;

	LETHE_SINGLETON(NameTable)

public:
	NameTable();
	~NameTable();

	String GetString(const Name &name) const;
	void ToCharBuffer(const Name &name, Array<char> &nbuf) const;

	// get size
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
	Int index;		// name index (0 should be empty string)
public:

	inline Name() : index(0) {}
	Name(const char *str);
	Name(const String &str);

	Name &operator =(const char *str);
	Name &operator =(const String &str);
	operator String () const;

	inline bool IsEmpty() const
	{
		return index == 0;
	}

	inline Int GetIndex() const
	{
		return index;
	}

	// note: use this at your own risk!
	inline void SetIndex(Int idx)
	{
		index = idx;
	}

	String ToString() const;

	void ToCharBuffer(Array<char> &buf) const;

	inline bool operator ==(const Name &o) const
	{
		return index == o.index;
	}
	inline bool operator !=(const Name &o) const
	{
		return index != o.index;
	}
	inline bool operator <(const Name &o) const
	{
		return index < o.index;
	}
	// fast hash version, unstable
	friend inline UInt Hash(const Name &n)
	{
		return (UInt)Hash(n.index);
	}

	// stable version, to be used for serialization
	friend UInt StableHash(const Name &n);

	inline Name &Clear()
	{
		index = 0;
		return *this;
	}

	// split name to base and index part
	// idx -1 means none
	void Split(String &base, Int &idx) const;
};

}
