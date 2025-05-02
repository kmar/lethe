#pragma once

#include "../Common.h"

#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Core/Lexer/Token.h>

namespace lethe
{

LETHE_API_BEGIN

struct AttributeToken
{
	// location
	TokenLocation location;
	// token type
	TokenType type;
	// token text
	String text;
	// token number
	TokenNumber number;
	// optional suffixes (bit flags)
	UInt numberFlags;

	AttributeToken() = default;
	AttributeToken(const AttributeToken &) = default;
	AttributeToken &operator =(const AttributeToken &) = default;
	AttributeToken(const Token &tok)
	{
		location = tok.location;
		type = tok.type;
		text = tok.text;
		number = tok.number;
		numberFlags = tok.numberFlags;
	}
};

struct LETHE_API Attributes : RefCounted
{
	// attributes are pre-parsed as lexer tokens
	Array<AttributeToken> tokens;
};

LETHE_API_END

}
