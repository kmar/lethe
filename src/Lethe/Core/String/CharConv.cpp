#include "CharConv.h"
#include "../Sys/Platform.h"
#include "../Sys/Assert.h"
#include "../Memory/Memory.h"

// this is some of my older code, may need to review

#if LETHE_OS_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	undef min			// thanks
#	undef max
#endif

namespace lethe
{

// CharConv

static const Short * const toUnicode[] =
{
	0,
	0
};

static const Short * const fromUnicode[] =
{
	0,
	0
};

CharConv::Encoding CharConv::defaultEncoding = CharConv::encUtf8;

// decode utf-8 character
int CharConv::DecodeUTF8(const Byte *&src, const Byte * const srcend)
{
	// FIXME: better?
	LETHE_ASSERT(src && srcend);

	if (src >= srcend)
		return -1;

	if (!(*src & 0x80))
		return *src++;

	int extra;
	int ch;

	if ((*src & 0xe0) == 0xc0)
	{
		// 2-char sequence
		ch = *src++ & 0x1f;
		extra = 1;
	}
	else if ((*src & 0xf0) == 0xe0)
	{
		// 3-char sequence
		ch = *src++ & 0xf;
		extra = 2;
	}
	else if ((*src & 0xf8) == 0xf0)
	{
		// 4-char sequence
		ch = *src++ & 0x7;
		extra = 3;
	}
	else if ((*src & 0xfc) == 0xf8)
	{
		// 5-char sequence
		ch = *src++ & 0x3;
		extra = 4;
	}
	else if ((*src & 0xfe) == 0xfc)
	{
		// 6-char sequence
		ch = *src++ & 0x1;
		extra = 5;
	}
	else
		return -1;

	for (int i=0; i<extra; i++)
	{
		if (src >= srcend)
			return -1;

		char c = *src;

		if ((c & 0xc0) != 0x80)
			return -1;

		++src;

		ch <<= 6;
		ch |= c & 0x3f;
	}

	return ch;
}

int CharConv::GetUTF8Length(int ch)
{
	LETHE_RET_FALSE(ch >= 0);

	if (ch < 0x80)
		return 1;

	if (ch < 0x800)
		return 2;

	if (ch < 0x10000)
		return 3;

	if (ch < 0x200000)
		return 4;

	// note: this is wrong according to latest standard which only allows up to 4 bytes
	if (ch < 0x4000000)
		return 5;

	// for bad chars assume 1 ('?')
	return ch < 0 ? 1 : 6;
}

// returns number of bytes used or 0 if failed
int CharConv::EncodeUTF8(int ch, void *dstp, const void * const dstendp)
{
	Byte *dst = reinterpret_cast<Byte *>(dstp);
	const Byte *dstend = reinterpret_cast<const Byte *>(dstendp);
	LETHE_ASSERT(dst && dstend);

	if (ch < 0)
		ch = '?';

	if (dst >= dstend)
		return 0;

	if (ch < 0x80)
	{
		// encode in 1-byte
		if (dst + 1 > dstend)
			return 0;

		*dst++ = (char)ch;
		return 1;
	}

	if (ch < 0x800)
	{
		// encode in 2-bytes
		if (dst + 2 > dstend)
			return 0;

		*dst++ = (Byte)(0xc0 | (ch>>6));
		*dst++ = (Byte)(0x80 | (ch & 0x3f));
		return 2;
	}

	if (ch < 0x10000)
	{
		// encode in 3-bytes
		if (dst + 3 > dstend)
			return 0;

		*dst++ = (Byte)(0xe0 | (ch>>12));
		*dst++ = (Byte)(0x80 | ((ch>>6) & 0x3f));
		*dst++ = (Byte)(0x80 | (ch & 0x3f));
		return 3;
	}

	if (ch < 0x200000)
	{
		// encode in 4-bytes
		if (dst + 4 > dstend)
			return 0;

		*dst++ = (Byte)(0xf0 | (ch>>18));
		*dst++ = (Byte)(0x80 | ((ch>>12) & 0x3f));
		*dst++ = (Byte)(0x80 | ((ch>>6) & 0x3f));
		*dst++ = (Byte)(0x80 | (ch & 0x3f));
		return 4;
	}

	if (ch < 0x4000000)
	{
		// encode in 5 bytes
		if (dst + 5 > dstend)
			return 0;

		*dst++ = (Byte)(0xf8 | (ch>>24));
		*dst++ = (Byte)(0x80 | ((ch>>18) & 0x3f));
		*dst++ = (Byte)(0x80 | ((ch>>12) & 0x3f));
		*dst++ = (Byte)(0x80 | ((ch>>6) & 0x3f));
		*dst++ = (Byte)(0x80 | (ch & 0x3f));
		return 5;
	}

	LETHE_ASSERT(ch <= 0x7fffffff);

	// encode in 6 bytes
	if (dst + 6 > dstend)
		return 0;

	*dst++ = (Byte)(0xfc | (ch>>30));
	*dst++ = (Byte)(0x80 | ((ch>>24) & 0x3f));
	*dst++ = (Byte)(0x80 | ((ch>>18) & 0x3f));
	*dst++ = (Byte)(0x80 | ((ch>>12) & 0x3f));
	*dst++ = (Byte)(0x80 | ((ch>>6) & 0x3f));
	*dst++ = (Byte)(0x80 | (ch & 0x3f));
	return 6;
}

int CharConv::AnsiToWide(const char *srcch, int srcSizeBytes, wchar_t *dst, int dstMaxChars, Encoding enc)
{
	const Byte *src = reinterpret_cast< const Byte *>(srcch);
	int res = 0;

	if (enc == encDefault)
		enc = defaultEncoding;

	if (enc == encUtf8)
	{
		// try to decode UTF-8
		const Byte *se = src + srcSizeBytes;

		if (!dstMaxChars)
		{
			while (src < se)
			{
				src += DecodeUTF8(src, se) <= 0;
				res++;
			}

			return res;
		}

		while (src < se)
		{
			int uni = DecodeUTF8(src, se);
			res++;

			if (--dstMaxChars < 0)
				break;

			src += uni <= 0;
			*dst++ = (unsigned)uni >= 65536 ? (wchar_t)'?' : (wchar_t)uni;
		}
	}
	else
	{
		const Short * const tbl = toUnicode[enc];

		if (!dstMaxChars)
			return srcSizeBytes;

		while (srcSizeBytes--)
		{
			int uni = tbl[*src++];

			if (!dstMaxChars--)
				break;

			*dst++ = uni == -1 ? (wchar_t)'?' : (wchar_t)uni;
			res++;
		}
	}

	return res;
}

int CharConv::WideToAnsi(const wchar_t *src, int srcSizeChars, char *dstch, int dstMaxBytes, Encoding enc)
{
	Byte *dst = reinterpret_cast<Byte *>(dstch);
	int res = 0;

	if (enc == encDefault)
		enc = defaultEncoding;

	if (enc == encUtf8)
	{
		// encode in UTF-8

		if (dstMaxBytes <= 0)
		{
			// just count bytes
			while (srcSizeChars--)
				res += GetUTF8Length(*src++);

			return res;
		}

		const Byte *de = dst + dstMaxBytes;

		while (srcSizeChars--)
		{
			wchar_t wc = *src++;
			int nb = EncodeUTF8((int)wc, dst, de);
			res += nb;
			dst += nb;
		}

		return res;
	}

	if (dstMaxBytes <= 0)
		return srcSizeChars;

	const Short * const tbl = fromUnicode[enc];

	while (srcSizeChars--)
	{
		wchar_t wc = *src++;
		int base = tbl[(wc >> 8) & 255];

		if (!dstMaxBytes--)
			break;

		if (base >= 0)
			base = tbl[base + (wc & 255)];

		*dst++ = base < 0 ? (Byte)'?' : (Byte)base;
		res++;
	}

	return res;
}

int CharConv::AnsiToUTF8(const char *srcch, int srcSizeBytes, char *dst, int dstMaxBytes, Encoding enc)
{
	const Byte *src = reinterpret_cast<const Byte *>(srcch);
	Byte *d = reinterpret_cast<Byte *>(dst);
	int res = 0;

	if (enc == encDefault)
		enc = defaultEncoding;

	if (enc == encUtf8)
	{
		if (dstMaxBytes > 0)
			MemCpy(dst, srcch, srcSizeBytes);

		return srcSizeBytes;
	}

	const Short * const tbl = toUnicode[enc];

	if (dstMaxBytes <= 0)
	{
		// just count
		int uni = tbl[*src++];
		res += GetUTF8Length(uni);
	}

	const Byte *de = d + dstMaxBytes;

	while (srcSizeBytes--)
	{
		int uni = tbl[*src++];
		int count = EncodeUTF8(uni, d, de);
		dstMaxBytes -= count;
		d += count;
		res += count;
	}

	return res;
}

int CharConv::UTF8ToAnsi(const char *src, int srcSizeChars, char *dstch, int dstMaxBytes, Encoding enc)
{
	Byte *dst = reinterpret_cast<Byte *>(dstch);
	const Byte *s = reinterpret_cast<const Byte *>(src);
	const Byte *se = s + srcSizeChars;

	int res = 0;

	if (enc == encDefault)
		enc = defaultEncoding;

	if (enc == encUtf8)
	{
		// encode in UTF-8

		if (dstMaxBytes <= 0)
			return srcSizeChars;

		const Byte *de = dst + dstMaxBytes;

		while (s < se)
		{
			int c = DecodeUTF8(s, se);
			int nb = EncodeUTF8(c, dst, de);
			res += nb;
			dst += nb;
		}

		return res;
	}

	if (dstMaxBytes <= 0)
		return srcSizeChars;

	const Short * const tbl = fromUnicode[enc];

	while (s < se)
	{
		int c = DecodeUTF8(s, se);
		int base = (UInt)c > 65535u ? (Short)-1 : tbl[(c >> 8) & 255];

		if (!dstMaxBytes--)
			break;

		if (base >= 0)
			base = tbl[base + (c & 255)];

		*dst++ = base < 0 ? (Byte)'?' : (Byte)base;
		res++;
	}

	return res;
}

void CharConv::Init(Encoding enc)
{
	if (enc != encDefault)
	{
		defaultEncoding = enc;
		return;
	}

	defaultEncoding = encUtf8;
}

void CharConv::Done()
{
}

}
