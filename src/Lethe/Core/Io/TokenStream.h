#pragma once

#include "../Lexer/Token.h"
#include "../Memory/BucketAlloc.h"
#include "../Collect/HashMap.h"
#include "../Ptr/UniquePtr.h"

namespace lethe
{

class Lexer;

struct TokenMacro
{
	String name;
	Int macroScopeIndex = 0;
	Int locked = 0;
	Array<Token> args;
	Array<Token *> argPtrs;
	Array<Token> tokens;
	Array<Token *> tokenPtrs;

	void SwapWith(TokenMacro &o)
	{
		MemSwap(this, &o, sizeof(TokenMacro));
	}
};

using TokenMacroMap = HashMap<String, UniquePtr<TokenMacro>>;

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

	// simple token-based macro support:

	// set token macro map reference so that it can be shared among token streams
	void SetMacroMap(TokenMacroMap *nmap);

	void SetLineFileMacros(const String &lineName, const String &fileName, const String &counterName, const String &funcName);
	void SetVarArgMacros(const String &varArgName, const String &varArgCountName);
	void SetStringizeMacros(const String &stringizeName, const String &concatName);

	void SetFuncName(const String &fname);

	void EnableMacros(bool enable);

	void BeginMacroScope();
	void EndMacroScope();

	// add simple macro, swapping-in tokens
	// macro redefinitions are illegal, so this returns false if so
	bool AddSwapSimpleMacro(const String &name, Array<Token> &args, Array<Token> &tokens);

private:
	using Macro = TokenMacro;

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

	TokenMacroMap *macros = nullptr;

	Array<Token *> macroTokens;
	Array<UniquePtr<Token>> macroArgTokens;

	struct MacroStack
	{
		// index into macroTokens
		Int index;
		// end in macroTokens for currently expanding macro
		Int end;
		// index/end in macroTokens (arguments)
		Int argIndex;
		Int argEnd;
		// starting index into macroArgTokens
		Int argTokenIndex;
		// macro name
		String name;
	};

	StackArray<MacroStack, 32> macroTokenStack;
	Int macroScopeIndex;
	// last macro scope index where macros were added
	Int lastMacroScopeIndex;
	// global macro lock
	Int macroLock;

	TokenLocation macroExpandLocation;

	// equivalent for __LINE__
	String lineMacroName;
	// equivalent for __FILE__
	String fileMacroName;
	// equivalent for __COUNTER__
	String counterMacroName;
	// equivalent for __func__
	String funcMacroName;
	// equivalent for __VA_ARGS
	String varArgMacroName;
	// non-std extension __VA_COUNT
	String varArgCountMacroName;
	// non-std __stringize
	String stringizeMacroName;
	// non-std __concat
	String concatMacroName;

	ULong macroExpandCounter;
	// current function name
	String macroExpandFunction;

	bool PushMacro(Macro &m);
	void PushMacroArg(Int start, Int end);
	void PopMacro();

	bool StringizeMacroArg(const MacroStack &ms, Token &ntok, const char *argName);

	TokenType FetchToken(Token &ntok);
	TokenType FetchTokenInternal(Token &ntok);
	TokenType FetchToken_Expand(Token &ntok);

	inline Int CyclicDelta(Int x, Int y) const;
	inline void AdvanceIndex(Int &idx);
};

}
