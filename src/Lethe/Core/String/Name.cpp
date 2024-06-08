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
	return n.value ? names.GetStableHash(n.value) : 0;
}

String NameTable::GetStringPrefix(const Name &name) const
{
	return names.GetStringPrefix(name.value);
}

String NameTable::GetString(const Name &name) const
{
	MutexLock _(mutex);
	auto ci = stringCache.Find(name.value);

	if (ci != stringCache.End())
		return ci->value;

	auto tmp = names.GetString(name.value);
	stringCache[name.value] = tmp;

	if (stringCache.GetSize() > NTAB_STRCACHE_MAX)
	{
		// purge cache...
		// FIXME: better?
		stringCache.Clear();
	}

	return tmp;
}

NameTable::NameTable()
{
	names.Add(String());
}

// add name (this must be locked)
Name NameTable::Add(const char *name)
{
	if (!name || !*name)
		return Name();

	return Add(StringRef(name));
}

Name NameTable::Add(const StringRef &nname)
{
	Name res;

	if (nname.IsEmpty())
		return res;

	MutexLock _(mutex);
	res.value = names.Add(nname);

	return res;
}

NameTable &NameTable::Clear()
{
	LETHE_ASSERT(this != &NameTable::Get());
	names.Clear();
	MutexLock _(mutex);
	stringCache.Clear();
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

String Name::ToStringPrefix() const
{
	return NameTable::Get().GetStringPrefix(*this);
}

void Name::Split(String &base, Int &idx) const
{
	auto &nt = NameTable::Get();
	nt.names.Extract(value, base, idx);
}

UInt StableHash(const Name &n)
{
	if (!n.value)
		return 0;

	return NameTable::Get().GetStableHash(n);
}

}
