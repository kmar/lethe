#include "Token.h"
#include "../Memory/Memory.h"
#include "../Sys/Likely.h"

namespace lethe
{

// Token

Token::Token()
{
	LETHE_COMPILE_ASSERT(sizeof(char) == 1);
	Clear();
}

Token::Token(TokenType ntype)
{
	Clear();
	type = ntype;
}

Token::Token(const Token &o)
{
	*this = o;
}

Token &Token::operator =(const Token &o)
{
	location = o.location;
	type = o.type;
	length = o.length;
	err = o.err;
	number = o.number;
	numberFlags = o.numberFlags;
	allocText = o.allocText;
	text = allocText.IsEmpty() ? nullptr : allocText.GetData();
	return *this;
}

// add text character
Token &Token::AddTextChar(char ch)
{
	allocText.Add(ch);
	length++;
	return *this;
}

// clear
Token &Token::Clear()
{
	location.column = location.line = 0;
	type = TOK_NONE;
	length = 0;
	text = "";
	err = 0;
	allocText.Clear();
	number.l = 0;
	numberFlags = 0;
	return *this;
}

Token &Token::FinishText()
{
	// add zero terminator
	allocText.Add(0);
	text = allocText.GetData();
	return *this;
}

Token &Token::KeywordAsIdent()
{
	if (type >= TOK_KEYWORD)
		type = TOK_IDENT;

	return *this;
}

Token &Token::AnyAsIdent()
{
	if (type == TOK_STRING || type == TOK_NAME || type >= TOK_KEYWORD)
		type = TOK_IDENT;

	return *this;
}

bool Token::IsKeyword() const
{
	return type >= TOK_KEYWORD;
}

bool Token::IsKeywordOrIdent() const
{
	return type == TOK_IDENT || type >= TOK_KEYWORD;
}

bool Token::IsText() const
{
	return type == TOK_IDENT || type == TOK_STRING || type == TOK_NAME || type >= TOK_KEYWORD;
}

}
