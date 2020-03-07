#include "Name.h"
#include "String.h"
#include "StringRef.h"
#include "../Sys/Assert.h"
#include "../Sys/Limits.h"

namespace lethe
{

// NameTable

LETHE_SINGLETON_INSTANCE(NameTable)

static const Int NTAB_STRCACHE_MAX = 8192;

UInt NameTable::GetStableHash(Name n) const
{
	return !n.index ? 0 : names.GetStableHash(n.index);
}

String NameTable::GetString(const Name &name) const
{
	MutexLock _(mutex);
	HashMap<Int, String>::ConstIterator ci = stringCache.Find(name.index);

	if (ci != stringCache.End())
		return ci->value;

	String tmp = names.GetString(name.index);
	stringCache[name.index] = tmp;

	if (stringCache.GetSize() > NTAB_STRCACHE_MAX)
	{
		// purge cache...
		// FIXME: better?
		stringCache.Clear();
	}

	return tmp;
}

void NameTable::ToCharBuffer(const Name &name, Array<char> &nbuf) const
{
	MutexLock _(mutex);
	names.ToCharBuffer(name.index, nbuf);
}

NameTable::NameTable()
{
	names.Add(String());
#if LETHE_DEBUG
	debugNames.Add(String());
#endif
}

// add name (this must be locked)
Name NameTable::Add(const char *name)
{
	if (!name || !*name)
	{
		Name res;
		res.index = 0;
		return res;
	}

	return Add(StringRef(name));
}

Name NameTable::Add(const StringRef &nname)
{
	Name res;

	if (nname.IsEmpty())
		return res;

	MutexLock _(mutex);
	res.index = names.Add(nname);

#if LETHE_DEBUG
	if (res.index >= debugNames.GetSize())
	{
		LETHE_VERIFY(res.index == debugNames.Add(nname.Ansi()));
	}
#endif

	return res;
}

NameTable &NameTable::Clear()
{
	LETHE_ASSERT(this != &NameTable::Get());
	MutexLock _(mutex);
	names.Clear();
#if LETHE_DEBUG
	debugNames.Clear();
#endif
	return *this;
}

// fixup string
void NameTable::FixupString(String &str)
{
	Name n = Add(str);
	str = GetString(n);
}

NameTable::~NameTable()
{
}

// get size
int NameTable::GetSize() const
{
	MutexLock _(mutex);
	return names.GetSize();
}

// Name

Name::Name(const char *str)
{
	*this = NameTable::Get().Add(str);
}

Name::Name(const String &str)
{
	*this = NameTable::Get().Add(str);
}

Name::operator String () const
{
	return ToString();
}

Name &Name::operator =(const char *str)
{
	return *this = NameTable::Get().Add(str);
}

Name &Name::operator =(const String &str)
{
	return *this = NameTable::Get().Add(str);
}

String Name::ToString() const
{
	return NameTable::Get().GetString(*this);
}

void Name::ToCharBuffer(Array<char> &nbuf) const
{
	return NameTable::Get().ToCharBuffer(*this, nbuf);
}

void Name::Split(String &base, Int &idx) const
{
	auto &nt = NameTable::Get();
	MutexLock _(nt.mutex);
	nt.names.Extract(index, base, idx);
}

UInt StableHash(const Name &n)
{
	if (!n.index)
		return 0;

	return NameTable::Get().GetStableHash(n);
}

}
