#pragma once

#include "../Memory/Memory.h"
#include "../Collect/HashSet.h"
#include "../Thread/Lock.h"
#include "../String/String.h"

namespace lethe
{

class StringRef;
class NameTable;

class LETHE_API NameTableNum
{
public:
	ULong Add(const char *str, Int slen = -1);
	ULong Add(const StringRef &str);

	// none of these is thread-safe
	String GetString(ULong val) const;
	String GetStringPrefix(ULong val) const;

	// returns zero-terminated char buffer
	template<typename Cont>
	void ToCharBuffer(ULong val, Cont &nbuf, bool zeroTerminated = true) const;
	Int GetSize() const;
	void Clear();
	void Reset();

	// extract base string + index
	void Extract(ULong val, String &base, Int &idx) const;

private:
	friend class NameTable;

	UInt GetStableHash(ULong val) const;

	static Int GetNumDigits(Int n);

	// stable hashes
	mutable RWMutex mutex;
	HashSet<String> strings;
};

template<typename Cont>
void NameTableNum::ToCharBuffer(ULong val, Cont &nbuf, bool zeroTerm) const
{
	ReadMutexLock _(mutex);

	Int num = Int(val >> 32)-1;
	Int name = (Int)(UInt)val;

	Int nd = GetNumDigits(num);

	const auto &str = strings.GetKey(name);

	if (!nd)
	{
		nbuf.Resize(str.GetLength()+(Int)zeroTerm);
		MemCpy(nbuf.GetData(), str.Ansi(), str.GetLength()+(size_t)zeroTerm);
		return;
	}

	Int ulen = str.GetLength();
	Int tlen = ulen + nd;
	nbuf.Reserve(tlen+(Int)zeroTerm);
	nbuf.Resize(tlen);

	if (ulen > 0)
		MemCpy(nbuf.GetData(), str.Ansi(), ulen);

	Int i = ulen;
	Int n = num;

	if (n == 0)
		nbuf[i++] = '0';

	while (n > 0)
	{
		nbuf[i++] = '0' + n % 10;
		n /= 10;
	}

	Reverse(nbuf.GetData() + ulen, nbuf.GetData() + nbuf.GetSize());

	if (zeroTerm)
		nbuf.Add(0);
}


}
