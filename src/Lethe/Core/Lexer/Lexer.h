#pragma once

#include "Token.h"
#include "../Io/Stream.h"
#include "../Sys/NoCopy.h"
#include "../Collect/HashMap.h"

namespace lethe
{

class StringBuilder;

enum LexerMode
{
	LEXM_LETHE,          // lethe default
	LEXM_LETHE_DOUBLE    // lethet with 1.2 being double by default instead of float
};

// for GetTokenLineAsIdent
enum LexerFlags
{
	LEXF_IGNORE_COMMENTS = 1,
	LEXF_TRIM_WHITESPACE = 2
};

LETHE_API_BEGIN

// priorities implicitly defined by parser (recursive descent)

class LETHE_API Lexer : NoCopy
{
public:
	Lexer(LexerMode nlexm = LEXM_LETHE);

	// note: does NOT rewind stream here => the caller must handle this if necessary
	// also, Lexer doesn't do buffering!
	bool Open(Stream &s, const String &nfilename);
	bool Close();

	// rewind and start over
	// note: calls rewind on ref stream; this might not be what you wanted
	bool Rewind();

	// token functions:
	TokenType GetToken(Token &tok);
	TokenType PeekToken(Token &tok);
	// this parses the rest of the line as identifier
	// NOTE: bypasses last token (if any!)
	TokenType GetTokenLineAsIdent(Token &tok, UInt flags = LEXF_IGNORE_COMMENTS | LEXF_TRIM_WHITESPACE);

	// use this after peek
	bool ConsumeToken();

	// stringize and append to StringBuilder
	bool StringizeToken(const Token &tok, StringBuilder &sb);

	// diagnostics
	const char *GetError() const;
	const TokenLocation &GetTokenLocation() const;

	// to support line directive
	// note: line always set, ignores column
	void SetTokenLocation(const TokenLocation &tl);

	// saves useless token copy
	void DisablePeek()
	{
		disablePeek = true;
	}

	struct LexerKeyword
	{
		const char *key;
		TokenType type;
	};

	// made public because these can be useful
	static const LexerKeyword KEYWORDS_LETHE[];

private:
	struct KeywordCmp
	{
		bool operator ()(const LexerKeyword &x, const LexerKeyword &y) const;
	};

	const char *err;
	TokenLocation loc;
	Stream *str;
	Token last;
	LexerMode mode;

	// using binary search (or not)
	const LexerKeyword *keywords;
	Int keywordCount;
	Array< LexerKeyword > keywordMap;

	Int minKeywordLength;

	// parse number suffixes?
	bool numSuffix;
	// special mode for script where 2.3 is float by default instead of float
	bool numSuffixDouble;
	bool disablePeek;

	void InitMode();

	// returns true if ++, -- is supported
	inline bool SupportPlusPlus() const;
	// C-style comments?
	inline bool HaveCComments() const;
	// sharp single line comments?
	inline bool HaveSharpComments() const;

	TokenType FinishPeek(TokenType type, Token &tok);
	TokenType FinishPeekWithError(TokenType tt, const char *msg, Token &tok);

	// process # character
	TokenType PeekSharp(Token &tok);
	// process . character
	TokenType PeekDot(Token &tok);
	// process slash character
	TokenType PeekSlash(Token &tok);
	// process number
	TokenType PeekNum(Token &tok);
	// process identifier
	TokenType PeekIdent(Int ch, Token &tok);
	// process string
	TokenType PeekString(Int ch, Token &tok, bool raw = false);
	// process colon
	TokenType PeekColon(Token &tok);
	// process +
	TokenType PeekPlus(Token &tok);
	// process -
	TokenType PeekMinus(Token &tok);
	// op(=)
	TokenType PeekOpEq(TokenType normal, TokenType eq, Token &tok);
	// process &
	TokenType PeekAnd(Token &tok);
	// process |
	TokenType PeekOr(Token &tok);
	// process =
	TokenType PeekEq(Token &tok);
	// process !
	TokenType PeekNot(Token &tok);
	// process <
	TokenType PeekLt(Token &tok);
	// process >
	TokenType PeekGt(Token &tok);

	// handle next line after \r or \n
	void NextLine(Int ch, Token &tok);
};

LETHE_API_END

}
