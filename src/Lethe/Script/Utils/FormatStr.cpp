#include "FormatStr.h"
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/TypeInfo/BaseObject.h>
#include <Lethe/Core/String/Name.h>
#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Core/Math/Math.h>
#include <string.h>

namespace lethe
{

bool FormatTypeOk(DataTypeEnum src, DataTypeEnum dst)
{
	if (dst == DT_MAX)
		return true;

	switch(src)
	{
	case DT_CHAR:
	case DT_BOOL:
	case DT_SBYTE:
	case DT_BYTE:
	case DT_SHORT:
	case DT_USHORT:
	case DT_ENUM:
	case DT_INT:
	case DT_UINT:
		src = DT_INT;
		break;

	case DT_LONG:
	case DT_ULONG:
		src = DT_LONG;
		break;

	case DT_NULL:
		src = DT_STRONG_PTR;
		break;

	case DT_RAW_PTR:
		src = DT_STRONG_PTR;
		break;

	default:
		;
	}

	return src == dst;
}

String FormatStr(const Stack &stk)
{
	Int tmp;
	return FormatStr(stk, tmp);
}

bool FormatCheckType(DataTypeEnum src, const void *dt)
{
	auto *type = static_cast<const DataType *>(dt);

	auto dst = type->type;

	return FormatTypeOk(dst, src);
}

void FormatInvalid(StringBuilder &str)
{
	str += "#err_type#";
}

String FormatStr(const Stack &stk, Int &ofs)
{
	ofs = 0;
	auto sb = FormatStrBuilder(stk, ofs);
	return sb.Get();
}

StringBuilder FormatStrBuilder(const Stack &stk, Int &ofs)
{
	StringBuilder res;
	auto str = stk.GetString(ofs);
	ofs += Stack::STRING_WORDS;
	auto numArgs = stk.GetSignedInt(ofs++);
	(void)numArgs;
	// okay, we have to format now...
	// problem with printf: unsafe! => must use special qualifiers to check args

	const char *wc = str.Ansi();

	while (*wc)
	{
		char c = *wc++;

		if (c == '\r')
		{
			if (*wc != '\n')
				res += '\n';

			continue;
		}

		if (c != '%')
		{
			res += c;
			continue;
		}

		if (*wc == '%')
		{
			res += c;
			wc++;
			continue;
		}

		// fun comes here: args!
		const char *opts = wc;

		while ((*wc >= '0' && *wc <= '9') || *wc == '-' || *wc == '+' || *wc == '.')
			wc++;

		bool isLong = (*wc && *wc == 'l');
		wc += isLong;

		const Int OPTS_MAX = 1024;
		char optstr[OPTS_MAX+3];
		optstr[0] = '%';

		Int optlen = Min(OPTS_MAX, Int(wc - opts));
		MemCpy(optstr+1, opts, optlen);
		optstr[2+optlen] = 0;
		char &fmt = optstr[1+optlen];

		switch(*wc)
		{
		case 't':
		{
			// auto
			auto *type = static_cast<const DataType *>(stk.GetPtr(ofs++));

			auto *src = reinterpret_cast<const Byte *>(stk.GetTop() + ofs);

			if (Endian::IsBig() && type->IsSmallNumber())
				src += 4-type->size;

			type->GetVariableText(res, src);

			ofs += (type->size + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;
			wc++;
			continue;
		}

		case 'c':
		{
			// char
			if (!FormatCheckType(DT_INT, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			WChar tmp[2] = { 0, 0 };
			tmp[0] = (WChar)stk.GetInt(ofs++);
			res += tmp;
			wc++;
			continue;
		}

		case 'd':
		case 'i':
		{
			// int/long
			if (!FormatCheckType(isLong ? DT_LONG : DT_INT, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			fmt = 'd';

			if (isLong)
			{
				(&fmt)[-1] = '\0';
				StringBuilder op = optstr;
				op += LETHE_FORMAT_LONG_SUFFIX;
				Long num = stk.GetSignedLong(ofs);
				ofs += Stack::LONG_WORDS;
				res.AppendFormat(op.Ansi(), num);
			}
			else
			{
				Int num = stk.GetSignedInt(ofs++);
				res.AppendFormat(optstr, num);
			}

			wc++;
			continue;
		}

		case 'u':
		{
			// uint
			if (!FormatCheckType(isLong ? DT_LONG : DT_INT, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			fmt = 'u';

			if (isLong)
			{
				(&fmt)[-1] = '\0';
				StringBuilder op = optstr;
				op += LETHE_FORMAT_ULONG_SUFFIX;
				ULong num = stk.GetLong(ofs++);
				res.AppendFormat(op.Ansi(), num);
			}
			else
			{
				UInt num = stk.GetInt(ofs++);
				res.AppendFormat(optstr, num);
			}

			wc++;
			continue;
		}

		case 'x':
		case 'X':
		{
			// int as hex
			if (!FormatCheckType(isLong ? DT_LONG : DT_INT, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			fmt = 'x';

			if (isLong)
			{
				(&fmt)[-1] = '\0';
				StringBuilder op = optstr;
				op += LETHE_FORMAT_ULONG_HEX_SUFFIX;
				ULong num = stk.GetLong(ofs++);
				res.AppendFormat(op.Ansi(), num);
			}
			else
			{
				UInt num = stk.GetInt(ofs++);
				res.AppendFormat(optstr, num);
			}

			wc++;
			continue;
		}

		case 'f':
		case 'g':
		{
			if (!FormatCheckType(isLong ? DT_DOUBLE: DT_FLOAT, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			// float/double
			Double num = isLong ? stk.GetDouble(ofs) : stk.GetFloat(ofs);
			ofs += isLong ? Stack::DOUBLE_WORDS : 1;
			fmt = 'g';

			if (optlen <= 1)
			{
				// quantize a bit...
				if (!isLong)
					num = Floor(num*1000000.0 + 0.5)/1000000.0;

				res.AppendFormat("%.16g", num);
			}
			else
				res.AppendFormat(optstr, num);

			wc++;
			continue;
		}

		case 's':
		{
			// string
			if (!FormatCheckType(DT_STRING, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			auto tmp = stk.GetString(ofs);
			ofs += Stack::STRING_WORDS;
			fmt = 's';
			res.AppendFormat(optstr, tmp.Ansi());
			wc++;
			continue;
		}

		case 'n':
		{
			if (!FormatCheckType(DT_NAME, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			Name n;
			n.SetIndex(stk.GetSignedInt(ofs++));
			fmt = 's';
			res.AppendFormat(optstr, n.ToString().Ansi());
			wc++;
			continue;
		}

		case 'p':
		{
			if (!FormatCheckType(DT_STRONG_PTR, stk.GetPtr(ofs++)))
			{
				FormatInvalid(res);
				return res;
			}

			auto ptr = stk.GetPtr(ofs++);
			fmt = 'p';
			StringBuilder extra;

			if (ptr)
			{
				auto obj = static_cast<BaseObject *>(ptr);
				res.AppendFormat("%s ", obj->GetScriptClassType()->name.Ansi());
				extra.AppendFormat("#%d/%d", obj->strongRefCount, obj->weakRefCount);
			}

			StringBuilder op = optstr;
			op += "%s";

			res.AppendFormat(op.Ansi(), ptr, extra.Ansi());
			wc++;
			continue;
		}

		default:
			;
		}

		// unknown format type => just copy!
		res += '%';

		while (opts < wc)
			res += *opts++;
	}

	return res;
}

void AnalyzeFormatStr(const String &str, Array<DataTypeEnum> &types)
{
	types.Clear();
	const char *ac = str.Ansi();

	while (*ac)
	{
		WChar c = *ac++;

		if (c != '%')
			continue;

		if (*ac == '%')
		{
			ac++;
			continue;
		}

		while ((*ac >= '0' && *ac <= '9') || (*ac == '-' || *ac == '+' || *ac == '.'))
			ac++;

		bool isLong = *ac && *ac == 'l';

		ac += isLong;

		switch (*ac)
		{
		case 'c':
		{
			types.Add(DT_INT);
			ac++;
			continue;
		}

		case 'd':
		case 'i':
		{
			// int
			types.Add(isLong ? DT_LONG : DT_INT);
			ac++;
			continue;
		}

		case 'u':
		{
			// uint
			types.Add(isLong ? DT_LONG : DT_INT);
			ac++;
			continue;
		}

		case 'x':
		case 'X':
		{
			types.Add(isLong ? DT_LONG : DT_INT);
			ac++;
			continue;
		}

		case 'f':
		case 'g':
		{
			// float
			types.Add(isLong ? DT_DOUBLE : DT_FLOAT);
			ac++;
			continue;
		}

		case 's':
		{
			// string
			types.Add(DT_STRING);
			ac++;
			continue;
		}

		case 'n':
		{
			types.Add(DT_NAME);
			ac++;
			continue;
		}

		case 'p':
		{
			types.Add(DT_STRONG_PTR);
			ac++;
			continue;
		}

		case 't':
		{
			// DT_MAX = any type
			types.Add(DT_MAX);
			ac++;
			continue;
		}

		default:
			;
		}
	}
}

}
