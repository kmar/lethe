#include "ParseNum.h"
#include "../Io/Stream.h"
#include "../Sys/Limits.h"
#include "../Math/Math.h"

namespace lethe
{

// FIXME: this whole cpp became somewhat ugly, refactor/simplify!
// FIXME: precision/rounding problem, strtod is more complicated than I thought

static bool IsDecimalChar(Int ch)
{
	return ch >= '0' && ch <= '9';
}

static bool IsHexChar(Int ch)
{
	if (IsDecimalChar(ch))
		return 1;

	ch |= 32;
	return ch >= 'a' && ch <= 'f';
}

static bool IsBinaryChar(Int ch)
{
	return ch == '0' || ch == '1';
}

static bool IsOctChar(Int ch)
{
	return ch >= '0' && ch <= '7';
}

static Int CharToBaseN(Int ch)
{
	ch |= 32;
	return ch - (ch >= 'a')*('a' - 10 - '0') - '0';
}

static bool IsBaseNChar(Int ch, Int base)
{
	if (base == 2)
		return IsBinaryChar(ch);

	if (base == 8)
		return IsOctChar(ch);

	if (base == 10)
		return IsDecimalChar(ch);

	LETHE_ASSERT(base == 16);
	return IsHexChar(ch);
}

static bool IsDoubleExponent(Int ch, Stream &s)
{
	if (ch != 'e' && ch != 'E')
		return 0;

	Int ch1 = s.GetByte();

	if (IsDecimalChar(ch1))
	{
		s.UngetByte(ch1);
		return 1;
	}

	// check for +-
	if (ch1 != '+' && ch1 != '-')
	{
		s.UngetByte(ch1);
		return 0;
	}

	Int ch2 = s.GetByte();
	s.UngetByte(ch2);
	s.UngetByte(ch1);
	return IsDecimalChar(ch2);
}

void SkipWhiteSpace(Stream &s, bool skipEol)
{
	Int ch;

	do
	{
		ch = s.GetByte();

		if (ch < 0)
			return;

		if (!skipEol && (ch == 13 || ch == 10))
			break;
	}
	while (ch <= 32);

	s.UngetByte(ch);
}

ULong ParseULong(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	*err = 0;
	SkipWhiteSpace(s);
	// now expect + or 0..9
	Int ch = s.GetByte();

	if (ch == '+')
		ch = s.GetByte();

	if (!IsDecimalChar(ch))
	{
		*err = "expected digit";
		return 0;
	}

	ULong res = 0;

	if (ch == '0')
	{
		// either hex or oct
		Int nch0 = s.GetByte();

		if (nch0 == 'b' || nch0 == 'B')
		{
			// support C++14 binary constant
			Int nch1 = s.GetByte();

			if (!IsBinaryChar(nch1))
			{
				s.UngetByte(nch1);
				s.UngetByte(nch0);
				return 0;
			}

			// here: parse binary
			ch = nch1;

			do
			{
				ULong ores = res;
				res <<= 1;

				if (res < ores)
				{
					*err = "number too large";
					return 0;
				}

				res |= ch - '0';
				ch = s.GetByte();
			}
			while (IsBinaryChar(ch));

			s.UngetByte(ch);
			return res;
		}

		if (nch0 == 'x' || nch0 == 'X')
		{
			// may be hex
			Int nch1 = s.GetByte();

			if (!IsHexChar(nch1))
			{
				s.UngetByte(nch1);
				s.UngetByte(nch0);
				return 0;
			}

			// here: parse hex
			ch = nch1;

			do
			{
				ULong ores = res;
				res <<= 4;

				if (res < ores)
				{
					*err = "number too large";
					return 0;
				}

				ch |= 32;
				ch -= (ch >= 'a')*('a' - 10 - '0');
				res |= ch - '0';
				ch = s.GetByte();
			}
			while (IsHexChar(ch));

			s.UngetByte(ch);
			return res;
		}

		if (!IsOctChar(nch0))
		{
			s.UngetByte(nch0);
			return 0;
		}

		// here: parse oct
		ch = nch0;

		do
		{
			ULong ores = res;
			res <<= 3;

			if (res < ores)
			{
				*err = "number too large";
				return 0;
			}

			res += ch - '0';
			ch = s.GetByte();
		}
		while (IsOctChar(ch));

		s.UngetByte(ch);
		return res;
	}

	do
	{
		ULong ores = res;
		res *= 10;

		if (res < ores)
		{
			*err = "number too large";
			return 0;
		}

		res += ch - '0';
		ch = s.GetByte();
	}
	while (ch >= '0' && ch <= '9');

	s.UngetByte(ch);
	return res;
}

UInt ParseUInt(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	ULong ul = ParseULong(s, err);

	if (*err)
		return 0;

	if (ul > Limits<UInt>::Max())
	{
		*err = "number too large";
		return 0;
	}

	return (UInt)ul;
}

Int ParseInt(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	SkipWhiteSpace(s);
	// now expect +- or 0..9
	Int ch = s.GetByte();

	if (ch != '+' && ch != '-' && (ch < '0' || ch > '9'))
	{
		*err = "unexpected character/EOF";
		s.UngetByte(ch);
		return 0;
	}

	Int sign = (ch == '-') ? -1 : 1;

	if (ch != '+' && ch != '-')
		s.UngetByte(ch);

	ULong ul = ParseULong(s, err);

	if (*err)
		return 0;

	if ((sign > 0 && ul > (ULong)Limits<Int>::Max()) || (sign < 0 && ul > (ULong)Limits<Int>::Max()+1))
	{
		*err = "number too large";
		return 0;
	}

	if (sign < 0 && ul > (ULong)Limits<Int>::Max())
		return Limits<Int>::Min();

	return (Int)ul * sign;
}

Long ParseLong(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	SkipWhiteSpace(s);
	// now expect +- or 0..9
	Int ch = s.GetByte();

	if (ch != '+' && ch != '-' && (ch < '0' || ch > '9'))
	{
		*err = "unexpected character/EOF";
		s.UngetByte(ch);
		return 0;
	}

	Int sign = (ch == '-') ? -1 : 1;

	if (ch != '+' && ch != '-')
		s.UngetByte(ch);

	ULong ul = ParseULong(s, err);

	if (*err)
		return 0;

	if ((sign > 0 && ul > (ULong)Limits<Long>::Max()) || (sign < 0 && ul > (ULong)Limits<Long>::Max()+1))
	{
		*err = "number too large";
		return 0;
	}

	if (sign < 0 && ul > (ULong)Limits<Long>::Max())
		return Limits<Long>::Min();

	return (Long)ul * sign;
}

Float ParseFloat(Stream &s, const char **err)
{
	Float res = (Float)ParseDouble(s, err);

	// avoid denormals here
	if (LETHE_UNLIKELY(IsAlmostZero(res)))
		res *= 0;

	return res;
}

// this one is optional!
static Double ParseDoubleExponent(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	Int ch = s.GetByte();

	if (ch != 'e' && ch != 'E')
		s.UngetByte(ch);

	// next can be +- or digit
	Int ch0 = s.GetByte();
	Double esign = 1;

	if (ch0 == '-' || ch0 == '+')
	{
		esign = ch0 == '-' ? -1 : 1;
		Int ch1 = s.GetByte();
		s.UngetByte(ch1);

		if (ch1 < '0' || ch1 > '9')
		{
			s.UngetByte(ch0);
			s.UngetByte(ch);
			return 1;
		}
	}
	else if (ch0 < '0' || ch0 > '9')
	{
		s.UngetByte(ch0);
		s.UngetByte(ch);
		return 1;
	}
	else
		s.UngetByte(ch0);

	ch = s.GetByte();
	LETHE_ASSERT(ch >= '0' && ch <= '9');
	Double res = 0;

	do
	{
		res *= 10;
		res += ch - '0';
		ch = s.GetByte();

		if (ch == '\'')
		{
			// try C++14 tick in numeric constants
			auto ch1 = s.GetByte();

			if (!IsDecimalChar(ch1))
				s.UngetByte(ch1);
			else
				ch = ch1;
		}
	}
	while (IsDecimalChar(ch));

	s.UngetByte(ch);
	// resolve scale depending on sign...
	// this should be pow, but... could we do better?
	res *= esign;
	*err = 0;
	return Pow(10.0, res);
}

static Double ParseDoubleFraction(Stream &s, const char **err, Double add = 0)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	Int ch = s.GetByte();

	if (ch < '0' || ch > '9')
	{
		*err = "expected digit";
		s.UngetByte(ch);
		return 0;
	}

	Double res = 0;
	Double div = 1;

	do
	{
		res *= 10;
		res += ch - '0';
		div *= 10;
		ch = s.GetByte();

		if (ch == '\'')
		{
			// try C++14 tick in numeric constants
			auto ch1 = s.GetByte();

			if (!IsDecimalChar(ch1))
				s.UngetByte(ch1);
			else
				ch = ch1;
		}
	}
	while (IsDecimalChar(ch));

	s.UngetByte(ch);
	Double scl = 1;

	if (ch == 'e' || ch == 'E')
		scl = ParseDoubleExponent(s, err);

	*err = 0;
	return res * scl / div + add * scl;
}

Double ParseDoubleInternal(Stream &s, const char **err)
{
	const char *lerr;

	if (!err)
		err = &lerr;

	*err = 0;
	SkipWhiteSpace(s);
	// we can start with +-, 0..9 or .
	Double sign = 1;
	Int ch0 = s.GetByte();

	if (ch0 == '-' || ch0 == '+')
		sign = (ch0 == '-') ? -1.0 : 1.0;
	else
		s.UngetByte(ch0);

	Int ch = s.GetByte();

	if (ch == '.')
	{
		Int ch1 = s.GetByte();

		if (ch1 < '0' || ch1 > '9')
		{
			*err = "expected digit";
			s.UngetByte(ch1);
			s.UngetByte(ch);
			s.UngetByte(ch0);
			return 0;
		}

		s.UngetByte(ch1);
		return ParseDoubleFraction(s, err)*sign;
	}

	if (ch < '0' || ch > '9')
	{
		*err = "expected digit";
		s.UngetByte(ch);
		s.UngetByte(ch0);
		return 0;
	}

	Double res = 0;

	do
	{
		res *= 10;
		res += ch - '0';
		ch = s.GetByte();
	}
	while (ch >= '0' && ch <= '9');

	// now ch can be . or e
	if (ch == '.')
	{
		ch = s.GetByte();
		s.UngetByte(ch);

		if (ch >= '0' && ch <= '9')
			return ParseDoubleFraction(s, err, res)*sign;

		if (ch == 'e' || ch == 'E')
			res *= ParseDoubleExponent(s, err);
		else
			*err = 0;

		return res * sign;
	}

	Double scl = 1;
	s.UngetByte(ch);

	if (ch == 'e' || ch == 'E')
		scl = ParseDoubleExponent(s, err);

	*err = 0;
	sign *= scl;
	return res * sign;
}

Double ParseDouble(Stream &s, const char **err)
{
	Double res = ParseDoubleInternal(s, err);

	// avoid denormals here
	if (LETHE_UNLIKELY(IsAlmostZero(res)))
		res *= 0;

	return res;
}

TokenNumber ParseTokenNumber(bool &isDouble, Stream &s, const char **err)
{
	TokenNumber res;
	res.l = 0;
	isDouble = 0;

	const char *lerr;

	if (!err)
		err = &lerr;

	*err = 0;

	Int ch = s.GetByte();

	if (ch == '.')
	{
		s.UngetByte(ch);
		Double d = ParseDouble(s, err);

		if (!*err)
		{
			isDouble = 1;
			res.d = d;
		}

		return res;
	}

	if (!IsDecimalChar(ch))
	{
		s.UngetByte(ch);
		*err = "unexpected character";
		return res;
	}

	Int base = 10;

	if (ch == '0')
	{
		// check for base prefix...
		Int ch1 = s.GetByte();

		if (ch1 == 'x' || ch1 == 'X')
		{
			Int ch2 = s.GetByte();
			s.UngetByte(ch2);

			if (!IsHexChar(ch2))
			{
				s.UngetByte(ch1);
				// long 0
				return res;
			}

			base = 16;
		}
		else if (ch1 == 'b' || ch1 == 'B')
		{
			Int ch2 = s.GetByte();
			s.UngetByte(ch2);

			if (!IsBinaryChar(ch2))
			{
				s.UngetByte(ch1);
				// long 0
				return res;
			}

			base = 2;
		}
		else if (ch1 == '.')
			s.UngetByte(ch1);
		else if (!IsOctChar(ch1))
		{
			s.UngetByte(ch1);
			// long 0
			return res;
		}
		else
		{
			s.UngetByte(ch1);
			base = 8;
		}
	}

	ULong tmpl = 0;
	ULong tmpl10 = 0;
	Double tmpd = 0;
	Double tmpd10 = 0;
	bool intOverflow = 0;

	do
	{
		ULong otmpl = tmpl;
		tmpl *= base;
		tmpd *= base;
		Int val = CharToBaseN(ch);
		tmpl += val;
		tmpd += val;

		if (base != 10)
		{
			tmpl10 *= 10;
			tmpl10 += val;
			tmpd10 *= 10.0;
			tmpd10 += val;
		}

		if (tmpl < otmpl)
			intOverflow = 1;

		ch = s.GetByte();

		if (ch == '\'')
		{
			// try C++14 tick in numeric constants
			auto ch1 = s.GetByte();

			if (!IsBaseNChar(ch1, base))
				s.UngetByte(ch1);
			else
				ch = ch1;
		}
	}
	while (IsBaseNChar(ch, base));

	if (base < 10 && IsDecimalChar(ch))
	{
		// continue parsing in base 10 (*this is nonstd wrt C++ but who cares)
		base = 10;
		tmpl = tmpl10;
		tmpd = tmpd10;

		do
		{
			ULong otmpl = tmpl;
			tmpl *= 10;
			tmpd *= 10;
			Int val = ch - '0';
			tmpl += val;
			tmpd += val;

			if (tmpl < otmpl)
				intOverflow = 1;

			ch = s.GetByte();

			if (ch == '\'')
			{
				// try C++14 tick in numeric constants
				auto ch1 = s.GetByte();

				if (!IsDecimalChar(ch1))
					s.UngetByte(ch1);
				else
					ch = ch1;
			}
		}
		while (IsDecimalChar(ch));
	}

	// here we must check for proper double continuation
	if (base == 10)
	{
		if (ch == '.')
		{
			// force a double
			isDouble = 1;
			res.d = tmpd;
			Int ch1 = s.GetByte();
			s.UngetByte(ch1);

			if (IsDecimalChar(ch1))
				res.d = ParseDoubleFraction(s, err, tmpd);
			else if (ch1 == 'e' || ch1 == 'E')
				res.d *= ParseDoubleExponent(s, err);

			return res;
		}

		// check for exponent
		if (IsDoubleExponent(ch, s))
		{
			res.d = tmpd * ParseDoubleExponent(s, err);
			isDouble = 1;
			return res;
		}
	}

	s.UngetByte(ch);

	if (intOverflow)
		*err = "integer overflow";

	res.l = tmpl;
	isDouble = 0;
	return res;
}

}
