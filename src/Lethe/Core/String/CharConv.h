#pragma once

#include "../Sys/Types.h"

namespace lethe
{

// Encapsulates character conversion (encoding) routines
struct LETHE_API CharConv
{
	// Character encoding.
	enum Encoding
	{
		encDefault		=	0,				// default encoding
		encUtf8			=	1				// UTF-8
	};

	// default encoding (encUtf8 by default, can be overridden)
	static Encoding defaultEncoding;

	// decode utf-8 character.
	// src reference to pointer to UTF8 character
	// srcend is pointer to end (limit)
	// returns decoded unicode character index or -1 on error
	static int DecodeUTF8(const Byte *&src, const Byte * const srcend);

	// encode UTF-8 character.
	// ch unicode character to be encoded
	// dst is pointer to destination buffer
	// dstend is end-pointer (limit)
	// returns number of bytes used, 0 if failed
	static int EncodeUTF8(int ch, void *dst, const void * const dstend);

	// simply returns number of bytes used, 0 if failed
	static int GetUTF8Length(int ch);

	// convert from ansi (=multibyte) encoding to wide (UCS-2).
	// src source data
	// srcSizeBytes source data size in bytes
	// dst pointer to destination buffer
	// dstMaxChars maximum number of characters destination buffer can hold
	// enc source data encoding (defaults to default system encoding)
	// returns number of characters processed
	static int AnsiToWide(const char *src, int srcSizeBytes, wchar_t *dst, int dstMaxChars, Encoding enc = encDefault);

	// convert from wide (UCS-2) encoding to ansi (=multibyte)
	// src source data
	// srcSizeChars source data size in characters
	// dst pointer to destination buffer
	// dstMaxBytes maximum number of bytes destination buffer can hold
	// enc desired encoding (defaults to system)
	// returns number of bytes processed
	static int WideToAnsi(const wchar_t *src, int srcSizeChars, char *dst, int dstMaxBytes, Encoding enc = encDefault);

	// UTF-8 versions
	static int AnsiToUTF8(const char *src, int srcSizeBytes, char *dst, int dstMaxBytes, Encoding enc = encDefault);
	static int UTF8ToAnsi(const char *src, int srcSizeChars, char *dst, int dstMaxBytes, Encoding enc = encDefault);

	// static initializer. can force global default ANSI encoding.
	// enc: force global ANSI encoding (defaults to system)
	static void Init(Encoding enc = encDefault);
	// static de-initializer
	static void Done();
};

}
