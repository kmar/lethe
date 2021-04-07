#pragma once

#include "../Collect/Array.h"
#include "../String/String.h"

namespace lethe
{

enum TokenTypeBase
{
	TOK_INVALID = -2,	// invalid token
	TOK_NONE = -1,		// empty
	TOK_EOF,			// end of file (or stream)
	TOK_CHAR,			// char number (store in unsigned long)
	TOK_ULONG,			// unsigned long number
	TOK_DOUBLE,			// double number
	TOK_IDENT,			// identifier
	TOK_STRING,			// string
	TOK_NAME,			// name (single quoted string)
	// operators
	TOK_DOT,			// .
	TOK_RANGE,			// ..
	TOK_CPP_DOT_MEMB,	// .*
	TOK_ELLIPSIS,		// ...
	TOK_COLON,			// :
	TOK_DOUBLE_COLON,	// ::
	TOK_SEMICOLON,		// ;
	TOK_LBLOCK,			// {
	TOK_RBLOCK,			// }
	TOK_LARR,			// [
	TOK_RARR,			// ]
	TOK_LBR,			// (
	TOK_RBR,			// )

	TOK_OPERATOR,		// this just marks operator base indices

	TOK_COMMA,			// ,
	TOK_PLUS,			// +
	TOK_PLUS_EQ,		// +=
	TOK_INC,			// ++
	TOK_MINUS,			// -
	TOK_MINUS_EQ,		// -=
	TOK_DEC,			// --
	TOK_C_DEREF,		// ->
	TOK_CPP_DEREF_MEMB,	// ->*
	TOK_MUL,			// *
	TOK_MUL_EQ,			// *=
	TOK_DIV,			// /
	TOK_DIV_EQ,			// /=
	TOK_MOD,			// %
	TOK_MOD_EQ,			// %=
	TOK_SHL,			// <<
	TOK_SHL_EQ,			// <<=
	TOK_SHR,			// >>
	TOK_SHR_EQ,			// >>=
	TOK_SHRU,			// >>>
	TOK_SHRU_EQ,		// >>>=
	TOK_AND,			// &
	TOK_AND_EQ,			// &=
	TOK_LAND,			// &&
	TOK_OR,				// |
	TOK_OR_EQ,			// |=
	TOK_LOR,			// ||
	TOK_XOR,			// ^
	TOK_XOR_EQ,			// ^=
	TOK_LNOT,			// !
	TOK_NOT,			// ~
	TOK_QUESTION,		// ?

	TOK_EQ,				// =
	TOK_EQ_EQ,			// ==
	TOK_NOT_EQ,			// !=
	TOK_EQUIV,			// ===
	TOK_NOT_EQUIV,		// !==
	TOK_LT,				// <
	TOK_LEQ,			// <=
	TOK_GT,				// >
	TOK_GEQ,			// >=

	// note: not used
	TOK_3WAYCMP,		// <=>
	TOK_SWAP,			// <->

	TOK_SHARP,			// #

	// keywords
	TOK_KEYWORD,		// this just marks keyword base indices

	TOK_KEY_BREAK,
	TOK_KEY_CASE,
	TOK_KEY_DEFAULT,
	TOK_KEY_CONTINUE,
	TOK_KEY_CLASS,
	TOK_KEY_DO,
	TOK_KEY_ELSE,
	TOK_KEY_FOR,
	TOK_KEY_FUNC,
	TOK_KEY_IF,
	TOK_KEY_GOTO,
	TOK_KEY_IN,
	TOK_KEY_IS,
	TOK_KEY_OPERATOR,
	TOK_KEY_RETURN,
	TOK_KEY_STRUCT,
	TOK_KEY_SWITCH,
	TOK_KEY_TYPEOF,
	TOK_KEY_WHILE,
	// script keywords:
	TOK_KEY_ENUM,
	TOK_KEY_THIS,
	TOK_KEY_SUPER,
	TOK_KEY_TYPE_VOID,
	TOK_KEY_TYPE_BOOL,
	TOK_KEY_TYPE_BYTE,
	TOK_KEY_TYPE_SBYTE,
	TOK_KEY_TYPE_SHORT,
	TOK_KEY_TYPE_USHORT,
	TOK_KEY_TYPE_CHAR,
	TOK_KEY_TYPE_INT,
	TOK_KEY_TYPE_UINT,
	TOK_KEY_TYPE_LONG,
	TOK_KEY_TYPE_ULONG,
	TOK_KEY_TYPE_FLOAT,
	TOK_KEY_TYPE_DOUBLE,
	TOK_KEY_TYPE_NAME,
	TOK_KEY_TYPE_STRING,
	TOK_KEY_TRUE,
	TOK_KEY_FALSE,
	TOK_KEY_NEW,
	TOK_KEY_THROW,
	TOK_KEY_NULL,
	TOK_KEY_CONST,
	TOK_KEY_CONSTEXPR,
	TOK_KEY_RAW,
	TOK_KEY_WEAK,
	TOK_KEY_NAMESPACE,
	TOK_KEY_NATIVE,
	TOK_KEY_STATIC,
	TOK_KEY_TRANSIENT,
	TOK_KEY_FINAL,
	TOK_KEY_PUBLIC,
	TOK_KEY_PROTECTED,
	TOK_KEY_PRIVATE,
	TOK_KEY_OVERRIDE,
	TOK_KEY_CAST,
	TOK_KEY_SIZEOF,
	TOK_KEY_OFFSETOF,
	TOK_KEY_ALIGNOF,
	TOK_KEY_TYPEID,
	TOK_KEY_AUTO,
	TOK_KEY_IMPORT,
	TOK_KEY_VAR,
	TOK_KEY_TRY,
	TOK_KEY_CATCH,
	// __format to validate format strings
	TOK_KEY_FORMAT,
	// __intrinsic for sqrt etc
	TOK_KEY_INTRINSIC,
	// __assert to mark assert dummy function
	TOK_KEY_ASSERT,
	TOK_KEY_INLINE,
	TOK_KEY_DEFER,
	TOK_KEY_NOCOPY,
	TOK_KEY_NOBOUNDS,
	TOK_KEY_NOINIT,
	TOK_KEY_TYPEDEF,
	TOK_KEY_USING,
	TOK_KEY_NOBREAK,
	TOK_KEY_EDITABLE,
	// editor hint
	TOK_KEY_PLACEABLE,
	TOK_KEY_LATENT,
	TOK_KEY_STATE,
	TOK_KEY_IGNORES,
	TOK_KEY_ENDCLASS,
	TOK_KEY_STATEBREAK,
	TOK_KEY_NONTRIVIAL,
	TOK_KEY_NODISCARD,
	TOK_KEY_MACRO,
	TOK_KEY_ENDMACRO,
	TOK_KEY_ENDIF
};

typedef TokenTypeBase TokenType;

struct TokenLocation
{
	Int column, line;
	// file; note that string data is reference-counted so no allocation problems here
	String file;
};

union TokenNumber
{
	ULong l;
	Double d;
};

enum TokenNumberFlags
{
	TOKF_UNSIGNED_SUFFIX	=	1,
	TOKF_LONG_SUFFIX		=	2,
	TOKF_FLOAT_SUFFIX		=	4,
	TOKF_DOUBLE_SUFFIX		=	8
};

// note: we don't wrap anything here with methods, that would be just useless waste of effort
class LETHE_API Token
{
public:
	// location
	TokenLocation location;
	// token type
	TokenType type;
	// text length in chars
	Int length;
	// token text
	const char *text;		// ref ptr, points to either small buffer or allocText
	const char *err;		// error message (null = none)
	// token number
	TokenNumber number;
	// optional suffixes (bit flags)
	UInt numberFlags;
	// used by script macro __VA_COUNT
	UInt userIndex;

	Token();
	explicit Token(TokenType ntype);
	Token(const Token &o);

	Token &operator =(const Token &o);

	// add text character
	Token &AddTextChar(char ch);
	// add text string
	Token &AddTextString(const char *str);
	// clear token
	Token &Clear();
	// sets up text
	Token &FinishText();

	// full token initializers:
	Token &SetString(const char *str);
	Token &SetULong(ULong value);

	// is keyword?
	bool IsKeyword() const;
	// is keyword or identifier?
	bool IsKeywordOrIdent() const;
	// is keyword/name/string/identifier?
	bool IsText() const;

	// convert keyword type to ident
	Token &KeywordAsIdent();

	// convert string/name/keyword to string
	Token &AnyAsIdent();

	// char treated as number as well
	inline bool IsNumber() const
	{
		return type == TOK_ULONG || type == TOK_DOUBLE || type == TOK_CHAR;
	}
private:
	StackArray<char, 256 > allocText;
};

}
