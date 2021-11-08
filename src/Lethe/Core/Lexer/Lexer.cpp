#include "Lexer.h"
#include "ParseNum.h"
#include "../String/StringBuilder.h"

namespace lethe
{

// Lexer

// keyword tables
const Lexer::LexerKeyword Lexer::KEYWORDS_LETHE[] =
{
	{"if", TOK_KEY_IF},
	{"goto", TOK_KEY_GOTO},
	{"else", TOK_KEY_ELSE},
	{"break", TOK_KEY_BREAK},
	{"continue", TOK_KEY_CONTINUE},
	{"return", TOK_KEY_RETURN},
	{"for", TOK_KEY_FOR},
	{"while", TOK_KEY_WHILE},
	{"do", TOK_KEY_DO},
	{"new", TOK_KEY_NEW},
	{"null", TOK_KEY_NULL},
	{"nullptr", TOK_KEY_NULL},
	{"true", TOK_KEY_TRUE},
	{"false", TOK_KEY_FALSE},
	{"const", TOK_KEY_CONST},
	{"constexpr", TOK_KEY_CONSTEXPR},
	{"raw", TOK_KEY_RAW},
	{"weak", TOK_KEY_WEAK},
	{"switch", TOK_KEY_SWITCH},
	{"case", TOK_KEY_CASE},
	{"default", TOK_KEY_DEFAULT},
	{"class", TOK_KEY_CLASS},
	{"struct", TOK_KEY_STRUCT},
	{"enum", TOK_KEY_ENUM},
	// types
	{"void", TOK_KEY_TYPE_VOID},
	{"bool", TOK_KEY_TYPE_BOOL},
	{"byte", TOK_KEY_TYPE_BYTE},
	{"sbyte", TOK_KEY_TYPE_SBYTE},
	{"short", TOK_KEY_TYPE_SHORT},
	{"ushort", TOK_KEY_TYPE_USHORT},
	{"char", TOK_KEY_TYPE_CHAR},
	{"int", TOK_KEY_TYPE_INT},
	{"uint", TOK_KEY_TYPE_UINT},
	{"long", TOK_KEY_TYPE_LONG},
	{"ulong", TOK_KEY_TYPE_ULONG},
	{"float", TOK_KEY_TYPE_FLOAT},
	{"double", TOK_KEY_TYPE_DOUBLE},
	// problem: variables with name => solved by not treating name as a keyword
	//{"name", TOK_KEY_TYPE_NAME},
	{"string", TOK_KEY_TYPE_STRING},
	{"namespace", TOK_KEY_NAMESPACE},
	{"static", TOK_KEY_STATIC},
	{"native", TOK_KEY_NATIVE},
	{"transient", TOK_KEY_TRANSIENT},
	{"final", TOK_KEY_FINAL},
	{"public", TOK_KEY_PUBLIC},
	{"protected", TOK_KEY_PROTECTED},
	{"private", TOK_KEY_PRIVATE},
	{"override", TOK_KEY_OVERRIDE},
	{"this", TOK_KEY_THIS},
	{"__format", TOK_KEY_FORMAT},
	{"__intrinsic", TOK_KEY_INTRINSIC},
	{"__assert", TOK_KEY_ASSERT},
	{"cast", TOK_KEY_CAST},
	{"sizeof", TOK_KEY_SIZEOF},
	{"offsetof", TOK_KEY_OFFSETOF},
	{"alignof", TOK_KEY_ALIGNOF},
	{"typeid", TOK_KEY_TYPEID},
	{"auto", TOK_KEY_AUTO},
	{"import", TOK_KEY_IMPORT},
	{"inline", TOK_KEY_INLINE},
	{"defer", TOK_KEY_DEFER},
	{"nocopy", TOK_KEY_NOCOPY},
	{"nobounds", TOK_KEY_NOBOUNDS},
	{"noinit", TOK_KEY_NOINIT},
	{"typedef", TOK_KEY_TYPEDEF},
	{"using", TOK_KEY_USING},
	{"operator", TOK_KEY_OPERATOR},
	{"nobreak", TOK_KEY_NOBREAK},
	{"editable", TOK_KEY_EDITABLE},
	{"placeable", TOK_KEY_PLACEABLE},
	{"latent", TOK_KEY_LATENT},
	{"state", TOK_KEY_STATE},
	{"ignores", TOK_KEY_IGNORES},
	{"endclass", TOK_KEY_ENDCLASS},
	{"statebreak", TOK_KEY_STATEBREAK},
	{"nontrivial", TOK_KEY_NONTRIVIAL},
	{"nodiscard", TOK_KEY_NODISCARD},
	{"macro", TOK_KEY_MACRO},
	{"endmacro", TOK_KEY_ENDMACRO},
	{"endif", TOK_KEY_ENDIF},
	{"static_assert", TOK_KEY_STATIC_ASSERT},

	{nullptr, TOK_INVALID}
};

static int StrCmp(const char *x, const char *y)
{
	while(*x == *y)
	{
		if (!*x)
			return 0;

		x++;
		y++;
	}

	return *x - *y;
}

bool Lexer::KeywordCmp::operator()(const LexerKeyword &x, const LexerKeyword &y) const
{
	return StrCmp(x.key, y.key) < 0;
}

void Lexer::InitMode()
{
	numSuffix = false;
	numSuffixDouble = false;

	if (mode == LEXM_LETHE || mode == LEXM_LETHE_DOUBLE)
	{
		keywords = KEYWORDS_LETHE;
		keywordCount = (Int)ArraySize(KEYWORDS_LETHE)-1;
		numSuffix = true;
		numSuffixDouble = mode == LEXM_LETHE;
	}
	else
	{
		keywords = 0;
		keywordCount = 0;
	}

	// build keyword map
	keywordMap.Resize(keywordCount);

	minKeywordLength = Limits<Int>::Max();
	for (Int i=0; i<keywordCount; i++)
	{
		keywordMap[i] = keywords[i];
		minKeywordLength = Min(minKeywordLength, (Int)StrLen(keywords[i].key));
	}

	KeywordCmp cmp;
	keywordMap.Sort(cmp);
}

Lexer::Lexer(LexerMode nlexm)
	: str(0)
	, mode(nlexm)
	, keywords(0)
	, keywordCount(0)
	, minKeywordLength(0)
	, numSuffix(false)
	, numSuffixDouble(false)
	, disablePeek(false)
{
	err = 0;
	loc.column = loc.line = 0;
	InitMode();
}

bool Lexer::Open(Stream &s, const String &nfilename)
{
	LETHE_RET_FALSE(Close());
	str = &s;
	loc.file = nfilename.Ansi();
	loc.line = loc.column = 1;
	str->SetColumn(1);
	return 1;
}

bool Lexer::Rewind()
{
	LETHE_RET_FALSE(str && str->Rewind());
	loc.line = loc.column = 1;
	str->SetColumn(1);
	return 1;
}

bool Lexer::Close()
{
	str = 0;
	return 1;
}

// returns true if ++, -- is supported
inline bool Lexer::SupportPlusPlus() const
{
	return 1;
}

// C-style comments?
inline bool Lexer::HaveCComments() const
{
	return true;
}

// sharp single line comments?
inline bool Lexer::HaveSharpComments() const
{
	return false;
}

TokenType Lexer::FinishPeek(TokenType type, Token &tok)
{
	tok.type = type;

	if (!disablePeek)
		last = tok;

	return type;
}

static UInt ParseNumberSuffixes(bool isDouble, bool doubleSuffix, Stream &str)
{
	UInt res = 0;
	Int ch = str.GetByte();
	Int chl = ch|32;

	if (isDouble)
	{
		// script: we only support floats or doubles; unlike C/C++,
		// no suffix is the same as f suffix (float)
		// lf or d suffix means double (but is special LEXM_LETHE_DOUBLE we behave like C/C++)
		if (chl == 'f')
			res |= TOKF_FLOAT_SUFFIX;
		else if (chl == 'l')
		{
			Int ch0 = str.GetByte();
			Int chl0 = ch0 | 32;

			if (chl0 == 'f')
				res |= TOKF_DOUBLE_SUFFIX;
			else
				str.UngetByte(ch0);
		}
		else if (chl == 'd')
			res |= TOKF_DOUBLE_SUFFIX;

		// default to float in default script mode
		if (!res)
		{
			str.UngetByte(ch);

			if (doubleSuffix)
				res |= TOKF_FLOAT_SUFFIX;
			else
				res |= TOKF_DOUBLE_SUFFIX;
		}

		return res;
	}
	else
	{
		// allow LU or UL, no long long and stuff like that
		if (chl == 'u' || chl == 'l')
		{
			res |= (chl == 'u' ? TOKF_UNSIGNED_SUFFIX : TOKF_LONG_SUFFIX);
			Int ch0 = str.GetByte();
			Int chl0 = ch0|32;

			if (chl0 != chl && (chl0 == 'u' || chl0 == 'l'))
				res |= (chl0 == 'u' ? TOKF_UNSIGNED_SUFFIX : TOKF_LONG_SUFFIX);
			else
				str.UngetByte(ch0);
		}
	}

	if (!res)
		str.UngetByte(ch);

	return res;
}

TokenType Lexer::PeekNum(Token &tok)
{
	bool isDouble;
	tok.number = ParseTokenNumber(isDouble, *str, &err);

	if (err)
	{
		tok.err = err;
		tok.type = TOK_INVALID;
	}
	else
	{
		tok.type = isDouble ? TOK_DOUBLE : TOK_ULONG;

		if (numSuffix)
			tok.numberFlags = ParseNumberSuffixes(isDouble, numSuffixDouble, *str);
	}

	return FinishPeek(tok.type, tok);
}

TokenType Lexer::PeekSharp(Token &tok)
{
	if (HaveSharpComments())
	{
		// single line comment
		Int ch0;

		do
		{
			ch0 = str->GetByte();
		}
		while (ch0 > 0 && ch0 != 13 && ch0 != 10);

		str->UngetByte(ch0);
		return TOK_NONE;
	}

	return FinishPeek(TOK_SHARP, tok);
}

TokenType Lexer::PeekDot(Token &tok)
{
	Int ch0 = str->GetByte();

	if (ch0 >= '0' && ch0 <= '9')
	{
		str->UngetByte(ch0);
		str->UngetByte('.');
		return PeekNum(tok);
	}

	if (ch0 == '*')
		return FinishPeek(TOK_CPP_DOT_MEMB, tok);

	if (ch0 == '.')
	{
		Int ch1 = str->GetByte();

		if (ch1 == '.')
			return FinishPeek(TOK_ELLIPSIS, tok);

		str->UngetByte(ch1);
		return FinishPeek(TOK_RANGE, tok);
	}

	str->UngetByte(ch0);
	return FinishPeek(TOK_DOT, tok);
}

TokenType Lexer::PeekSlash(Token &tok)
{
	Int ch0 = str->GetByte();

	if (HaveCComments())
	{
		if (ch0 == '/')
		{
			// single line comment
			do
			{
				ch0 = str->GetByte();

				if (ch0 == '\\')
				{
					// allow continuation on next line
					Int ch1 = str->GetByte();

					if (ch1 == 13 || ch1 == 10)
						NextLine(ch1, tok);
					else
						str->UngetByte(ch1);
				}
			}
			while (ch0 > 0 && ch0 != 13 && ch0 != 10);

			str->UngetByte(ch0);
			return TOK_NONE;
		}

		if (ch0 == '*')
		{
			// multi-line comment
			for(;;)
			{
				ch0 = str->GetByte();

				if (ch0 <= 0)
					break;

				if (ch0 == 13 || ch0 == 10)
				{
					// next line
					NextLine(ch0, tok);
					continue;
				}

				if (ch0 != '*')
					continue;

				ch0 = str->GetByte();

				if (ch0 == '/')
					break;

				str->UngetByte(ch0);
			}

			return TOK_NONE;
		}
	}

	if (ch0 == '=')
		return FinishPeek(TOK_DIV_EQ, tok);

	str->UngetByte(ch0);
	return FinishPeek(TOK_DIV, tok);
}

TokenType Lexer::PeekIdent(Int ch, Token &tok)
{
	// identifier
	do
	{
		tok.AddTextChar((char)ch);
		ch = str->GetByte();
	}
	while (ch == '_' || (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'));

	str->UngetByte(ch);
	tok.FinishText();
	tok.type = TOK_IDENT;

	if (tok.length >= minKeywordLength)
	{
		// lookup keyword table
		LexerKeyword key;
		key.key = tok.text;
		KeywordCmp cmp;
		Array<LexerKeyword>::ConstIterator ci = LowerBound(keywordMap.Begin(), keywordMap.End(), key, cmp);

		if (ci != keywordMap.End() && StrCmp(ci->key, tok.text) == 0)
			tok.type = ci->type;
	}

	return FinishPeek(tok.type, tok);
}

TokenType Lexer::FinishPeekWithError(TokenType tt, const char *msg, Token &tok)
{
	tok.err = err = msg;
	tok.location.line = loc.line;
	tok.location.column = str->GetColumn();
	return FinishPeek(tt, tok);
}

static Int IndexFromHexChar(Int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';

	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;

	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;

	return -1;
}

TokenType Lexer::PeekString(Int ch, Token &tok, bool raw)
{
	Int quote = ch;

	// string
	for(;;)
	{
		Long unicodeChar = -1;

		ch = str->GetByte();

		if (ch <= 0)
			break;

		// script: triple quote = raw string literal
		if (raw)
		{
			if (ch == quote)
			{
				Int ch1 = str->GetByte();

				if (ch1 == quote)
				{
					Int ch2 = str->GetByte();

					if (ch2 == quote)
						break;

					str->UngetByte(ch2);
				}

				str->UngetByte(ch1);
			}

			if (ch == 13 || ch == 10)
			{
				tok.AddTextChar('\n');
				NextLine(ch, tok);
				continue;
			}

			tok.AddTextChar((char)ch);
			continue;
		}

		if (ch == quote)
			break;

		if (ch == 13 || ch == 10)
		{
			// handle EOL
			tok.FinishText();
			return FinishPeekWithError(TOK_INVALID, "newline in string constant", tok);
		}

		if (ch == '\\')
		{
			Int ch0 = str->GetByte();

			if (ch0 == 13 || ch0 == 10)
			{
				NextLine(ch0, tok);
				continue;
			}

			if (ch0 >= '0' && ch0 <= '7')
			{
				// octal
				ch = ch0 - '0';

				for (Int i=0; i<2; i++)
				{
					ch0 = str->GetByte();

					if (ch0 >= '0' && ch0 <= '7')
					{
						ch *= 8;
						ch += ch0 - '0';
					}
					else
					{
						str->UngetByte(ch0);
						break;
					}
				}

				tok.AddTextChar((char)ch);
				continue;
			}

			switch(ch0)
			{
			case 'a':
				ch = '\a';
				break;

			case 'b':
				ch = '\b';
				break;

			case 'f':
				ch = '\f';
				break;

			case 'n':
				ch = '\n';
				break;

			case 'r':
				ch = '\r';
				break;

			case 't':
				ch = '\t';
				break;

			case 'v':
				ch = '\v';
				break;

			case 'x':
			{
				// TODO: handle error
				Int hc0 = str->GetByte();
				Int hex0 = IndexFromHexChar(hc0);

				if (hex0 < 0)
				{
					str->UngetByte(hc0);
					ch = 'x';
					break;
				}

				hex0 &= 15;
				ch = hex0;

				Int hc1 = str->GetByte();
				Int hex1 = IndexFromHexChar(hc1);

				if (hex1 < 0)
				{
					str->UngetByte(hc1);
					break;
				}

				hex1 &= 15;
				ch = hex0*16 + hex1;
			}
			break;

			case 'u':
			case 'U':
			{
				Int maxCount = ch0 == 'u' ? 4 : 8;

				Long charValue = -1;

				// TODO: handle error
				for (Int i=0; i<maxCount; i++)
				{
					Int hc = str->GetByte();
					Int hex = IndexFromHexChar(hc);

					if (hex < 0)
					{
						str->UngetByte(hc);
						break;
					}

					if (charValue < 0)
						charValue = 0;

					charValue = charValue*16 + hex;
				}

				unicodeChar = charValue;
			}
			break;

			default:
				// just prefixed char
				ch = ch0;
			}
		}

		if (unicodeChar > 0)
		{
			char buf[16];
			auto nbytes = CharConv::EncodeUTF8((Int)unicodeChar, buf, buf+16);

			for (Int i=0; i<nbytes; i++)
				tok.AddTextChar(buf[i]);
		}
		else
			tok.AddTextChar((char)ch);
	}

	tok.FinishText();
	return FinishPeek(quote == '"' ? TOK_STRING : TOK_NAME, tok);
}

TokenType Lexer::PeekColon(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == ':')
		return FinishPeek(TOK_DOUBLE_COLON, tok);

	str->UngetByte(ch);
	return FinishPeek(TOK_COLON, tok);
}

TokenType Lexer::PeekPlus(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '+' && SupportPlusPlus())
		return FinishPeek(TOK_INC, tok);

	if (ch == '=')
		return FinishPeek(TOK_PLUS_EQ, tok);

	str->UngetByte(ch);
	return FinishPeek(TOK_PLUS, tok);
}

TokenType Lexer::PeekMinus(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '-' && SupportPlusPlus())
		return FinishPeek(TOK_DEC, tok);

	if (ch == '=')
		return FinishPeek(TOK_MINUS_EQ, tok);

	if (ch == '>')
	{
		Int ch0 = str->GetByte();

		if (ch0 == '*')
			return FinishPeek(TOK_CPP_DEREF_MEMB, tok);

		str->UngetByte(ch0);
		return FinishPeek(TOK_C_DEREF, tok);
	}

	str->UngetByte(ch);
	return FinishPeek(TOK_MINUS, tok);
}

TokenType Lexer::PeekOpEq(TokenType normal, TokenType eq, Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
		return FinishPeek(eq, tok);

	str->UngetByte(ch);
	return FinishPeek(normal, tok);
}

TokenType Lexer::PeekAnd(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
		return FinishPeek(TOK_AND_EQ, tok);

	if (ch == '&')
		return FinishPeek(TOK_LAND, tok);

	str->UngetByte(ch);
	return FinishPeek(TOK_AND, tok);
}

TokenType Lexer::PeekOr(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
		return FinishPeek(TOK_OR_EQ, tok);

	if (ch == '|')
		return FinishPeek(TOK_LOR, tok);

	str->UngetByte(ch);
	return FinishPeek(TOK_OR, tok);
}

TokenType Lexer::PeekEq(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
	{
		ch = str->GetByte();

		if (ch == '=')
			return FinishPeek(TOK_EQUIV, tok);

		str->UngetByte(ch);
		return FinishPeek(TOK_EQ_EQ, tok);
	}

	str->UngetByte(ch);
	return FinishPeek(TOK_EQ, tok);
}

TokenType Lexer::PeekNot(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
	{
		ch = str->GetByte();

		if (ch == '=')
			return FinishPeek(TOK_NOT_EQUIV, tok);

		str->UngetByte(ch);
		return FinishPeek(TOK_NOT_EQ, tok);
	}

	str->UngetByte(ch);
	return FinishPeek(TOK_LNOT, tok);
}

TokenType Lexer::PeekLt(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
	{
		// <= or <=>
		ch = str->GetByte();

		if (ch == '>')
			return FinishPeek(TOK_3WAYCMP, tok);

		str->UngetByte(ch);
		return FinishPeek(TOK_LEQ, tok);
	}

	if (ch == '-')
	{
		auto nch = str->GetByte();

		if (nch == '>')
			return FinishPeek(TOK_SWAP, tok);

		str->UngetByte(nch);
	}

	if (ch == '<')
	{
		// shl or shl eq
		ch = str->GetByte();

		if (ch == '=')
			return FinishPeek(TOK_SHL_EQ, tok);

		str->UngetByte(ch);
		return FinishPeek(TOK_SHL, tok);
	}

	str->UngetByte(ch);
	return FinishPeek(TOK_LT, tok);
}

TokenType Lexer::PeekGt(Token &tok)
{
	Int ch = str->GetByte();

	if (ch == '=')
		return FinishPeek(TOK_GEQ, tok);

	if (ch == '>')
	{
		// shr, shru, shr eq or shru eq
		ch = str->GetByte();

		if (ch == '>')
		{
			ch = str->GetByte();

			if (ch == '=')
				return FinishPeek(TOK_SHRU_EQ, tok);

			str->UngetByte(ch);
			return FinishPeek(TOK_SHRU, tok);
		}

		if (ch == '=')
			return FinishPeek(TOK_SHR_EQ, tok);

		str->UngetByte(ch);
		return FinishPeek(TOK_SHR, tok);
	}

	str->UngetByte(ch);
	return FinishPeek(TOK_GT, tok);
}

void Lexer::NextLine(Int ch, Token &tok)
{
	tok.location.line = ++loc.line;

	if (ch == 13)
	{
		Int ch0 = str->GetByte();

		if (ch0 != 10)
			str->UngetByte(ch0);
	}

	str->SetColumn(1);
}

TokenType Lexer::PeekToken(Token &tok)
{
	if (last.type != TOK_NONE)
	{
		tok = last;
		return tok.type;
	}

	err = 0;
	tok.Clear();
	tok.location.line = loc.line;
	tok.location.file = loc.file;

	for (;;)
	{
		tok.location.column = str->GetColumn();
		Int ch = str->GetByte();

		if (ch <= 0)
			break;

		switch(ch)
		{
		case '\t':
		case '\v':
		case 32:
			// skip whitespace
			continue;

		case 13:
		case 10:
			// next line
			NextLine(ch, tok);
			continue;

		case '#':
		{
			TokenType tt = PeekSharp(tok);

			// returns NONE if it's a comment
			if (tt == TOK_NONE)
				continue;

			return tt;
		}

		case '.':
			return PeekDot(tok);

		case '/':
		{
			TokenType tt = PeekSlash(tok);

			// returns NONE if it's a comment
			if (tt == TOK_NONE)
				continue;

			return tt;
		}

		case 'u':
		{
			// special handling of character literals because we use '' for names
			Int q = str->GetByte();

			if (q == '\'')
			{
				Token ntok;
				TokenType tt = PeekString(q, ntok);
				String tmps(ntok.text, ntok.length);

				if (tt != TOK_NAME || tmps.GetWideLength() != 1)
					return FinishPeekWithError(TOK_INVALID, "invalid character literal", ntok);

				tok.number.l = tmps.Begin().ch;
				tok.numberFlags = 0;
				return FinishPeek(TOK_CHAR, tok);
			}

			str->UngetByte(q);
		}
		break;

		case '"':
		{
			Int b2, b3;
			b2 = str->GetByte();

			if (b2 == '"')
			{
				b3 = str->GetByte();

				if (b3 == '"')
					return PeekString(ch, tok, true);

				str->UngetByte(b3);
			}

			str->UngetByte(b2);
		}
		// fall through
		case '\'':
			return PeekString(ch, tok);

		case '{':
			return FinishPeek(TOK_LBLOCK, tok);

		case '}':
			return FinishPeek(TOK_RBLOCK, tok);

		case '(':
			return FinishPeek(TOK_LBR, tok);

		case ')':
			return FinishPeek(TOK_RBR, tok);

		case '[':
			return FinishPeek(TOK_LARR, tok);

		case ']':
			return FinishPeek(TOK_RARR, tok);

		case ',':
			return FinishPeek(TOK_COMMA, tok);

		case ';':
			return FinishPeek(TOK_SEMICOLON, tok);

		case ':':
			return PeekColon(tok);

		case '~':
			return FinishPeek(TOK_NOT, tok);

		case '?':
			return FinishPeek(TOK_QUESTION, tok);

		case '+':
			return PeekPlus(tok);

		case '-':
			return PeekMinus(tok);

		case '*':
			return PeekOpEq(TOK_MUL, TOK_MUL_EQ, tok);

		case '%':
			return PeekOpEq(TOK_MOD, TOK_MOD_EQ, tok);

		case '^':
			return PeekOpEq(TOK_XOR, TOK_XOR_EQ, tok);

		case '&':
			return PeekAnd(tok);

		case '|':
			return PeekOr(tok);

		case '=':
			return PeekEq(tok);

		case '!':
			return PeekNot(tok);

		case '<':
			return PeekLt(tok);

		case '>':
			return PeekGt(tok);

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			str->UngetByte(ch);
			return PeekNum(tok);

		case '_':
			return PeekIdent(ch, tok);
		}

		Int ch32 = ch|32;

		if (ch32 >= 'a' && ch32 <= 'z')
			return PeekIdent(ch, tok);

		// if everything else fails, we got invalid character
		return FinishPeekWithError(TOK_INVALID, "invalid character", tok);
	}

	return FinishPeek(TOK_EOF, tok);
}

TokenType Lexer::GetToken(Token &tok)
{
	if (last.type != TOK_NONE)
	{
		tok = last;
		last.Clear();
		return tok.type;
	}

	TokenType res = PeekToken(tok);
	ConsumeToken();
	return res;
}

TokenType Lexer::GetTokenLineAsIdent(Token &tok, UInt flags)
{
	LETHE_ASSERT(last.type == TOK_NONE);

	err = 0;
	tok.Clear();
	tok.location.line = loc.line;
	tok.location.file = loc.file;

	int state = 0;

	for(;;)
	{
		tok.location.column = str->GetColumn();
		Int ch = str->GetByte();

		if (ch <= 0)
			break;

		// TODO: handle // comments (unsupported for now)
		switch(ch)
		{
		case '\t':
		case '\v':
		case 32:

			// skip whitespace
			if (state == 0 && !(flags & LEXF_TRIM_WHITESPACE))
				tok.AddTextChar((char)ch);

			continue;

		case 13:
		case 10:
			str->UngetByte(ch);
			goto doneLine;

		case '#':
			if (!(flags & LEXF_IGNORE_COMMENTS) && HaveSharpComments())
			{
				str->UngetByte(ch);
				goto doneLine;
			}

			break;
		}

		if (ch < 32)
		{
			tok.err = "invalid character";
			tok.type = TOK_INVALID;
			return tok.type;
		}

		Byte b = (Byte)ch;
		tok.AddTextChar(*reinterpret_cast<const char *>(&b));
	}

doneLine:
	tok.FinishText();

	if (flags & LEXF_TRIM_WHITESPACE)
	{
		while (tok.length > 0 && tok.text[tok.length-1] <= 32)
			const_cast<char *>(tok.text)[--tok.length] = 0;
	}

	tok.type = TOK_IDENT;
	return tok.type;
}

bool Lexer::ConsumeToken()
{
	last.Clear();
	return 1;
}

const char *Lexer::GetError() const
{
	return err;
}

const TokenLocation &Lexer::GetTokenLocation() const
{
	return loc;
}

void Lexer::SetTokenLocation(const TokenLocation &tl)
{
	// we must compensate for buffered token

	Int lineDelta = 0;

	if (!tl.file.IsEmpty())
		loc.file = tl.file;

	if (last.type != TOK_NONE)
	{
		lineDelta = last.location.line - loc.line;
		last.location.file = loc.file;
	}

	// note: line always set
	loc.line = tl.line + lineDelta;
}

bool Lexer::StringizeToken(const Token &tok, StringBuilder &sb)
{
	if (tok.type >= TOK_KEYWORD)
	{
		sb += tok.text;
		return true;
	}

	// TODO: strings, chars, names? => probably not...
	const char *op = nullptr;

	switch(tok.type)
	{
	case TOK_IDENT:
		sb += tok.text;
		break;
	case TOK_ULONG:
		sb.AppendFormat(LETHE_FORMAT_ULONG, tok.number.l);
		break;
	case TOK_DOUBLE:
		sb.AppendFormat("%lg", tok.number.d);
		break;

	// operators:
	case TOK_DOT:               op = "."; break;
	case TOK_RANGE:             op = " .. "; break;
	case TOK_CPP_DOT_MEMB:      op = ".*"; break;
	case TOK_ELLIPSIS:          op = " ... "; break;
	case TOK_COLON:             op = ":"; break;
	case TOK_DOUBLE_COLON:      op = "::"; break;
	case TOK_SEMICOLON:         op = ";"; break;
	case TOK_LBLOCK:            op = "{"; break;
	case TOK_RBLOCK:            op = "}"; break;
	case TOK_LARR:              op = "["; break;
	case TOK_RARR:              op = "]"; break;
	case TOK_LBR:               op = "("; break;
	case TOK_RBR:               op = ")"; break;
	case TOK_COMMA:             op = ","; break;
	case TOK_PLUS:              op = "+"; break;
	case TOK_PLUS_EQ:           op = "+-"; break;
	case TOK_INC:               op = "++"; break;
	case TOK_MINUS:             op = "-"; break;
	case TOK_MINUS_EQ:          op = "-="; break;
	case TOK_DEC:               op = "--"; break;
	case TOK_C_DEREF:           op = "->"; break;
	case TOK_CPP_DEREF_MEMB:    op = "->*"; break;
	case TOK_MUL:               op = "*"; break;
	case TOK_MUL_EQ:            op = "*="; break;
	case TOK_DIV:               op = "/"; break;
	case TOK_DIV_EQ:            op = "/="; break;
	case TOK_MOD:               op = "%"; break;
	case TOK_MOD_EQ:            op = "%="; break;
	case TOK_SHL:               op = "<<"; break;
	case TOK_SHL_EQ:            op = "<<="; break;
	case TOK_SHR:               op = ">>"; break;
	case TOK_SHR_EQ:            op = ">>="; break;
	case TOK_SHRU:              op = ">>>"; break;
	case TOK_SHRU_EQ:           op = ">>>="; break;
	case TOK_AND:               op = "&"; break;
	case TOK_AND_EQ:            op = "&="; break;
	case TOK_LAND:              op = "&&"; break;
	case TOK_OR:                op = "|"; break;
	case TOK_OR_EQ:             op = "|="; break;
	case TOK_LOR:               op = "||"; break;
	case TOK_XOR:               op = "^"; break;
	case TOK_XOR_EQ:            op = "^="; break;
	case TOK_LNOT:              op = "!"; break;
	case TOK_NOT:               op = "~"; break;
	case TOK_QUESTION:          op = "?"; break;

	case TOK_EQ:                op = "="; break;
	case TOK_EQ_EQ:             op = "=="; break;
	case TOK_NOT_EQ:            op = "!="; break;
	case TOK_EQUIV:             op = "==="; break;
	case TOK_NOT_EQUIV:         op = "!=="; break;
	case TOK_LT:                op = "<"; break;
	case TOK_LEQ:               op = "<="; break;
	case TOK_GT:                op = ">"; break;
	case TOK_GEQ:               op = ">="; break;

	case TOK_3WAYCMP:           op = "<=>"; break;
	case TOK_SWAP:              op = "<->"; break;

	case TOK_SHARP:             op = "#"; break;

	default:
		return false;
	}

	if (op)
		sb += op;

	return true;
}

}
