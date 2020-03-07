#include "TokenStream.h"
#include "../Lexer/Lexer.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(TokenStream)

inline Int TokenStream::CyclicDelta(Int a, Int b) const
{
	return (b >= a) ? b-a : b + buffer.GetSize() - a;
}

inline void TokenStream::AdvanceIndex(Int &idx)
{
	if (++idx >= buffer.GetSize())
		idx = 0;
}

TokenStream::TokenStream(Lexer &l, Int nbufSize)
	: lex(&l)
	, readPtr(0)
	, writePtr(0)
	, fullSize(0)
	, position(0)
	, eofIndex(0)
{
	buffer.Resize(nbufSize);
	lex->DisablePeek();
}

const Token &TokenStream::GetToken()
{
	Int rp = readPtr;

	if (readPtr == writePtr)
	{
		buffer[writePtr].prevLocation = lex->GetTokenLocation();

		// okay, we need to fetch next token
		if (lex->GetToken(buffer[writePtr]) == TOK_EOF && eofIndex < eofTokens.GetSize())
		{
			auto loc = buffer[writePtr].location;
			*static_cast<Token *>(&buffer[writePtr]) = eofTokens[eofIndex++];
			buffer[writePtr].location = loc;
		}

		fullSize = Max(fullSize, writePtr+1);
		AdvanceIndex(writePtr);
	}

	AdvanceIndex(readPtr);
	position++;
	return buffer[rp];
}

const Token &TokenStream::PeekToken()
{
	if (readPtr == writePtr)
	{
		// but this advances readPtr
		Int oldPtr = readPtr;
		Int oldPos = position;
		GetToken();
		readPtr = oldPtr;
		position = oldPos;
	}

	return buffer[readPtr];
}

bool TokenStream::UngetToken(Int count)
{
	Int space = fullSize - CyclicDelta(readPtr, writePtr);
	LETHE_RET_FALSE(space >= count);
	readPtr -= count;
	position -= count;
	LETHE_ASSERT(position >= 0);

	if (readPtr < 0)
		readPtr += buffer.GetSize();

	return 1;
}

bool TokenStream::ConsumeToken()
{
	LETHE_RET_FALSE(readPtr != writePtr);
	AdvanceIndex(readPtr);
	position++;
	return 1;
}

bool TokenStream::ConsumeTokenIf(TokenType ntype)
{
	return PeekToken().type == ntype ? ConsumeToken() : 1;
}

TokenLocation TokenStream::GetTokenLocation()
{
	Int rp = readPtr;
	LETHE_ASSERT(rp >= 0);
	return (rp < fullSize) ? buffer[rp].location : PeekToken().location;
}

bool TokenStream::Rewind()
{
	LETHE_RET_FALSE(lex->Rewind());
	readPtr = writePtr = fullSize = 0;
	position = 0;
	eofIndex = 0;
	eofTokens.Clear();
	return 1;
}

void TokenStream::AppendEof(const Token &ntok)
{
	eofTokens.Add(ntok);
}

void TokenStream::PopEofToken()
{
	eofTokens.Pop();
}

void TokenStream::SetTokenLocation(TokenLocation tl)
{
	auto ridx = readPtr;

	auto lineDelta = 0;

	while (ridx != writePtr)
	{
		auto &tok = buffer[ridx];
		lineDelta += tok.location.line - tok.prevLocation.line;

		tok.location.line = tl.line + lineDelta;

		if (!tl.file.IsEmpty())
			tok.location.file = tl.file;

		if (++ridx >= buffer.GetSize())
			ridx = 0;
	}

	tl.line += lineDelta;
	lex->SetTokenLocation(tl);
}

}
