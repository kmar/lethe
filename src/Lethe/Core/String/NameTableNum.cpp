#include "NameTableNum.h"
#include "String.h"
#include "StringRef.h"

namespace lethe
{

// NameTableSimple

const String &NameTableSimple::GetString(Int idx) const
{
	return strings.GetKey(idx);
}

Int NameTableSimple::Add(const String &str)
{
	Int res = strings.FindIndex(str);

	if (res < 0)
	{
		res = strings.GetSize();
		strings.Add(str);
	}

	return res;
}

Int NameTableSimple::GetSize() const
{
	return strings.GetSize();
}

void NameTableSimple::Clear()
{
	strings.Clear();
}

void NameTableSimple::Reset()
{
	strings.Reset();
}

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

Int NameTableNum::Add(const char *str, Int slen)
{
	LETHE_ASSERT(str);

	if (slen < 0)
		slen = (Int)StrLen(str);

	ComplexName cname;
	// extract number
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
		cname.num = -1;
	else
	{
		// so we want to convert c..end to a number (if it's a digit)
		cname.num = 0;

		while (c < end)
		{
			cname.num *= 10;
			cname.num += *c++ - '0';
		}
	}

	auto text = String(str, textlen);

	cname.name = strings.Add(text);
	Int res = names.FindIndex(cname);

	if (res < 0)
	{
		res = names.GetSize();
		names.Add(cname);

		UInt cnameHash = HashMerge(Hash(text), Hash(cname.num));

		WriteMutexLock _(hashMutex);
		hashes.Add(cnameHash);
	}

	return res;
}

Int NameTableNum::Add(const StringRef &str)
{
	return Add(str.GetData(), str.GetLength());
}

String NameTableNum::GetString(Int idx) const
{
	const auto &cname = names.GetKey(idx);
	Int nd = GetNumDigits(cname.num);

	if (!nd)
		return strings.GetString(cname.name);

	const String &ustr = strings.strings.GetKey(cname.name);
	Int ulen = ustr.GetLength();
	Int tlen = ulen + nd;
	StackArray<char, 4096> buf(tlen);

	if (ulen > 0)
		MemCpy(buf.GetData(), ustr.Ansi(), ulen);

	Int i = ulen;
	Int n = cname.num;

	if (n == 0)
		buf[i++] = '0';

	while (n > 0)
	{
		buf[i++] = '0' + n % 10;
		n /= 10;
	}

	Reverse(buf.GetData() + ulen, buf.GetData() + buf.GetSize());
	return String(buf.GetData(), buf.GetSize());
}

void NameTableNum::ToCharBuffer(Int idx, Array<char> &nbuf) const
{
	const auto &cname = names.GetKey(idx);
	Int nd = GetNumDigits(cname.num);

	if (!nd)
	{
		const auto &str = strings.GetString(cname.name);
		nbuf.Resize(str.GetLength()+1);
		MemCpy(nbuf.GetData(), str.Ansi(), str.GetLength()+1);
		return;
	}

	const String &ustr = strings.strings.GetKey(cname.name);
	Int ulen = ustr.GetLength();
	Int tlen = ulen + nd;
	nbuf.Reserve(tlen+1);
	nbuf.Resize(tlen);

	if (ulen > 0)
		MemCpy(nbuf.GetData(), ustr.Ansi(), ulen);

	Int i = ulen;
	Int n = cname.num;

	if (n == 0)
		nbuf[i++] = '0';

	while (n > 0)
	{
		nbuf[i++] = '0' + n % 10;
		n /= 10;
	}

	Reverse(nbuf.GetData() + ulen, nbuf.GetData() + nbuf.GetSize());
	nbuf.Add(0);
}

Int NameTableNum::GetSize() const
{
	return names.GetSize();
}

void NameTableNum::Clear()
{
	names.Clear();
	strings.Clear();

	WriteMutexLock _(hashMutex);
	hashes.Clear();
}

void NameTableNum::Reset()
{
	names.Reset();
	strings.Reset();

	WriteMutexLock _(hashMutex);
	hashes.Reset();
}

void NameTableNum::Extract(Int nidx, String &base, Int &idx) const
{
	const auto &cn = names.GetKey(nidx);
	base = strings.GetString(cn.name);
	idx = cn.num;
}

UInt NameTableNum::GetStableHash(Int nidx) const
{
	ReadMutexLock _(hashMutex);
	return nidx <= 0 ? 0 : hashes[nidx-1];
}

}
