#include "TokenStream.h"
#include "../Lexer/Lexer.h"
#include "../String/StringRef.h"
#include "../String/StringBuilder.h"

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
	, macroScopeIndex(0)
	, lastMacroScopeIndex(0)
	, macroLock(0)
	, parseMacroLock(0)
	, macroKeyword(TOK_INVALID)
	, macroExpandCounter(0)
{
	buffer.Resize(nbufSize);
	lex->DisablePeek();
}

TokenType TokenStream::FetchToken_Expand(Token &ntok)
{
	ntok = GetToken();
	return ntok.type;
}

TokenType TokenStream::FetchToken(Token &ntok)
{
	bool inmacro = !macroTokenStack.IsEmpty();
	auto res = FetchTokenInternal(ntok);

	if (!inmacro)
		return res;

	if (macroTokenStack.IsEmpty())
		return res;

	Int idx = macroTokenStack.GetSize()-1;

	while (idx >= 0 && macroTokenStack[idx].index >= macroTokenStack[idx].end)
		--idx;

	if (idx < 0 || idx >= macroTokenStack.GetSize())
		return res;

	const auto &ms = macroTokenStack[idx];
	const auto *mt = macroTokens[ms.index];

	if (mt->type != TOK_IDENT || StringRef(mt->text) != concatMacroName)
		return res;

	// can only merge identifiers/numbers
	if (ntok.type == TOK_ULONG)
	{
		// convert to ident => this is necessary for multiple concats
		// however here's a catch - you can create identifiers that start with numbers this way
		// I won't fix it - not worth the extra effort, because this is not a typical use case
		auto loc = ntok.location;
		ntok.SetString(String::Printf(LETHE_FORMAT_ULONG, ntok.number.l).Ansi());
		ntok.location = loc;
		res = ntok.type = TOK_IDENT;
	}

	if (ntok.type != TOK_IDENT)
	{
		ntok.type = TOK_INVALID;
		return TOK_INVALID;
	}

	// try to concat!
	Token nextTok;

	// skip __concat
	FetchTokenInternal(nextTok);

	auto nextType = FetchToken(nextTok);

	if (nextType != TOK_ULONG && nextType != TOK_IDENT)
	{
		ntok.type = TOK_INVALID;
		return TOK_INVALID;
	}

	// concatenate!
	StringBuilder sb = ntok.text;

	if (nextType == TOK_ULONG)
		sb.AppendFormat(LETHE_FORMAT_ULONG, nextTok.number.l);
	else
		sb += nextTok.text;

	auto loc = ntok.location;
	ntok.SetString(sb.Get().Ansi());
	ntok.type = res;
	ntok.location = loc;

	return res;
}

TokenType TokenStream::FetchTokenInternal(Token &ntok)
{
	for (;;)
	{
	loop:
		if (macroTokenStack.IsEmpty())
			return lex->GetToken(ntok);

		auto &ms = macroTokenStack.Back();

		if (ms.index >= ms.end)
		{
			PopMacro();
			continue;
		}

		ntok = *macroTokens[ms.index++];

		if (ntok.numberFlags & TOKF_MACRO_IGNORE)
			continue;

		if (ntok.type != TOK_IDENT)
			break;

		StringRef sr(ntok.text);

		if (sr == lineMacroName)
		{
			ntok.SetULong(macroExpandLocation.line);
		}
		else if (sr == counterMacroName)
		{
			ntok.SetULong(macroExpandCounter-1);
		}
		else if (sr == fileMacroName)
		{
			ntok.SetString(macroExpandLocation.file.Ansi());
		}
		else if (sr == funcMacroName)
		{
			ntok.SetString(macroExpandFunction.Ansi());
		}
		else if (sr == selfMacroName)
		{
			if (macroExpandSelf.IsEmpty())
				return TOK_INVALID;

			ntok.SetString(macroExpandSelf.Ansi());
			ntok.type = TOK_IDENT;
		}
		else
		{
			if (ms.argIndex < ms.argEnd && macroTokens[ms.argEnd-1]->type == TOK_ELLIPSIS)
			{
				auto argCount = macroTokens[ms.argEnd-1]->userIndex;

				if (sr == varArgCountMacroName)
				{
					ntok.SetULong(argCount);
					break;
				}

				if (sr == varArgOptMacroName)
				{
					if (ms.index >= ms.end || macroTokens[ms.index++]->type != TOK_LBR)
						return TOK_INVALID;

					Int start = ms.index;

					Int openBr = 0;

					while (ms.index < ms.end)
					{
						auto *mt = macroTokens[ms.index++];

						if (mt->type == TOK_LBR)
						{
							++openBr;
							continue;
						}

						if (mt->type == TOK_RBR && --openBr < 0)
						{
							// mark as ignored
							mt->numberFlags |= TOKF_MACRO_IGNORE;
							break;
						}
					}

					if (argCount)
						ms.index = start;

					continue;
				}
			}

			if (sr == stringizeMacroName && ms.index < ms.end)
			{
				auto *tok = macroTokens[ms.index++];

				if (!StringizeMacroArg(ms, ntok, tok->text))
					return TOK_INVALID;

				break;
			}

			// try args
			for (Int i=ms.argIndex; i<ms.argEnd; i++)
			{
				const auto *mt = macroTokens[i];

				StringRef cmp;

				if (mt->type == TOK_IDENT)
					cmp = StringRef(mt->text);
				else
				{
					LETHE_ASSERT(mt->type == TOK_ELLIPSIS);
					cmp = varArgMacroName;
				}

				if (sr == cmp)
				{
					// unpack
					Int start = (Int)(UInt)mt->number.l;
					Int end = (Int)(UInt)(mt->number.l >> 32);

					PushMacroArg(start, end);
					goto loop;
				}
			}
		}

		break;
	}

	return ntok.type;
}

const Token &TokenStream::GetToken()
{
retry:
	Int rp = readPtr;

	if (readPtr == writePtr)
	{
		auto owritePtr = writePtr;
		auto ofullSize = fullSize;

		buffer[writePtr].prevLocation = lex->GetTokenLocation();

		// okay, we need to fetch next token
		if (FetchToken(buffer[writePtr]) == TOK_EOF && eofIndex < eofTokens.GetSize())
		{
			auto loc = buffer[writePtr].location;
			*static_cast<Token *>(&buffer[writePtr]) = eofTokens[eofIndex++];
			buffer[writePtr].location = loc;
		}

		if (!parseMacroLock && macroKeyword != TOK_INVALID && buffer[rp].type == macroKeyword)
		{
			auto opos = position;

			++parseMacroLock;
			auto res = onParseMacro();
			--parseMacroLock;

			if (res)
			{
				while (readPtr != writePtr && buffer[readPtr].type == macroKeyword)
				{
					if (!onParseMacro())
						goto fail;
				}

				if (readPtr != writePtr)
				{
					// special handling!
					auto dst = owritePtr;

					while (readPtr != writePtr)
					{
						if (dst != readPtr)
							Swap(buffer[dst], buffer[readPtr]);

						AdvanceIndex(readPtr);
						AdvanceIndex(dst);
					}

					owritePtr = dst;
					ofullSize = Max(ofullSize, owritePtr+1);
				}

				position = opos;
				readPtr = rp;
				writePtr = owritePtr;
				fullSize = ofullSize;
				goto retry;
			}
fail:
			// parse macro failed => return invalid token
			buffer[rp].type = TOK_INVALID;
		}

		fullSize = Max(fullSize, writePtr+1);
		AdvanceIndex(writePtr);

		if (!macroLock && macros)
		{
			const auto ttype = buffer[rp].type;

			if (ttype == TOK_LBLOCK)
				BeginMacroScope();
			else if (ttype == TOK_RBLOCK)
				EndMacroScope();
			else if (ttype == TOK_IDENT)
			{
				auto idx = macros->FindIndex(StringRef(buffer[rp].text));

				if (idx >= 0)
				{
					auto &m = *macros->GetValue(idx);

					if (!m.locked)
					{
						if (macroTokenStack.IsEmpty())
						{
							macroExpandLocation = buffer[rp].location;
							++macroExpandCounter;
						}

						writePtr = owritePtr;
						fullSize = ofullSize;

						if (!PushMacro(m))
						{
							buffer[rp].type = TOK_INVALID;
							return buffer[rp];
						}

						writePtr = owritePtr;
						fullSize = ofullSize;
						readPtr = rp;

						return GetToken();
					}
				}
			}
		}
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
	return (rp < fullSize && rp != writePtr) ? buffer[rp].location : PeekToken().location;
}

bool TokenStream::Rewind()
{
	LETHE_RET_FALSE(lex->Rewind());
	readPtr = writePtr = fullSize = 0;
	position = 0;
	eofIndex = 0;
	eofTokens.Clear();
	macroScopeIndex = 0;
	lastMacroScopeIndex = 0;
	macroTokens.Clear();
	macroTokenStack.Clear();
	macroArgTokens.Clear();
	macroLock = 0;
	macroExpandCounter = 0;
	macroExpandFunction.Clear();

	return true;
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

bool TokenStream::AddSwapSimpleMacro(const String &name, Array<Token> &args, Array<Token> &tokens)
{
	LETHE_RET_FALSE(macros);
	LETHE_RET_FALSE(macros->FindIndex(name) < 0);

	macros->Insert(name, new Macro());
	auto &m = *(*macros)[name];
	m.name = name;
	m.macroScopeIndex = macroScopeIndex;
	Swap(args, m.args);
	Swap(tokens, m.tokens);

	m.argPtrs.Resize(m.args.GetSize());

	for (Int i=0; i<m.argPtrs.GetSize(); i++)
		m.argPtrs[i] = &m.args[i];

	m.tokenPtrs.Resize(m.tokens.GetSize());

	for (Int i=0; i<m.tokenPtrs.GetSize(); i++)
		m.tokenPtrs[i] = &m.tokens[i];

	lastMacroScopeIndex = macroScopeIndex;

	return true;
}

void TokenStream::BeginMacroScope()
{
	++macroScopeIndex;
}

void TokenStream::EndMacroScope()
{
	// note: == should do
	if (macros && lastMacroScopeIndex >= macroScopeIndex)
	{
		for (auto it = macros->Begin(); it != macros->End();)
		{
			if (it->value->macroScopeIndex >= macroScopeIndex)
				it = macros->Erase(it);
			else
				++it;
		}

		--lastMacroScopeIndex;
	}

	--macroScopeIndex;
}

void TokenStream::PushMacroArg(Int start, Int end)
{
	Int astart = macroTokens.GetSize();

	for (Int i=start; i<end; i++)
		macroTokens.Add(macroArgTokens[i]);

	macroTokenStack.Add(MacroStack{astart, macroTokens.GetSize(), astart, astart, macroArgTokens.GetSize(), String()});
}

bool TokenStream::PushMacro(Macro &m)
{
	LETHE_ASSERT(!m.locked);
	++m.locked;

	Array<UniquePtr<Token>> margTokens;

	if (!m.argPtrs.IsEmpty())
	{
		// need to parse macro args now!
		// ulong: lo 32 bits = index into macroTokens (start), hi 32 bits = end in macroargtokens => yes!

		Token tok;

		LETHE_RET_FALSE(FetchToken_Expand(tok) == TOK_LBR);

		bool hasEllipsis = m.argPtrs.Back()->type == TOK_ELLIPSIS;

		if (hasEllipsis)
			m.argPtrs.Back()->userIndex = 0;

		// make sure we get as many as needed...
		Int curArg = 0;
		Int numArgs = m.argPtrs.GetSize();
		Int minArgs = numArgs - hasEllipsis;
		Int maxArgs = hasEllipsis ? 128 : minArgs;

		// we'll need to count brace nesting level to properly parse arg separators...
		// token-based problem: can't do empty args! (really?)

		Int nesting = 0;
		Int argTokenStart = 0;

		auto flushArg = [&]()
		{
			auto *mt = m.argPtrs[curArg++];
			mt->number.l = ULong(argTokenStart) + ((ULong)margTokens.GetSize() << 32);
			argTokenStart = margTokens.GetSize();
		};

		bool ellipsisArgs = false;

		for (;;)
		{
			auto tt = FetchToken_Expand(tok);

			LETHE_RET_FALSE(tt != TOK_EOF && tt != TOK_INVALID);

			if (tt == TOK_RBR && --nesting < 0)
			{
				if (hasEllipsis && ellipsisArgs)
					++m.argPtrs.Back()->userIndex;
				break;
			}

			if (tt == TOK_LBR)
				++nesting;

			if (tt == TOK_COMMA && !nesting)
			{
				if (curArg < minArgs)
				{
					flushArg();
					continue;
				}
				else if (hasEllipsis)
				{
					++m.argPtrs.Back()->userIndex;
				}
			}

			ellipsisArgs = hasEllipsis && curArg >= minArgs;

			// otherwise we continue parsing args...
			margTokens.Add(new Token(tok));
		}

		LETHE_RET_FALSE(curArg < m.argPtrs.GetSize());
		flushArg();
		LETHE_RET_FALSE(curArg >= minArgs && curArg <= maxArgs);
	}

	// never push empty macros => simplifies FetchToken
	if (m.tokens.IsEmpty())
	{
		--m.locked;
		return true;
	}

	auto argTokenIndex = macroArgTokens.GetSize();
	auto argIndex = macroTokens.GetSize();

	for (auto *it : m.argPtrs)
		it->number.l += (ULong)argTokenIndex | ((ULong)argTokenIndex << 32);

	macroTokens.Append(m.argPtrs);
	macroArgTokens.Append(margTokens);

	auto argEnd = macroTokens.GetSize();

	auto start = macroTokens.GetSize();

	macroTokens.Append(m.tokenPtrs);

	auto end = macroTokens.GetSize();

	macroTokenStack.Add(MacroStack{start, end, argIndex, argEnd, argTokenIndex, m.name});

	return true;
}

void TokenStream::PopMacro()
{
	auto &s = macroTokenStack.Back();

	if (macros && !s.name.IsEmpty())
		--(*macros)[s.name]->locked;

	macroTokens.Resize(s.argIndex);
	macroArgTokens.Resize(s.argTokenIndex);
	macroTokenStack.Pop();
}

bool TokenStream::StringizeMacroArg(const MacroStack &ms, Token &ntok, const char *argName)
{
	StringRef sw(argName);
	const Token *found = nullptr;

	for (Int i=ms.argIndex; i<ms.argEnd; i++)
	{
		const auto *mt = macroTokens[i];

		StringRef cmp;

		if (mt->type == TOK_IDENT)
			cmp = StringRef(mt->text);
		else
		{
			LETHE_ASSERT(mt->type == TOK_ELLIPSIS);
			cmp = varArgMacroName;
		}

		if (cmp == sw)
		{
			found = mt;
			break;
		}
	}

	if (found)
	{
		// unpack
		Int start = (Int)(UInt)found->number.l;
		Int end = (Int)(UInt)(found->number.l >> 32);

		StringBuilder sb;

		bool lastOperator = false;

		for (Int i=start; i<end; i++)
		{
			const auto *mt = macroArgTokens[i].Get();

			bool isOperator = mt->type == TOK_LAND || mt->type == TOK_LOR;

			if (i > start && (isOperator != lastOperator || (isOperator && lastOperator)))
				sb += ' ';

			lex->StringizeToken(*mt, sb);

			lastOperator = isOperator;
		}

		ntok.SetString(sb.Get().Ansi());
		return true;
	}

	ntok.type = TOK_INVALID;
	return false;
}

void TokenStream::EnableMacros(bool enable)
{
	macroLock += enable ? 1 : -1;
}

void TokenStream::SetLineFileMacros(const String &lineName, const String &fileName, const String &counterName, const String &funcName, const String &selfName)
{
	lineMacroName = lineName;
	fileMacroName = fileName;
	counterMacroName = counterName;
	funcMacroName = funcName;
	selfMacroName = selfName;
}

void TokenStream::SetVarArgMacros(const String &varArgName, const String &varArgCountName, const String &varArgOptName)
{
	varArgMacroName = varArgName;
	varArgCountMacroName = varArgCountName;
	varArgOptMacroName = varArgOptName;
}

void TokenStream::SetStringizeMacros(const String &stringizeName, const String &concatName)
{
	stringizeMacroName = stringizeName;
	concatMacroName = concatName;
}

void TokenStream::SetFuncName(const String &fname)
{
	macroExpandFunction = fname;
}

void TokenStream::SetSelfName(const String &sname)
{
	macroExpandSelf = sname;
}

void TokenStream::SetMacroMap(TokenMacroMap *nmap)
{
	macros = nmap;
}

void TokenStream::SetMacroKeyword(TokenType tt)
{
	macroKeyword = tt;
}

}
