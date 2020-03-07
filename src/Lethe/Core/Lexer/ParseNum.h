#pragma once

#include "../Sys/Types.h"
#include "Token.h"

namespace lethe
{

class Stream;

// new version to simplify token parsing
// (doesn't handle +- prefix)
TokenNumber LETHE_API ParseTokenNumber(bool &isDouble, Stream &s, const char **err = 0);

// simple helpers to parse int/long/float/double
// suffixes have to be handled externally
// hex numbers are parsed C style, i.e. 0x or 0777 oct
// those operate on byte stream, i.e. using GetByte/UngetByte!
Int LETHE_API ParseInt(Stream &s, const char **err = 0);
UInt LETHE_API ParseUInt(Stream &s, const char **err = 0);
Long LETHE_API ParseLong(Stream &s, const char **err = 0);
ULong LETHE_API ParseULong(Stream &s, const char **err = 0);
Float LETHE_API ParseFloat(Stream &s, const char **err = 0);
Double LETHE_API ParseDouble(Stream &s, const char **err = 0);

void LETHE_API SkipWhiteSpace(Stream &s, bool skipEol = 0);

template< typename T >
inline T ParseNum(Stream &s, const char **err = 0);

template<> inline Int ParseNum<Int>(Stream &s, const char **err)
{
	return ParseInt(s, err);
}

template<> inline UInt ParseNum<UInt>(Stream &s, const char **err)
{
	return ParseUInt(s, err);
}

template<> inline Long ParseNum<Long>(Stream &s, const char **err)
{
	return ParseLong(s, err);
}

template<> inline ULong ParseNum<ULong>(Stream &s, const char **err)
{
	return ParseULong(s, err);
}

template<> inline Float ParseNum<Float>(Stream &s, const char **err)
{
	return ParseFloat(s, err);
}

template<> inline Double ParseNum<Double>(Stream &s, const char **err)
{
	return ParseDouble(s, err);
}

}
