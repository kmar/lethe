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
	, nofail(0)
	, templateAccum(0)
	, staticInitCounter(0)
	, pstaticInitCounter(pstaticInitCtr)
	, floatLitIsDouble(false)
{
	globalScope = new NamedScope(NSCOPE_GLOBAL);
}

Compiler::Compiler()
	: currentScope(0)
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
	if (currentScope->members.FindIndex(nname) >= 0 ||
			currentScope->namedScopes.FindIndex(nname) >= 0)
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
	if (currentScope->members.FindIndex(nname) >= 0)
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		Expect(0, tmp.Ansi());
		return 0;
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
	if (currentScope->IsLocal() && (nnode->type == AST_VAR_DECL || nnode->type == AST_ARG))
	{
		const auto *tmp = currentScope->parent;

		while (tmp)
		{
			auto *sym = tmp->FindSymbol(nname);

			if (sym && (sym->type == AST_VAR_DECL || sym->type == AST_ARG))
			{
				Path pth = sym->location.file;
				WarningLoc(
					String::Printf("declaration of %s shadows a previous variable at line %d in %s", nname.Ansi(), sym->location.line, pth.GetFilename().Ansi()),
					nnode->location, WARN_SHADOW);
				break;
			}

			tmp = tmp->parent;
		}
	}

	if ((isCtor && currentScope->ctorDefined) || currentScope->members.FindIndex(nname) >= 0 ||
			currentScope->namedScopes.FindIndex(nname) >= 0)
	{
		String tmp = String::Printf("illegal redefinition of `%s'", nname.Ansi());
		return ExpectLoc(0, tmp.Ansi(), nnode->location);
	}

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

	Attributes *res = new Attributes;

	Int count = 1;

	for (;;)
	{
		const auto &t = ts->GetToken();

		if (t.type == TOK_INVALID || t.type == TOK_EOF)
			LETHE_RET_FALSE(ExpectPrev(false, "unexpected end of attribute list"));

		if (t.type == TOK_RARR && !--count)
			break;

		count += t.type == TOK_LARR;

		res->tokens.Add(t);
	}

	return res;
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

AstNode *Compiler::ParseProgram(Int depth, const String &nfilename)
{
	classOpen = 0;

	onCompile(nfilename);
	currentScope = globalScope;

	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = NewAstNode<AstProgram>(ts->PeekToken().location);
	AstNode *cur = res.Get();

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
			}
			else
				LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));
		}
		break;

		case TOK_KEY_TYPEDEF:
		{
			UniquePtr<AstNode> tdef = ParseTypeDef(depth+1);
			LETHE_RET_FALSE(tdef);
			cur->Add(tdef.Detach());
		}
		break;

		case TOK_KEY_USING:
		{
			UniquePtr<AstNode> tdef = ParseUsing(depth+1);
			LETHE_RET_FALSE(tdef);
			cur->Add(tdef.Detach());
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
		case TOK_KEY_NOBOUNDS:
		case TOK_KEY_NOINIT:
		case TOK_KEY_NONTRIVIAL:
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
	LETHE_RET_FALSE(Open(s, nfilename));

	Path p = nfilename;
	imported.Add(AddString(p.Get().Ansi()));
	UniquePtr<AstNode> res = ParseProgram(0, nfilename);
	LETHE_RET_FALSE(res);

	while (!import.IsEmpty())
	{
		String imp = import.Back();
		import.Pop();

		VfsFile f(imp);
		BufferedStream bs(f);
		LETHE_RET_FALSE(Open(bs, imp));

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

AstNode *Compiler::CompileBuffered(Stream &s, const String &nfilename, Double *ioTime)
{
	auto res = CompileBufferedInternal(s, nfilename, ioTime);
	tempStream.Clear();
	return res;
}

AstNode *Compiler::CompileBufferedInternal(Stream &s, const String &nfilename, Double *ioTime)
{
	LETHE_RET_FALSE(OpenBuffered(s, nfilename, ioTime));

	Path p = nfilename;
	imported.Add(AddString(p.Get().Ansi()));
	UniquePtr<AstNode> res = ParseProgram(0, nfilename);
	LETHE_RET_FALSE(res);

	while (!import.IsEmpty())
	{
		String imp = import.Back();
		import.Pop();

		VfsFile f(imp);
		LETHE_RET_FALSE(OpenBuffered(f, imp, ioTime));

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

bool Compiler::Resolve(bool ignoreErrors)
{
	LETHE_RET_FALSE(progList);

	ErrorHandler eh;
	InjectScopes(eh);

	if (!ignoreErrors)
	{
		eh.onError = onError;
		eh.onWarning = onWarning;

		LETHE_RET_FALSE(InstantiateTemplates(eh));
	}

	Int resolveSteps = 0;
	bool resolveError = false;

	for (;;)
	{
		++resolveSteps;

		auto rres = progList->ResolveAsync(eh);

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

		for (auto &&adl : adlNodes)
		{
			bool oldResolved = adl.node->IsResolved();
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

	onResolve(resolveSteps);

	bool res = !resolveError && progList->IsResolved();

	if (!res && !ignoreErrors)
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

void Compiler::InitNativeTypeScopes()
{
	// FIXME: this asks for tables!

	TokenLocation loc;
	loc.column = loc.line = 0;
	nullNode = new AstNode(AST_NPROP, loc);

	nullScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));
	nullNode->scopeRef = nullScope;

	stringScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	AstNode *n;

	// TODO: table-based init to reduce code bloat
	n = AddNativeProp("__strlen", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;

	stringScope->members["length"] = n;

	n = AddNativeProp("__str_trim", AST_NPROP_METHOD);
	stringScope->members["trim"] = n;

	n = AddNativeProp("__str_insert", AST_NPROP_METHOD);
	stringScope->members["insert"] = n;

	n = AddNativeProp("__str_find", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;
	stringScope->members["find"] = n;

	n = AddNativeProp("__str_starts_with", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;
	stringScope->members["starts_with"] = n;

	n = AddNativeProp("__str_ends_with", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;
	stringScope->members["ends_with"] = n;

	n = AddNativeProp("__str_replace", AST_NPROP_METHOD);
	stringScope->members["replace"] = n;

	n = AddNativeProp("__str_erase", AST_NPROP_METHOD);
	stringScope->members["erase"] = n;

	n = AddNativeProp("__str_slice", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;
	stringScope->members["slice"] = n;

	n = AddNativeProp("__str_split", AST_NPROP_METHOD);
	n->qualifiers |= AST_Q_CONST;
	stringScope->members["split"] = n;

	n = AddNativeProp("__str_toupper", AST_NPROP_METHOD);
	stringScope->members["toupper"] = n;

	n = AddNativeProp("__str_tolower", AST_NPROP_METHOD);
	stringScope->members["tolower"] = n;

	n = AddNativeProp("__strscan_int", AST_NPROP_METHOD);
	stringScope->members["scan_int"] = n;

	n = AddNativeProp("__strscan_float", AST_NPROP_METHOD);
	stringScope->members["scan_float"] = n;

	n = AddNativeProp("__strscan_double", AST_NPROP_METHOD);
	stringScope->members["scan_double"] = n;

	n = AddNativeProp("__strscan_string", AST_NPROP_METHOD);
	stringScope->members["scan_string"] = n;

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

	arrayRefScope->members["size"] = n;
	// aliases:
	arrayRefScope->members["length"] = n;

	arrayRefScope->members["reverse"] = nreverse;

	arrayScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	auto an = AddNativeProp("size", AST_NPROP);
	an->qualifiers |= AST_Q_CONST;
	an->offset = -1;

	arrayScope->members["size"] = an;
	// aliases
	arrayScope->members["length"] = an;

	dynamicArrayScope = globalScope->Add(new NamedScope(NSCOPE_STRUCT));

	dynamicArrayScope->members["size"] = n;
	// aliases
	dynamicArrayScope->members["length"] = n;

	dynamicArrayScope->members["capacity"] = ncap;

	auto nresize = AddNativeProp("__da_resize", AST_NPROP_METHOD);
	nresize->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["resize"] = nresize;

	auto nreserve = AddNativeProp("__da_reserve", AST_NPROP_METHOD);
	nreserve->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["reserve"] = nreserve;

	auto nclear = AddNativeProp("__da_clear", AST_NPROP_METHOD);
	nclear->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["clear"] = nclear;

	auto nreset = AddNativeProp("__da_reset", AST_NPROP_METHOD);
	nreset->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["reset"] = nreset;

	auto npop = AddNativeProp("__da_pop", AST_NPROP_METHOD);
	npop->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["pop"] = npop;

	auto nshrink = AddNativeProp("__da_shrink", AST_NPROP_METHOD);
	nshrink->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["shrink"] = nshrink;

	auto nerase = AddNativeProp("__da_erase", AST_NPROP_METHOD);
	nerase->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["erase"] = nerase;

	auto neraseu = AddNativeProp("__da_erase_unordered", AST_NPROP_METHOD);
	neraseu->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["erase_unordered"] = neraseu;

	auto npred = AddNativeProp("__da_pred", AST_NPROP_METHOD);
	npred->qualifiers |= AST_Q_CONST;
	npred->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["pred"] = npred;

	auto nsucc = AddNativeProp("__da_succ", AST_NPROP_METHOD);
	nsucc->qualifiers |= AST_Q_CONST;
	nsucc->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["succ"] = nsucc;

	auto nbegin = AddNativeProp("__da_begin", AST_NPROP_METHOD);
	nbegin->qualifiers |= AST_Q_CONST;
	nbegin->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["begin"] = nbegin;

	dynamicArrayScope->members["reverse"] = nreverse;

	// special propmethods:
	auto npush = AddNativeProp("__da_push", AST_NPROP_METHOD);
	npush->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["push"] = npush;
	// alias:
	dynamicArrayScope->members["add"] = npush;
	dynamicArrayScope->members["push_back"] = npush;

	auto npushu = AddNativeProp("__da_push_unique", AST_NPROP_METHOD);
	npushu->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["push_unique"] = npushu;
	// alias:
	dynamicArrayScope->members["add_unique"] = npushu;
	dynamicArrayScope->members["push_back_unique"] = npushu;

	auto ninsert = AddNativeProp("__da_insert", AST_NPROP_METHOD);
	ninsert->flags |= AST_F_PUSH_TYPE | AST_F_ARG2_ELEM;
	dynamicArrayScope->members["insert"] = ninsert;

	auto nfind = AddNativeProp("__da_find", AST_NPROP_METHOD);
	nfind->qualifiers |= AST_Q_CONST;
	nfind->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["find"] = nfind;
	dynamicArrayScope->members["indexof"] = nfind;
	dynamicArrayScope->members["index_of"] = nfind;

	arrayRefScope->members["find"] = nfind;
	arrayRefScope->members["indexof"] = nfind;
	arrayRefScope->members["index_of"] = nfind;

	auto nsort = AddNativeProp("__da_sort", AST_NPROP_METHOD);
	nsort->flags |= AST_F_PUSH_TYPE;
	dynamicArrayScope->members["sort"] = nsort;
	arrayRefScope->members["sort"] = nsort;

	auto nlower = AddNativeProp("__da_lower_bound", AST_NPROP_METHOD);
	nlower->qualifiers |= AST_Q_CONST;
	nlower->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["lower_bound"] = nlower;
	dynamicArrayScope->members["lowerbound"] = nlower;
	arrayRefScope->members["lower_bound"] = nlower;
	arrayRefScope->members["lowerbound"] = nlower;

	auto nupper = AddNativeProp("__da_upper_bound", AST_NPROP_METHOD);
	nupper->qualifiers |= AST_Q_CONST;
	nupper->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["upper_bound"] = nupper;
	dynamicArrayScope->members["upperbound"] = nupper;
	arrayRefScope->members["upper_bound"] = nupper;
	arrayRefScope->members["upperbound"] = nupper;

	auto nfsorted = AddNativeProp("__da_find_sorted", AST_NPROP_METHOD);
	nfsorted->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["find_sorted"] = nfsorted;
	arrayRefScope->members["find_sorted"] = nfsorted;

	auto nisorted = AddNativeProp("__da_insert_sorted", AST_NPROP_METHOD);
	nisorted->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["insert_sorted"] = nisorted;

	auto nisortedu = AddNativeProp("__da_insert_sorted_unique", AST_NPROP_METHOD);
	nisortedu->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["insert_sorted_unique"] = nisortedu;

	auto npushheap = AddNativeProp("__da_push_heap", AST_NPROP_METHOD);
	npushheap->flags |= AST_F_PUSH_TYPE | AST_F_ARG1_ELEM;
	dynamicArrayScope->members["push_heap"] = npushheap;
	dynamicArrayScope->members["pushheap"] = npushheap;

	auto npopheap = AddNativeProp("__da_pop_heap", AST_NPROP_METHOD);
	npopheap->flags |= AST_F_PUSH_TYPE | AST_F_RES_ELEM;
	dynamicArrayScope->members["pop_heap"] = npopheap;
	dynamicArrayScope->members["popheap"] = npopheap;

	auto nslice = AddNativeProp("__da_slice", AST_NPROP_METHOD);
	nslice->qualifiers |= AST_Q_CONST;
	nslice->flags |= AST_F_PUSH_TYPE_SIZE | AST_F_RES_SLICE;
	dynamicArrayScope->members["slice"] = nslice;
	arrayRefScope->members["slice"] = nslice;
}

}
