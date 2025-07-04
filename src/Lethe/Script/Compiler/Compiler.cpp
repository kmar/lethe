#include <Lethe/Core/Io/Stream.h>
#include <Lethe/Core/Io/MemoryStream.h>
#include <Lethe/Core/Io/VfsFile.h>
#include <Lethe/Core/Io/BufferedStream.h>
#include <Lethe/Core/Sys/Path.h>
#include <Lethe/Core/Lexer/Lexer.h>
#include <Lethe/Core/String/StringRef.h>
#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Core/Time/Timer.h>

#include "Compiler.h"
#include "Warnings.h"

#include <Lethe/Script/Program/CompiledProgram.h>

#include "AstIncludes.h"

#include <stdio.h>

namespace lethe
{

// Compiler

const String &Compiler::AddStringRef(StringRef sr)
{
	Int idx = stringTable.FindIndex(sr);

	if (idx >= 0)
		return stringTable.GetKey(idx);

	String nstr(sr.ToString());
	stringTable.Add(nstr);
	return *stringTable.Find(nstr);
}

const String &Compiler::AddString(const char *str)
{
	return AddStringRef(StringRef(str));
}

Compiler::Compiler(Threaded, AtomicInt *pstaticInitCtr)
	: currentScope(nullptr)
	, currentProgram(nullptr)
	, structAccess(0)
	, nofail(0)
	, templateAccum(0)
	, staticInitCounter(0)
	, pstaticInitCounter(pstaticInitCtr)
	, floatLitIsDouble(false)
{
	globalScope = new NamedScope(NSCOPE_GLOBAL);
}

Compiler::Compiler()
	: currentScope(nullptr)
	, currentProgram(nullptr)
	, structAccess(0)
	, nofail(0)
	, templateAccum(0)
	, staticInitCounter(0)
	, pstaticInitCounter(&staticInitCounter)
	, floatLitIsDouble(false)
{
	globalScope = new NamedScope(NSCOPE_GLOBAL);
	InitNativeTypeScopes();

	onError.Set(&DefaultOnError);
	onWarning.Set(&DefaultOnWarning);
}

// just here so that UniquePtr<NameScope> compiles
Compiler::~Compiler()
{
}

void Compiler::InitTokenStream()
{
	// we only use __LINE instead of __LINE__ to avoid clashes with external preprocessor
	ts->SetLineFileMacros("__LINE", "__FILE", "__COUNTER", "__func", "__self");
	ts->SetVarArgMacros("__VA_ARGS", "__VA_COUNT", "__VA_OPT");
	ts->SetStringizeMacros("__stringize", "__concat");
	ts->SetMacroMap(&macroMap);
	ts->SetMacroKeyword(TOK_KEY_MACRO);
	ts->onParseMacro = [this]()->bool
	{
		return ParseMacro(0);
	};
}

void Compiler::SetFloatLiteralIsDouble(bool nfloatLitIsDouble)
{
	floatLitIsDouble = nfloatLitIsDouble;
}

bool Compiler::Open(Stream &s, const String &nfilename)
{
	if (!s.IsOpen())
		return Error(String::Printf("Stream not open: %s", nfilename.Ansi()));

	lex = new Lexer(floatLitIsDouble ? LEXM_LETHE_DOUBLE : LEXM_LETHE);

	if (!lex->Open(s, nfilename))
		return Error(String::Printf("Lex open failed: %s", nfilename.Ansi()));

	ts = new TokenStream(*lex);
	InitTokenStream();
	return true;
}

bool Compiler::OpenBuffered(Stream &s, const String &nfilename, Double *ioTime)
{
	PerfWatch pw;
	pw.Start();

	if (!s.IsOpen())
		return Error(String::Printf("Stream not open: %s", nfilename.Ansi()));

	lex = new Lexer(floatLitIsDouble ? LEXM_LETHE_DOUBLE : LEXM_LETHE);

	fileBuffer.Clear();

	if (!s.ReadAll(fileBuffer, 1))
		return Error(String::Printf("Couldn't read %s", nfilename.Ansi()));

	fileBuffer.Add(0);

	auto deltaUs = pw.Stop();

	if (ioTime)
		*ioTime += (Double)deltaUs / 1000000.0;

	tempStream = new MemoryStream(reinterpret_cast<const char *>(fileBuffer.GetData()));

	if (!lex->Open(*tempStream, nfilename))
		return Error(String::Printf("Lex open failed: %s", nfilename.Ansi()));

	ts = new TokenStream(*lex);
	InitTokenStream();
	return true;
}

void Compiler::DefaultOnError(const String &msg, const TokenLocation &loc)
{
	printf("Error: %s at line %d near col %d in file %s",
		msg.Ansi(), loc.line, loc.column, loc.file.Ansi());
}

void Compiler::DefaultOnWarning(const String &msg, const TokenLocation &loc, Int warnid)
{
	printf("Warning (%d): %s at line %d near col %d in file %s",
		warnid, msg.Ansi(), loc.line, loc.column, loc.file.Ansi());
}

bool Compiler::Error(const String &msg)
{
	onError(msg, ts->GetTokenLocation());
	return false;
}

void Compiler::Warning(const String &msg, Int warnid)
{
	onWarning(msg, ts->GetTokenLocation(), warnid);
}

void Compiler::ErrorLoc(const String &msg, const TokenLocation &nloc)
{
	onError(msg, nloc);
}

void Compiler::WarningLoc(const String &msg, const TokenLocation &nloc, Int warnid)
{
	onWarning(msg, nloc, warnid);
}

bool Compiler::ExpectLoc(bool expr, const char *msg, const TokenLocation &nloc)
{
	if (!expr)
	{
		if (nofail <= 0)
			ErrorLoc(msg, nloc);

		return 0;
	}

	return 1;
}

bool Compiler::Expect(bool expr, const char *msg)
{
	if (!expr)
	{
		if (nofail <= 0)
			Error(msg);

		return 0;
	}

	return 1;
}

bool Compiler::ExpectPrev(bool expr, const char *msg)
{
	if (!expr)
	{
		if (nofail <= 0)
		{
			ts->UngetToken();
			Error(msg);
		}

		return 0;
	}

	return 1;
}

bool Compiler::CheckDepth(Int ndepth)
{
	if (ndepth < MAX_DEPTH)
		return 1;

	Error("maximum recursion depth reached");
	return 0;
}

NamedScope *Compiler::AddUniqueNamedScope(const String &nname)
{
	if (!currentScope->IsUniqueName(nname))
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		Expect(false, tmp.Ansi());
		return nullptr;
	}

	NamedScope *res = new NamedScope;
	res->parent = currentScope;
	res->name = nname;
	currentScope->namedScopes[nname] = res;
	return res;
}

NamedScope *Compiler::FindAddNamedScope(const String &nname)
{
	if (!currentScope->IsUniqueMemberName(nname))
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		Expect(0, tmp.Ansi());
		return nullptr;
	}

	Int idx = currentScope->namedScopes.FindIndex(nname);

	if (idx >= 0)
		return currentScope->namedScopes.GetValue(idx);

	NamedScope *res = new NamedScope;
	res->parent = currentScope;
	res->name = nname;
	currentScope->namedScopes[nname] = res;
	return res;
}

bool Compiler::AddScopeMember(const String &nname, AstNode *nnode, bool isCtor)
{
	if ((isCtor && currentScope->ctorDefined) || !currentScope->IsUniqueName(nname))
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		return ExpectLoc(0, tmp.Ansi(), nnode->location);
	}

	ErrorHandler::CheckShadowing(currentScope, nname, nnode, onWarning);

	if (isCtor)
		currentScope->ctorDefined = true;
	else
		currentScope->members[nname] = nnode;

	return true;
}

bool Compiler::AddScopeLabel(const String &nname, AstNode *nnode)
{
	if (currentScope->labels.FindIndex(nname) >= 0)
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		return ExpectLoc(0, tmp.Ansi(), nnode->location);
	}

	currentScope->labels[nname] = nnode;

	return true;
}

struct NamespaceStack
{
	AstNode *node;
	NamedScope *scope;
	Int pop;
};

Attributes *Compiler::ParseAttributes()
{
	const auto &tok = ts->GetToken();
	LETHE_RET_FALSE(ExpectPrev(tok.type == TOK_LARR, "not an attribute list"));

	UniquePtr<Attributes> res = new Attributes;

	Int count = 1;

	for (;;)
	{
		const auto &t = ts->GetToken();

		if (t.type == TOK_INVALID || t.type == TOK_EOF)
			LETHE_RET_FALSE(ExpectPrev(false, "unexpected end of attribute list"));

		if (t.type == TOK_RARR && !--count)
			break;

		count += t.type == TOK_LARR;

		AttributeToken at(t);

		if (!at.text.IsEmpty())
			at.text = AddStringRef(at.text);

		res->tokens.Add(at);
	}

	return res.Detach();
}

bool Compiler::ParseDirective(Int lineNumber)
{
	const auto &tok = ts->GetToken();
	// try to parse line directive
	LETHE_RET_FALSE(ExpectPrev(tok.type == TOK_IDENT && tok.location.line == lineNumber, "directive must start with an identifier"));

	StringRef sr(tok.text);

	if (sr == "line")
	{
		// parse line directive, optionally
		auto *nt = &ts->GetToken();

		LETHE_RET_FALSE(ExpectPrev(nt->type == TOK_ULONG && nt->location.line == lineNumber, "line number expected"));
		Int nextLine = (Int)nt->number.l;

		TokenLocation loc;

		nt = &ts->PeekToken();

		if (nt->type == TOK_STRING && nt->location.line == lineNumber)
			loc.file = ts->GetToken().text;

		loc.column = 0;
		loc.line = nextLine-1;
		ts->SetTokenLocation(loc);
	}
	else
	{
		return ExpectPrev(false, String::Printf("unknown directive: `%s'", sr.Ansi()).Ansi());
	}

	return true;
}

bool Compiler::ConditionalEnabled() const
{
	return conditionalStack.IsEmpty() || (conditionalStack.Back() & CSF_ACTIVE) != 0;
}

bool Compiler::EnterIfMacro(bool cond, Int depth, bool nopush)
{
	auto csize = conditionalStack.GetSize();

	if (!nopush)
		conditionalStack.Add(cond ? CSF_ACTIVE | CSF_IF_TAKEN : 0);

	if (cond)
		return true;

	// consume now
	ts->EnableMacros(false);

	for (;;)
	{
		const auto &t = ts->GetToken();

		if (t.type == TOK_EOF || t.type == TOK_INVALID)
			break;

		if (t.type != TOK_KEY_MACRO)
			continue;

		ts->UngetToken(1);

		ts->EnableMacros(true);
		LETHE_RET_FALSE(ParseMacro(depth+1, true));

		ts->EnableMacros(false);

		if (conditionalStack.GetSize() < csize || ConditionalEnabled())
			break;
	}

	ts->EnableMacros(true);
	return true;
}

bool Compiler::EndIfMacro()
{
	if (conditionalStack.IsEmpty())
		return ExpectPrev(false, "unexpected conditional endif");

	conditionalStack.Pop();

	return true;
}

bool Compiler::ParseMacro(Int depth, bool conditionalOnly)
{
	ts->EnableMacros(false);
	ts->ConsumeToken();

	const auto &nt = ts->PeekToken();
	ts->EnableMacros(true);

	bool processIfs = !conditionalOnly;

	enum
	{
		IFT_ENDIF,
		IFT_IF,
		IFT_ELSE,
		IFT_ELSEIF
	};

	Int ifType = -1;

	if (nt.type == TOK_KEY_ENDIF)
	{
		processIfs = processIfs || !conditionalSkipCounter;
		ts->ConsumeToken();
		conditionalSkipCounter -= !processIfs;
		return processIfs ? EndIfMacro() : true;
	}
	else if (nt.type == TOK_KEY_IF)
	{
		ts->ConsumeToken();
		ifType = IFT_IF;
	}
	else if (nt.type == TOK_KEY_ELSE)
	{
		ts->ConsumeToken();
		ifType = IFT_ELSE;

		if (conditionalStack.IsEmpty())
			return ExpectPrev(false, "unexpected conditional else");

		if (ts->PeekToken().type == TOK_KEY_IF)
		{
			ts->ConsumeToken();
			ifType = IFT_ELSEIF;
		}
	}

	if (!conditionalSkipCounter)
		processIfs = true;

	if (conditionalOnly && ifType == IFT_IF)
	{
			++conditionalSkipCounter;
			processIfs = false;
	}

	if (ifType == IFT_ELSE)
	{
		if (processIfs)
		{
			if (conditionalStack.IsEmpty())
				return ExpectPrev(false, "conditional stack is empty");

			auto &csb = conditionalStack.Back();

			if (csb & CSF_GOT_ELSE)
				return ExpectPrev(false, "else after else");

			csb |= CSF_ACTIVE | CSF_GOT_ELSE;

			if (csb & CSF_IF_TAKEN)
				csb &= ~CSF_ACTIVE;
			else
				csb |= CSF_IF_TAKEN;

			return EnterIfMacro((csb & CSF_ACTIVE) != 0, depth+1, true);
		}

		return true;
	}

	if (ifType == IFT_IF || ifType == IFT_ELSEIF)
	{
		if (!processIfs)
			return true;

		// macro if...
		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBR, "expected `('"));

		auto *expr = ParseExpression(depth);
		LETHE_RET_FALSE(expr);

		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)'"));

		auto eloc = expr->location;

		AstNode tmpn(AST_NONE, TokenLocation());
		tmpn.Add(expr);

		if (!tempProgram)
			tempProgram = new CompiledProgram;

		while (tmpn.FoldConst(*tempProgram));

		auto ifres = tmpn.nodes[0]->ToBoolConstant(*tempProgram);

		// note: we treat macro if something as false, just like the C preprocessor
		if (ifres < 0)
			ifres = 0;

		if (ifType == IFT_ELSEIF)
		{
			auto &csb = conditionalStack.Back();

			if (csb & CSF_GOT_ELSE)
				return ExpectPrev(false, "else after else");

			csb |= CSF_ACTIVE * ifres;

			if (csb & CSF_IF_TAKEN)
				csb &= ~CSF_ACTIVE;
			else
				csb |= CSF_IF_TAKEN * ifres;

			ifres = (csb & CSF_ACTIVE) != 0;
		}

		return EnterIfMacro(ifres > 0, depth+1, ifType == IFT_ELSEIF);
	}

	ts->EnableMacros(false);
	auto res = ParseMacroInternal(conditionalOnly);
	ts->EnableMacros(true);
	return res;
}

bool Compiler::ParseMacroArgs(Array<Token> &nargs)
{
	nargs.Clear();

	bool lastComma = false;

	for (;;)
	{
		const auto &t = ts->GetToken();

		if (t.type == TOK_RBR)
		{
			LETHE_RET_FALSE(ExpectPrev(!lastComma, "expected argument"));
			break;
		}

		lastComma = false;

		if (t.type == TOK_IDENT)
			nargs.Add(t);
		else if (t.type == TOK_ELLIPSIS)
		{
			nargs.Add(t);
			return ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)'");
		}

		if (ts->PeekToken().type == TOK_COMMA)
		{
			ts->ConsumeToken();
			lastComma = true;
		}
	}

	if (nargs.IsEmpty())
	{
		// FIXME: this is hacky but we have to distinguish here
		Token tmp;
		tmp.type = TOK_EOF;
		nargs.Add(tmp);
	}

	return true;
}

bool Compiler::ParseMacroInternal(bool conditionalOnly)
{
	const auto &ntok = ts->GetToken();
	auto ntokLoc = ntok.location;
	LETHE_RET_FALSE(ExpectPrev(ntok.type == TOK_IDENT, "expected identifier"));

	const auto &tname = AddString(ntok.text);

	Array<Token> args;

	if (ts->PeekToken().type == TOK_LBR)
	{
		ts->ConsumeToken();
		LETHE_RET_FALSE(ParseMacroArgs(args));
	}

	bool simple = ts->PeekToken().type == TOK_EQ;

	if (simple)
		ts->ConsumeToken();

	Array<Token> tokens;

	for (;;)
	{
		const auto &tok = ts->GetToken();

		if (tok.type == (simple ? TOK_SEMICOLON : TOK_KEY_ENDMACRO) || tok.type == TOK_EOF || tok.type == TOK_INVALID)
			break;

		if (!conditionalOnly)
			tokens.Add(tok);
	}

	if (conditionalOnly)
		return true;

	if (!ts->AddSwapSimpleMacro(tname, args, tokens))
	{
		ErrorLoc(String::Printf("illegal macro redefinition: `%s'", tname.Ansi()), ntokLoc);
		return false;
	}

	return true;
}

AstNode *Compiler::ParseStaticAssert(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	const auto &tok = ts->GetToken();

	UniquePtr<AstNode> res = NewAstNode<AstStaticAssert>(tok.location);

	LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBR, "expected `('"));

	UniquePtr<AstNode> expr = ParseAssignExpression(depth+1);
	LETHE_RET_FALSE(!expr.IsEmpty());

	res->Add(expr.Detach());

	if (ts->PeekToken().type == TOK_COMMA)
	{
		ts->ConsumeToken();
		const auto &msgtok = ts->GetToken();
		LETHE_RET_FALSE(ExpectPrev(msgtok.type == TOK_STRING, "expected a string literal"));
		res->Add(NewAstText<AstTextConstant>(msgtok.text, AST_CONST_STRING, msgtok.location));
	}

	LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)'"));

	return res.Detach();
}

AstNode *Compiler::ParseProgram(Int depth, const String &nfilename)
{
	classOpen = 0;

	onCompile(nfilename);
	currentScope = globalScope;

	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = NewAstNode<AstProgram>(ts->PeekToken().location);
	AstNode *cur = res.Get();
	currentProgram = cur;

	// using namespace stack...
	Array<NamespaceStack> namespaces;

	for (;;)
	{
		ULong qualifiers = 0;
		const Token &t = ts->PeekToken();

		switch(t.type)
		{
		case TOK_SEMICOLON:
			ts->ConsumeToken();
			break;

		case TOK_SHARP:
		{
			auto tokLine = t.location.line;
			ts->ConsumeToken();
			LETHE_RET_FALSE(ParseDirective(tokLine));
			break;
		}

		case TOK_KEY_IMPORT:
		{
			ts->ConsumeToken();
			const auto &nt = ts->GetToken();
			LETHE_RET_FALSE(ExpectPrev(nt.type == TOK_STRING || nt.type == TOK_NAME, "expected string or name literal"));

			Path p = nfilename;
			p.Append("..");
			p.Append(nt.text);

			String imp = p;

			UniquePtr<AstText> impNode = NewAstText<AstImport>(imp.Ansi(), nt.location);
			impNode->flags |= AST_F_RESOLVED;

			TokenLocation targetLoc;
			targetLoc.column = 1;
			targetLoc.line = 1;
			targetLoc.file = impNode->text;
			UniquePtr<AstNode> targNode = NewAstNode<AstNode>(AST_NONE, targetLoc);
			targNode->flags |= AST_F_RESOLVED;

			impNode->nodes.Add(targNode.Detach());

			cur->Add(impNode.Detach());

			if (imported.FindIndex(imp) >= 0)
				continue;

			imp = AddString(imp.Ansi());
			imported.Add(imp);
			import.Add(imp);
		}
		break;

		case TOK_KEY_NAMESPACE:
		{
			ts->ConsumeToken();

			Int count = 0;

			for (;;)
			{
				count++;
				const Token &nt = ts->GetToken();
				LETHE_RET_FALSE(ExpectPrev(nt.type == TOK_IDENT, "expected identifier"));

				UniquePtr<AstNode> nspace = NewAstText<AstNamespace>(nt.text, t.location);
				const String &sname = static_cast< const AstText * >(nspace.Get())->text;
				NamedScope *scp = FindAddNamedScope(sname);
				LETHE_RET_FALSE(scp);
				scp->type = NSCOPE_NAMESPACE;
				scp->node = nspace.Get();
				scp->name = sname;
				nspace->scopeRef = scp;

				AstNode *tmp = nspace.Detach();
				cur->Add(tmp);

				NamespaceStack entry;
				entry.node = cur;
				entry.scope = currentScope;
				entry.pop = 1;
				namespaces.Add(entry);
				cur = tmp;
				currentScope = scp;

				const Token &nt2 = ts->PeekToken();

				if (nt2.type != TOK_DOUBLE_COLON)
					break;

				ts->ConsumeToken();
			}

			namespaces.Back().pop = count;

			if (ts->PeekToken().type == TOK_SEMICOLON)
			{
				ts->ConsumeToken();
				ts->AppendEof(Token(TOK_RBLOCK));
				ts->BeginMacroScope();
			}
			else
				LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));
		}
		break;

		case TOK_KEY_TYPEDEF:
		case TOK_KEY_USING:
		{
			AstNode *tdef = t.type == TOK_KEY_TYPEDEF ? ParseTypeDef(depth+1) : ParseUsing(depth+1);
			LETHE_RET_FALSE(tdef);
			cur->Add(tdef);
		}
		break;

		case TOK_KEY_STATIC_ASSERT:
		{
			auto *sa = ParseStaticAssert(depth+1);
			LETHE_RET_FALSE(sa);
			cur->Add(sa);
		}
		break;

		case TOK_KEY_ASSERT:
		case TOK_KEY_FORMAT:
		case TOK_KEY_INTRINSIC:
		case TOK_KEY_CONST:
		case TOK_KEY_CONSTEXPR:
		case TOK_KEY_RAW:
		case TOK_KEY_WEAK:
		case TOK_KEY_STATIC:
		case TOK_KEY_NATIVE:
		case TOK_KEY_NOCOPY:
		case TOK_KEY_NOTEMP:
		case TOK_KEY_NOBOUNDS:
		case TOK_KEY_NOINIT:
		case TOK_KEY_NONTRIVIAL:
		case TOK_KEY_NODISCARD:
		case TOK_KEY_MAYBE_UNUSED:
		case TOK_KEY_TRANSIENT:
		case TOK_KEY_FINAL:
		case TOK_KEY_INLINE:
		case TOK_KEY_LATENT:
		case TOK_KEY_STATE:
		case TOK_KEY_STATEBREAK:
		case TOK_KEY_PUBLIC:
		case TOK_KEY_PROTECTED:
		case TOK_KEY_PRIVATE:
		case TOK_KEY_OVERRIDE:
		case TOK_KEY_EDITABLE:
		case TOK_KEY_PLACEABLE:
		case TOK_KEY_THREAD_UNSAFE:
		case TOK_KEY_THREAD_CALL:
			// qualifiers...
			qualifiers |= ParseQualifiers();
			// fall through
		case TOK_KEY_TYPE_VOID:
		case TOK_KEY_TYPE_BOOL:
		case TOK_KEY_TYPE_BYTE:
		case TOK_KEY_TYPE_SBYTE:
		case TOK_KEY_TYPE_SHORT:
		case TOK_KEY_TYPE_USHORT:
		case TOK_KEY_TYPE_CHAR:
		case TOK_KEY_TYPE_INT:
		case TOK_KEY_TYPE_UINT:
		case TOK_KEY_TYPE_LONG:
		case TOK_KEY_TYPE_ULONG:
		case TOK_KEY_TYPE_FLOAT:
		case TOK_KEY_TYPE_DOUBLE:
		case TOK_KEY_TYPE_NAME:
		case TOK_KEY_TYPE_STRING:
		case TOK_KEY_AUTO:
		case TOK_KEY_STRUCT:
		case TOK_KEY_CLASS:
		case TOK_KEY_ENUM:
		case TOK_IDENT:
		case TOK_DOUBLE_COLON:
		{
			UniquePtr<AstNode> sub = ParseQualifiedDecl(qualifiers, depth+1);
			LETHE_RET_FALSE(sub);
			cur->Add(sub.Detach());
		}
		break;

		case TOK_EOF:
			LETHE_RET_FALSE(Expect(namespaces.IsEmpty(), "expected `}` to close namespace"));
			return res.Detach();

		case TOK_LARR:
			attributes = ParseAttributes();
			LETHE_RET_FALSE(attributes);
			break;

		case TOK_RBLOCK:
			if (!namespaces.IsEmpty())
			{
				ts->ConsumeToken();
				const NamespaceStack &entry = namespaces.Back();
				Int count = entry.pop;

				while (count-- > 0)
				{
					const NamespaceStack &entry2 = namespaces.Back();
					currentScope = entry2.scope;
					cur = entry2.node;
					namespaces.Pop();
				}

				break;
			}
			// fall through
		default:
			LETHE_RET_FALSE(Expect(false, "unexpected token"));
		}
	}
}

AstNode *Compiler::Compile(Stream &s, const String &nfilename)
{
	return CompileInternal(false, s, nfilename, nullptr);
}

AstNode *Compiler::CompileBuffered(Stream &s, const String &nfilename, Double *ioTime)
{
	auto *res = CompileInternal(true, s, nfilename, ioTime);
	tempStream.Clear();
	return res;
}

AstNode *Compiler::CompileInternal(bool nbuffered, Stream &s, const String &nfilename, Double *ioTime)
{
	LETHE_RET_FALSE(nbuffered ? OpenBuffered(s, nfilename, ioTime) : Open(s, nfilename));

	Path p = nfilename;
	imported.Add(AddString(p.Get().Ansi()));
	UniquePtr<AstNode> res = ParseProgram(0, nfilename);
	LETHE_RET_FALSE(res);

	while (!import.IsEmpty())
	{
		String imp = import.Back();
		import.Pop();

		VfsFile f(imp);
		BufferedStream bs;

		if (!nbuffered)
			bs.SetStream(f);

		LETHE_RET_FALSE(nbuffered ? OpenBuffered(f, imp, ioTime) : Open(bs, imp));

		UniquePtr<AstNode> tmp = ParseProgram(0, imp);
		LETHE_RET_FALSE(tmp);

		if (progList.IsEmpty())
		{
			TokenLocation loc;
			loc.line = loc.column = 0;
			progList = NewAstNode<AstNode>(AST_PROGRAM_LIST, loc);
		}

		progList->Add(tmp.Detach());
	}

	return res.Detach();
}

bool Compiler::CompileMacroExpansion(Stream &s, const String &nfilename)
{
	return CompileMacroExpansionInternal(false, s, nfilename, nullptr);
}

bool Compiler::CompileMacroExpansionBuffered(Stream &s, const String &nfilename, Double *ioTime)
{
	auto res = CompileMacroExpansionInternal(true, s, nfilename, ioTime);
	tempStream.Clear();
	return res;
}

bool Compiler::MacroExpandProgram(const String &nfilename)
{
	Token tok;

	for (;;)
	{
		auto tt = lex->GetToken(tok);

		// note: we should probably make sure import can be only called from root or subnamespaces, but we really don't care
		if (tt == TOK_KEY_IMPORT)
		{
			ts->ConsumeToken();
			const auto &nt = ts->GetToken();
			LETHE_RET_FALSE(ExpectPrev(nt.type == TOK_STRING || nt.type == TOK_NAME, "expected string or name literal"));

			Path p = nfilename;
			p.Append("..");
			p.Append(nt.text);

			String imp = p;

			if (imported.FindIndex(imp) >= 0)
				continue;

			imp = AddString(imp.Ansi());
			imported.Add(imp);
			import.Add(imp);
		}

		if (tt == TOK_EOF)
			break;

		if (tt == TOK_INVALID)
			return Expect(false, "invalid token");
	}

	return true;
}

bool Compiler::CompileMacroExpansionInternal(bool nbuffered, Stream &s, const String &nfilename, Double *ioTime)
{
	LETHE_RET_FALSE(nbuffered ? OpenBuffered(s, nfilename, ioTime) : Open(s, nfilename));

	Path p = nfilename;
	imported.Add(AddString(p.Get().Ansi()));

	LETHE_RET_FALSE(MacroExpandProgram(nfilename));

	while (!import.IsEmpty())
	{
		String imp = import.Back();
		import.Pop();

		VfsFile f(imp);
		BufferedStream bs;

		if (!nbuffered)
			bs.SetStream(f);

		LETHE_RET_FALSE(nbuffered ? OpenBuffered(f, imp, ioTime) : Open(bs, imp));

		LETHE_RET_FALSE(MacroExpandProgram(imp));
	}

	return true;
}

bool Compiler::Merge(Compiler &c)
{
	auto *plist = c.progList.Get();
	LETHE_RET_FALSE(plist);

	HashMap<NamedScope *, NamedScope *> scopeRemap;

	if (progList.IsEmpty())
	{
		TokenLocation loc;
		loc.line = loc.column = 0;
		progList = NewAstNode<AstNode>(AST_PROGRAM_LIST, loc);
	}

	for (auto *it : plist->nodes)
	{
		it->parent = nullptr;
		progList->Add(it);

		if (it->scopeRef == c.globalScope)
			it->scopeRef = globalScope;
	}

	// merge scopes, sigh...
	LETHE_ASSERT(c.globalScope);
	LETHE_RET_FALSE(globalScope->Merge(*c.globalScope, c, scopeRemap));

	AstIterator ait(plist);
	AstNode *n;

	while ((n = ait.Next()) != nullptr)
	{
		auto sidx = scopeRemap.FindIndex(n->scopeRef);

		if (sidx >= 0)
			n->scopeRef = scopeRemap.GetValue(sidx);
	}

	plist->nodes.Clear();

	return true;
}

bool Compiler::AddCompiledProgram(AstNode *node)
{
	LETHE_RET_FALSE(node);

	if (progList.IsEmpty())
	{
		TokenLocation loc;
		loc.line = loc.column = 0;
		progList = NewAstNode<AstNode>(AST_PROGRAM_LIST, loc);
	}

	progList->Add(node);
	return true;
}

void Compiler::InjectScopes(ErrorHandler &eh)
{
	eh.stringScope = stringScope;
	eh.arrayRefScope = arrayRefScope;
	eh.arrayScope = arrayScope;
	eh.dynamicArrayScope = dynamicArrayScope;
}

bool Compiler::ReplaceClasses(ErrorHandler &eh)
{
	HashMap<AstNode *, AstNode *> remap;

	static const char baseSuffix[] = "/base";

	for (auto &&it : replaceClasses)
	{
		if (it->type != AST_STRUCT && it->type != AST_CLASS)
			continue;

		auto *node = AstStaticCast<AstTypeStruct *>(it);

		if (!IsValidArrayIndex(AstTypeStruct::IDX_BASE, node->nodes.GetSize()))
			return eh.Error(node, "replace_class needs base");

		// base is either AST_IDENT or AST_OP_SCOPE_RES
		auto *base = node->nodes[AstTypeStruct::IDX_BASE];

		if (base->type != AST_BASE || base->nodes.IsEmpty())
			return eh.Error(node, "replace_class needs base");

		base = base->nodes[0];

		AstNode *ident = nullptr;

		if (base->type == AST_IDENT)
		{
			if (base->Resolve(eh) != AstNode::RES_OK)
				return eh.Error(base, "failed to resolve base");

			ident = base->GetResolveTarget();
		}
		else if (base->type == AST_OP_SCOPE_RES)
		{
			auto *sres = AstStaticCast<AstScopeResOp *>(base);

			if (sres->Resolve(eh) != AstNode::RES_OK)
				return eh.Error(sres, "failed to resolve base");

			ident = sres->GetResolveTarget();
		}

		if (!ident || (ident->type != AST_STRUCT && ident->type != AST_CLASS))
			return eh.Error(base, "couldn't resolve ident");


		// now the problem here is, IF we do this, we're fucked? => nope, nope, nope
		auto *primaryBase = AstStaticCast<AstTypeStruct *>(ident);

		if (primaryBase->qualifiers & (AST_Q_NATIVE | AST_Q_INTRINSIC))
			return eh.Error(base, "cannot replace native/intrinsic struct/class");

		// we need to iterate nested classes here and replace base if same
		AstIterator ait(primaryBase);

		while (AstNode *n = ait.Next())
		{
			if (n->type != AST_CLASS || n == primaryBase)
				continue;

			if (AstTypeStruct::IDX_BASE >= n->nodes.GetSize())
				continue;

			auto *nbase = n->nodes[AstTypeStruct::IDX_BASE];

			if (nbase->type != AST_BASE)
				continue;

			auto *nbasetmp = nbase->nodes[0];

			if (nbasetmp->Resolve(eh) != AstNode::RES_OK)
				continue;
		}

		auto *txt = AstStaticCast<AstText *>(primaryBase->nodes[AstTypeStruct::IDX_NAME]);

		if (!StringRef(txt->text).EndsWith(baseSuffix))
		{
			// first time => inject using typedef
			txt->text += baseSuffix;
		}

		auto idx = remap.FindIndex(primaryBase);

		if (idx >= 0)
		{
			// chain!
			auto *chain = remap.GetValue(idx);
			// replace our basechain
			base->target = chain;
		}

		remap[primaryBase] = it;
	}

	// finally we need to inject the original nodes

	for (auto &&it : remap)
	{
		auto *txt = AstStaticCast<AstText *>(it.key->nodes[AstTypeStruct::IDX_NAME]);

		// complicated scope rename
		if (auto *sparent = it.key->scopeRef->parent)
		{
			auto itx = sparent->namedScopes.Find(it.key->scopeRef->name);

			if (itx != sparent->namedScopes.End())
			{
				auto old = itx->value;
				sparent->namedScopes.Erase(itx);
				sparent->namedScopes.Add(it.key->scopeRef->name + baseSuffix, old);
			}
		}

		it.key->scopeRef->name += baseSuffix;

		currentScope = it.key->scopeRef->parent;
		auto rname = txt->text;
		rname.Erase(rname.GetLength() - StringRef(baseSuffix).GetLength());
		auto *nscope = AddUniqueNamedScope(rname);
		nscope->base = it.value->scopeRef;
		nscope->type = nscope->base->type;

		if (!nscope)
			return eh.Error(txt, "failed to add new scope");

		auto *nclass = it.key->parent->Add(
			it.value->type == AST_STRUCT ?
				NewAstNode<AstTypeStruct>(it.value->location) :
				NewAstNode<AstTypeClass>(it.value->location)
		);
		nclass->scopeRef = nscope;
		nscope->node = nclass;

		auto *nname = nclass->Add(NewAstText<AstSymbol>(rname.Ansi(), it.value->location));
		nname->flags |= AST_F_RESOLVED | AST_F_SKIP_CGEN;
		nname->scopeRef = nscope->parent;

		auto *nbasetmp = nclass->Add(NewAstNode<AstNode>(AST_BASE, it.value->location));
		nbasetmp->flags |= AST_F_RESOLVED;
		nbasetmp->scopeRef = nscope->parent;

		auto *nbase = nbasetmp->Add(NewAstText<AstSymbol>("//dummy", it.value->location));
		nbase->scopeRef = nscope->parent;
		nbase->flags |= AST_F_RESOLVED | AST_F_SKIP_CGEN;
		nbase->target = it.value;

		currentScope = nullptr;
	}

	return true;
}

bool Compiler::Resolve(bool ignoreErrors)
{
	LETHE_RET_FALSE(progList);

	ErrorHandler eh;
	InjectScopes(eh);

	if (!ignoreErrors)
	{
		eh.onError = onError;
		eh.onWarning = onWarning;
	}

	LETHE_RET_FALSE(ReplaceClasses(eh));

	LETHE_RET_FALSE(MoveExternalFunctions(eh));
	LETHE_RET_FALSE(InstantiateTemplates(eh));

	Int resolveSteps = 0;
	bool resolveError = false;

	for (;;)
	{
		++resolveSteps;

		auto rres = progList->Resolve(eh);

		if (rres == AstNode::RES_ERROR)
		{
			resolveError = true;
			break;
		}

		if (rres != AstNode::RES_MORE)
			break;
	}

	auto adlNodes = progList->GetAdlResolveNodes();

	bool changed = !resolveError;

	// hmm, someone changes adl nodes under the hood in Resolve!
	while (changed)
	{
		changed = false;
		++resolveSteps;

		for (Int i=0; i<adlNodes.GetSize(); i++)
		{
			const auto &adl = adlNodes[i];
			bool oldResolved = adl.node->IsResolved();

			if (oldResolved)
				continue;

			eh.tryADL = false;

			auto rres = adl.node->Resolve(eh);

			if (rres == AstNode::RES_ERROR)
			{
				resolveError = true;
				break;
			}

			eh.tryADL = true;
			rres = adl.node->Resolve(eh);

			if (rres == AstNode::RES_ERROR)
			{
				resolveError = true;
				break;
			}

			if (!oldResolved && adl.node->IsResolved())
				changed = true;
		}
	}

	bool progResolved = progList->IsResolved();

	AstNode *fn;

	bool doFinalResolveStep = false;

	if (!progResolved)
	{
		for (AstIterator it(progList); (fn = it.Next()) != nullptr;)
		{
			if (fn->type != AST_FUNC)
				continue;

			if (fn->qualifiers & (AST_Q_NATIVE | AST_Q_VIRTUAL | AST_Q_FUNC_REFERENCED | AST_Q_CTOR | AST_Q_DTOR))
				continue;

			if (fn->flags & AST_F_RESOLVED)
				continue;

			// if we're inside template, mark as resolved
			auto *templ = fn->FindTemplate();

			if (!templ)
				continue;

			fn->flags |= AST_F_RESOLVED | AST_F_SKIP_CGEN | AST_F_TYPE_GEN;

			AstNode *tnode;
			for (AstIterator it2(fn); (tnode = it2.Next()) != nullptr;)
				tnode->flags |= AST_F_RESOLVED | AST_F_SKIP_CGEN | AST_F_TYPE_GEN;

			doFinalResolveStep = true;
		}
	}

	eh.FlushLateDeleteNodes();

	if (doFinalResolveStep)
	{
		++resolveSteps;
		progList->Resolve(eh);
		progResolved = progList->IsResolved();
	}

	onResolve(resolveSteps);

	bool res = !resolveError && progResolved;

	if (!res && !ignoreErrors && !resolveError)
	{
		// walk AST and dump errors
		const AstNode *n;

		for (AstConstIterator ci(progList); (n = ci.Next()) != nullptr;)
		{
			if (n->flags & AST_F_RESOLVED)
				continue;

			if (!n->nodes.IsEmpty())
				continue;

			const auto tname = n->FindTemplateName();

			String tmp = tname.IsEmpty() ?
				String::Printf("unresolved symbol [%s]", n->GetTextRepresentation().Ansi()) :
				String::Printf("unresolved symbol [%s] in `%s'", n->GetTextRepresentation().Ansi(), tname.Ansi());

			ErrorLoc(tmp.Ansi(), n->location);
		}
	}

	return res;
}

bool Compiler::CodeGen(CompiledProgram &p)
{
	InjectScopes(p);

	LETHE_RET_FALSE(progList);

	LETHE_RET_FALSE(progList->BeginCodegen(p));

	p.foldSizeof = false;
	while (progList->FoldConst(p));
	p.foldSizeof = true;
	while (progList->FoldConst(p));

	LETHE_RET_FALSE(progList->TypeGenDef(p));
	LETHE_RET_FALSE(progList->TypeGen(p));
	// generate ctors/dtors/assignment for composite types
	LETHE_RET_FALSE(progList->CodeGenComposite(p));
	LETHE_RET_FALSE(progList->CodeGenGlobalCtor(p));
	LETHE_RET_FALSE(progList->CodeGen(p));

	// (re)generate vtbls
	do
	{
		LETHE_RET_FALSE(progList->VtblGen(p));
	}
	while (!p.VtblOk());

	p.Optimize();
	p.FixupVtbl();
	return 1;
}

void Compiler::Clear()
{
	*this = Compiler();
}

AstNode *Compiler::AddNativeProp(const char *nname, AstNodeType ntype)
{
	TokenLocation loc;
	AstNode *n = NewAstText<AstSymbol>(nname, loc);
	n->type = ntype;
	n->scopeRef = nullScope;
	nativeNodes.Add(n);
	return n;
}

static LETHE_NOINLINE void Compiler_AddMemberToScope(NamedScope *scope, const char *name, AstNode *n)
{
	scope->members[name] = n;
}

void Compiler::InitNativeTypeScopes()
{
	// FIXME: this asks for tables!

	struct SimpleMethodTable
	{
		const char *member;
		const char *prop;
		ULong qualifiers;
	};

	TokenLocation loc;
	loc.column = loc.line = 0;
	nullNode = new AstNode(AST_NPROP, loc);

	nullScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));
	nullNode->scopeRef = nullScope;

	stringScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	AstNode *n;

	// table-based init to reduce code bloat a bit
	static const SimpleMethodTable simple_string[] =
	{
		{"length", "__strlen", AST_Q_CONST},
		{"trim", "__str_trim", 0},
		{"insert", "__str_insert", 0},
		{"find", "__str_find", AST_Q_CONST},
		{"starts_with", "__str_starts_with", AST_Q_CONST},
		{"ends_with", "__str_ends_with", AST_Q_CONST},
		{"replace", "__str_replace", 0},
		{"erase", "__str_erase", 0},
		{"slice", "__str_slice", AST_Q_CONST},
		{"split", "__str_split", AST_Q_CONST},
		{"toupper", "__str_toupper", 0},
		{"tolower", "__str_tolower", 0},
		{"scan_int", "__strscan_int", 0},
		{"scan_float", "__strscan_float", 0},
		{"scan_double", "__strscan_double", 0},
		{"scan_string", "__strscan_string", 0},
	};

	for (auto &&it : simple_string)
	{
		n = AddNativeProp(it.prop, AST_NPROP_METHOD);
		n->qualifiers |= it.qualifiers;
		Compiler_AddMemberToScope(stringScope, it.member, n);
	}

	// note: __da_reverse will also work for array refs!
	auto nreverse = AddNativeProp("__da_reverse", AST_NPROP_METHOD);
	nreverse->flags |= AST_F_PUSH_TYPE;

	arrayRefScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	n = AddNativeProp("size", AST_NPROP);
	n->qualifiers |= AST_Q_CONST;
	n->offset = sizeof(void *);

	AstNode *ncap = AddNativeProp("capacity", AST_NPROP);
	ncap->qualifiers |= AST_Q_CONST;
	ncap->offset = sizeof(void *) + sizeof(Int);

	Compiler_AddMemberToScope(arrayRefScope, "size", n);
	// aliases:
	Compiler_AddMemberToScope(arrayRefScope, "length", n);

	Compiler_AddMemberToScope(arrayRefScope, "reverse", nreverse);

	arrayScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	auto an = AddNativeProp("size", AST_NPROP);
	an->qualifiers |= AST_Q_CONST;
	an->offset = -1;

	Compiler_AddMemberToScope(arrayScope, "size", an);
	// aliases
	Compiler_AddMemberToScope(arrayScope, "length", an);

	dynamicArrayScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	Compiler_AddMemberToScope(dynamicArrayScope, "size", n);
	// aliases
	Compiler_AddMemberToScope(dynamicArrayScope, "length", n);

	Compiler_AddMemberToScope(dynamicArrayScope, "capacity", ncap);

	auto nresize = AddNativeProp("__da_resize", AST_NPROP_METHOD);
	nresize->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "resize", nresize);

	auto nreserve = AddNativeProp("__da_reserve", AST_NPROP_METHOD);
	nreserve->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "reserve", nreserve);

	auto nclear = AddNativeProp("__da_clear", AST_NPROP_METHOD);
	nclear->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "clear", nclear);

	auto nreset = AddNativeProp("__da_reset", AST_NPROP_METHOD);
	nreset->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "reset", nreset);

	auto npop = AddNativeProp("__da_pop", AST_NPROP_METHOD);
	npop->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "pop", npop);

	auto nshrink = AddNativeProp("__da_shrink", AST_NPROP_METHOD);
	nshrink->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "shrink", nshrink);

	auto nerase = AddNativeProp("__da_erase", AST_NPROP_METHOD);
	nerase->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "erase", nerase);

	auto neraseu = AddNativeProp("__da_erase_unordered", AST_NPROP_METHOD);
	neraseu->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "erase_unordered", neraseu);

	auto npred = AddNativeProp("__da_pred", AST_NPROP_METHOD);
	npred->qualifiers |= AST_Q_CONST;
	npred->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "pred", npred);

	auto nsucc = AddNativeProp("__da_succ", AST_NPROP_METHOD);
	nsucc->qualifiers |= AST_Q_CONST;
	nsucc->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "succ", nsucc);

	auto nbegin = AddNativeProp("__da_begin", AST_NPROP_METHOD);
	nbegin->qualifiers |= AST_Q_CONST;
	nbegin->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "begin", nbegin);

	Compiler_AddMemberToScope(dynamicArrayScope, "reverse", nreverse);

	// special propmethods:
	auto npush = AddNativeProp("__da_push", AST_NPROP_METHOD);
	npush->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "push", npush);
	// alias:
	Compiler_AddMemberToScope(dynamicArrayScope, "add", npush);
	Compiler_AddMemberToScope(dynamicArrayScope, "push_back", npush);

	auto npushu = AddNativeProp("__da_push_unique", AST_NPROP_METHOD);
	npushu->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "push_unique", npushu);
	// alias:
	Compiler_AddMemberToScope(dynamicArrayScope, "add_unique", npushu);
	Compiler_AddMemberToScope(dynamicArrayScope, "push_back_unique", npushu);

	auto ninsert = AddNativeProp("__da_insert", AST_NPROP_METHOD);
	ninsert->flags |= AST_F_PUSH_TYPE | AST_F_ARG2_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "insert", ninsert);

	auto nfind = AddNativeProp("__da_find", AST_NPROP_METHOD);
	nfind->qualifiers |= AST_Q_CONST;
	nfind->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "find", nfind);
	Compiler_AddMemberToScope(dynamicArrayScope, "indexof", nfind);
	Compiler_AddMemberToScope(dynamicArrayScope, "index_of", nfind);

	Compiler_AddMemberToScope(arrayRefScope, "find", nfind);
	Compiler_AddMemberToScope(arrayRefScope, "indexof", nfind);
	Compiler_AddMemberToScope(arrayRefScope, "index_of", nfind);

	auto nsort = AddNativeProp("__da_sort", AST_NPROP_METHOD);
	nsort->flags |= AST_F_PUSH_TYPE;
	Compiler_AddMemberToScope(dynamicArrayScope, "sort", nsort);
	Compiler_AddMemberToScope(arrayRefScope, "sort", nsort);

	auto nlower = AddNativeProp("__da_lower_bound", AST_NPROP_METHOD);
	nlower->qualifiers |= AST_Q_CONST;
	nlower->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "lower_bound", nlower);
	Compiler_AddMemberToScope(dynamicArrayScope, "lowerbound", nlower);
	Compiler_AddMemberToScope(arrayRefScope, "lower_bound", nlower);
	Compiler_AddMemberToScope(arrayRefScope, "lowerbound", nlower);

	auto nupper = AddNativeProp("__da_upper_bound", AST_NPROP_METHOD);
	nupper->qualifiers |= AST_Q_CONST;
	nupper->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "upper_bound", nupper);
	Compiler_AddMemberToScope(dynamicArrayScope, "upperbound", nupper);
	Compiler_AddMemberToScope(arrayRefScope, "upper_bound", nupper);
	Compiler_AddMemberToScope(arrayRefScope, "upperbound", nupper);

	auto nfsorted = AddNativeProp("__da_find_sorted", AST_NPROP_METHOD);
	nfsorted->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "find_sorted", nfsorted);
	Compiler_AddMemberToScope(arrayRefScope, "find_sorted", nfsorted);

	auto nisorted = AddNativeProp("__da_insert_sorted", AST_NPROP_METHOD);
	nisorted->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "insert_sorted", nisorted);

	auto nisortedu = AddNativeProp("__da_insert_sorted_unique", AST_NPROP_METHOD);
	nisortedu->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	Compiler_AddMemberToScope(dynamicArrayScope, "insert_sorted_unique", nisortedu);

	auto nslice = AddNativeProp("__da_slice", AST_NPROP_METHOD);
	nslice->qualifiers |= AST_Q_CONST;
	nslice->flags |= AST_F_PUSH_TYPE_SIZE | AST_F_RES_SLICE;
	Compiler_AddMemberToScope(dynamicArrayScope, "slice", nslice);
	Compiler_AddMemberToScope(arrayRefScope, "slice", nslice);
}

}
