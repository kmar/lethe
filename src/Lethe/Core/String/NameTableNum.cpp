#include "NameTableNum.h"
#include "String.h"
#include "StringRef.h"

namespace lethe
{

// NameTableNum

Int NameTableNum::GetNumDigits(Int n)
{
	if (n < 0)
		return 0;

	Int res = 0;

	while (n > 0)
	{
		res++;
		n /= 10;
	}

	return Max<Int>(res, 1);
}

ULong NameTableNum::Add(const char *str, Int slen)
{
	if (!str || !*str)
		return 0;

	WriteMutexLock _(mutex);

	// reserve 0 for empty string if needed
	if (strings.IsEmpty())
		strings.Add(String());

	if (slen < 0)
		slen = (Int)StrLen(str);

	// extract number
	Int num = -1;

	const char *c = str + slen;
	const char *end = c;
	// assume maximum integer that fits in Int is 9 decimal digits
	const char *start = end - Min<Int>(9, slen);

	while (--c >= start)
	{
		if (*c < '0' || *c > '9')
			break;
	}

	c++;

	// don't allow nums to start with zeros (except if it's zero)
	while (c+1 < end && *c == '0')
		c++;

	Int textlen = (Int)(c - str);

	if (c >= end)
		num = -1;
	else
	{
		// so we want to convert c..end to a number (if it's a digit)
		num = 0;

		while (c < end)
		{
			num *= 10;
			num += *c++ - '0';
		}
	}

	auto text = StringRef(str, textlen);

	Int name = strings.FindIndex(text);

	if (name < 0)
	{
		name = strings.GetSize();
		strings.Add(text);
	}

	return ((ULong(num)+1) << 32) | name;
}

ULong NameTableNum::Add(const StringRef &str)
{
	return Add(str.GetData(), str.GetLength());
}

String NameTableNum::GetStringPrefix(ULong val) const
{
	ReadMutexLock _(mutex);
	Int idx = (Int)(UInt)val;
	return strings.GetKey(idx);
}

String NameTableNum::GetString(ULong val) const
{
	if (val == 0)
		return String();

	ReadMutexLock _(mutex);

	Int idx = (Int)(UInt)val;

	Int num = (Int)(UInt)(val >> 32)-1;
	Int nd = GetNumDigits(num);

	if (!nd)
		return strings.GetKey(idx);

	StackArray<char, 4096> buf;

	ToCharBuffer(val, buf, false);

	return String(buf.GetData(), buf.GetSize());
}

Int NameTableNum::GetSize() const
{
	ReadMutexLock _(mutex);
	return strings.GetSize();
}

void NameTableNum::Clear()
{
	WriteMutexLock _(mutex);
	strings.Clear();
}

void NameTableNum::Reset()
{
	WriteMutexLock _(mutex);
	strings.Reset();
}

void NameTableNum::Extract(ULong val, String &base, Int &idx) const
{
	if (!val)
	{
		base.Clear();
		idx = -1;
		return;
	}

	auto nidx = (Int)(UInt)val;
	ReadMutexLock _(mutex);
	base = strings.GetKey(nidx);
	idx = (Int)(UInt)(val >> 32) - 1;
}

UInt NameTableNum::GetStableHash(ULong val) const
{
	if (val == 0)
		return 0;

	UInt shash;

	{
		ReadMutexLock _(mutex);
		shash = Hash(strings.GetKey((Int)(UInt)val));
	}

	// and merge with num part of the name
	return HashMerge(shash, (UInt)(val >> 32));
}

}
