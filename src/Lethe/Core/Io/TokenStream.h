#pragma once

#include "../Lexer/Token.h"
#include "../Memory/BucketAlloc.h"

namespace lethe
{

class Lexer;

class LETHE_API TokenStream
{
	LETHE_BUCKET_ALLOC(TokenStream)
public:
	explicit TokenStream(Lexer &l, Int nbufSize = 128);

	const Token &GetToken();
	const Token &PeekToken();
	// unget 1 or more tokens
	bool UngetToken(Int count = 1);
	// use this after peek
	bool ConsumeToken();
	// conditional consume if next token type == ntype
	// returns 1 on success
	bool ConsumeTokenIf(TokenType ntype);

	// rewind
	bool Rewind();

	inline Lexer &GetLexer()
	{
		return *lex;
	}

	inline Int GetPosition() const
	{
		return position;
	}

	TokenLocation GetTokenLocation();
	// to support line directive
	// note: line always set, ignores column
	void SetTokenLocation(TokenLocation tl);

	// append token at EOF
	void AppendEof(const Token &tok);

	// pop EOF token
	void PopEofToken();

private:
	struct BufferedToken : public Token
	{
		TokenLocation prevLocation;
	};

	// refptr
	Lexer *lex;
	// circular buffer; might be useful to create a template for this?
	Array<BufferedToken> buffer;
	Int readPtr;
	Int writePtr;
	Int fullSize;
	Int position;

	Array<Token> eofTokens;
	Int eofIndex;

	inline Int CyclicDelta(Int x, Int y) const;
	inline void AdvanceIndex(Int &idx);
};

}
