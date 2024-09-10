#include "Compiler.h"

#include "AstIncludes.h"

namespace lethe
{

bool Compiler::MoveExternalFunctions(ErrorHandler &eh)
{
	AstIterator ait(progList);

	Array<AstNode *> externalFuncs;

	while (auto *node = ait.Next(AST_BLOCK))
	{
		if (node->type != AST_FUNC)
			continue;

		if (AstFunc::IDX_BODY >= node->nodes.GetSize())
			continue;

		if (node->nodes[AstFunc::IDX_NAME]->type != AST_OP_SCOPE_RES)
			continue;

		externalFuncs.Add(node);
	}

	for (auto *n : externalFuncs)
	{
		// try to find target
		const auto &parts = n->nodes[AstFunc::IDX_NAME]->nodes;

		if (parts.IsEmpty())
			continue;

		auto *cur = n;

		for (auto *it : parts)
		{
			LETHE_ASSERT(it->type == AST_IDENT);
			if (it->type != AST_IDENT)
				continue;

			auto *ntext = AstStaticCast<AstText *>(it);

			const NamedScope *nscope;
			auto *nsym = cur->scopeRef->FindSymbolFull(ntext->text, nscope);

			if (!nsym)
				break;

			cur = nsym;
		}

		if (cur && cur != n && cur->type == AST_FUNC)
		{
			// got func!
			LETHE_RET_FALSE(AstStaticCast<AstFunc *>(n)->MoveBody(eh, cur) != AstFunc::RES_ERROR);
		}
	}

	return true;
}

AstNode *Compiler::ParseFuncArgsDecl(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = NewAstNode<AstNode>(AST_ARG_LIST, ts->PeekToken().location);

	bool lastComma = 0;

	for (;;)
	{
		const Token &nt = ts->PeekToken();

		if (nt.type == TOK_KEY_TYPE_VOID)
		{
			ts->ConsumeToken();

			if (ts->PeekToken().type == TOK_RBR)
			{
				ts->ConsumeToken();
				break;
			}

			ts->UngetToken();
		}

		if (nt.type == TOK_ELLIPSIS)
		{
			// FIXME: not really constant but resolved by default
			auto *ellipsis = res->Add(NewAstNode<AstConstant>(AST_ARG_ELLIPSIS, nt.location));
			ts->ConsumeToken();

			// allow & suffix for special ref-ellipsis where we'll automatically box as arrayref of size 1
			if (ts->PeekToken().type == TOK_AND)
			{
				ts->ConsumeToken();
				ellipsis->qualifiers |= AST_Q_REFERENCE;
			}

			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));
			break;
		}

		if (nt.type == TOK_RBR)
		{
			LETHE_RET_FALSE(Expect(!lastComma, "expected argument"));
			ts->ConsumeToken();
			break;
		}

		UniquePtr<AstNode> argType = ParseType(depth+1);
		LETHE_RET_FALSE(argType);

		if (argType->type == AST_TYPE_VOID)
			LETHE_RET_FALSE(Expect(false, "invalid void type"));

		UniquePtr<AstNode> argName = ParseName(depth+1);
		LETHE_RET_FALSE(argName);
		argName->flags |= AST_F_RESOLVED;

		if (ts->PeekToken().type == TOK_LARR)
		{
			// support C-style array decl
			LETHE_RET_FALSE(Expect(!(argType->qualifiers & AST_Q_REFERENCE), "arrays of references not allowed"));
			argType = ParseArrayType(argType, depth+1);
			LETHE_RET_FALSE(argType);
		}

		// now if next token is `=`, parse init expr
		UniquePtr<AstNode> argInit;

		if (ts->PeekToken().type == TOK_EQ)
		{
			ts->ConsumeToken();

			// FIXME: really assign expr?!
			argInit = ts->PeekToken().type == TOK_LBLOCK ?
				ParseAnonStructLiteral(depth+1) : ParseAssignExpression(depth+1);

			LETHE_RET_FALSE(argInit);
		}

		if (argType->type == AST_TYPE_ARRAY)
		{
			// force reference for array types
			argType->qualifiers |= AST_Q_REFERENCE;
		}

		AstNode *arg = NewAstNode<AstArg>(argType->location);
		argType->qualifiers |= AST_Q_LOCAL_INT;
		arg->Add(argType.Detach());
		arg->Add(argName.Detach());

		if (argInit)
			arg->Add(argInit.Detach());

		res->Add(arg);

		TokenType tt = ts->PeekToken().type;
		LETHE_RET_FALSE(Expect(tt == TOK_COMMA || tt == TOK_RBR, "expected `,` or `)`"));

		lastComma = tt == TOK_COMMA;

		if (tt == TOK_COMMA)
			ts->ConsumeToken();
	}

	if (res->nodes.IsEmpty())
	{
		// empty arglist => resolved
		res->flags |= AST_F_RESOLVED;
	}

	return res.Detach();
}

AstNode *Compiler::ParseShorthandFunction(Int depth, AstNode *ntype, const String &fname)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	const auto &tok = ts->GetToken();
	// parse shorthand creating a fake block and return if needed

	NamedScopeGuard nsg(this, currentScope->Add(new NamedScope(NSCOPE_FUNCTION)));

	UniquePtr<AstNode> fbody = NewAstNode<AstFuncBody>(tok.location);

	currentScope->needExtraScope = false;
	currentScope->node = fbody;
	currentScope->name = fname;

	fbody->scopeRef = currentScope;

	if (ntype->type == AST_TYPE_VOID)
	{
		// just inject a statement
		auto *stmt = ParseStatement(depth+1);
		LETHE_RET_FALSE(stmt);
		fbody->Add(stmt);
	}
	else
	{
		// synthesize a return
		UniquePtr<AstNode> ret = NewAstNode<AstReturn>(fbody->location);

		LETHE_RET_FALSE(ParseReturn(depth+1, ret));

		fbody->Add(ret.Detach());
	}

	return fbody.Detach();
}

AstNode *Compiler::ParseFuncDecl(UniquePtr<AstNode> &ntype, UniquePtr<AstNode> &nname, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	if (currentScope->IsComposite())
		ntype->qualifiers |= structAccess;

	const NamedScope *oldScope = currentScope;

	// arg scope
	NamedScopeGuard nsg(this, currentScope->Add(new NamedScope(NSCOPE_ARGS)));
	currentScope->resultPtr = ntype.Get();

	ntype->scopeRef = currentScope;

	LETHE_ASSERT(ts->PeekToken().type == TOK_LBR);
	ts->ConsumeToken();
	// now parse arg list
	UniquePtr<AstNode> args = ParseFuncArgsDecl(depth+1);
	LETHE_RET_FALSE(args);
	// TODO: only allow ellipsis on non-void types

	auto fqualifiers = ParseQualifiers();

	// allow "modern-C++ style" auto func()->result
	if (ntype->type == AST_TYPE_AUTO && ts->PeekToken().type == TOK_C_DEREF)
	{
		ts->ConsumeToken();
		UniquePtr<AstNode> typeover = ParseType(depth+1);
		LETHE_RET_FALSE(typeover);
		typeover->qualifiers |= ntype->qualifiers;
		typeover->scopeRef = ntype->scopeRef;
		typeover.SwapWith(ntype);
		currentScope->resultPtr = ntype.Get();
	}

	// copy native to func
	// what also applies here: private/protected/public/static/native/final/virtual
	if (ntype->qualifiers & AST_Q_FUNC_MASK)
	{
		fqualifiers |= ntype->qualifiers & AST_Q_FUNC_MASK;
		ntype->qualifiers &= ~AST_Q_FUNC_MASK;
	}

	// now get ready to parse the most important one: function body
	// this is where all code resides

	UniquePtr<AstNode> fbody;

	// note: native func has no body
	bool isNative = (fqualifiers & AST_Q_NATIVE) != 0;

	bool isComposite = oldScope->IsComposite();

	if (!isNative)
	{
		bool skipBody = false;

		if (isComposite && ts->PeekToken().type == TOK_SEMICOLON)
		{
			ts->ConsumeToken();
			skipBody = true;
		}
		else
		{
			auto tt = ts->PeekToken().type;
			LETHE_RET_FALSE(Expect(tt == TOK_LBLOCK || tt == TOK_EQ_GT, "expected `{' or `=>'"));
		}

		// add args (members)
		for (Int i=0; i<args->nodes.GetSize(); i++)
		{
			AstNode *n = args->nodes[i];

			if (n->type != AST_ARG)
				continue;

			LETHE_ASSERT(n->nodes[1]->type == AST_IDENT);
			const String &argn = AstStaticCast<const AstText *>(n->nodes[1])->text;
			LETHE_RET_FALSE(AddScopeMember(argn, n));
		}

		if (!skipBody)
		{
			String fname;

			// we want a fully qualified function name for __func, also for state vars
			if (nname->type == AST_IDENT)
			{
				auto *txt = AstStaticCast<AstText *>(nname);
				fname = AddStringRef(txt->GetQTextSlow());
			}
			else if (nname->type == AST_OP_SCOPE_RES)
			{
				// need to handle this for outer funcs
				StringBuilder sb;

				// must handle namespaces here...
				auto *scp = currentScope;

				while (scp)
				{
					if (scp->type == NSCOPE_NAMESPACE && !scp->name.IsEmpty())
					{
						if (sb.GetLength())
							sb.Prepend("::");

						sb.Prepend(scp->name);
					}

					scp = scp->parent;
				}

				for (auto *it : nname->nodes)
				{
					if (it->type == AST_IDENT)
					{
						if (sb.GetLength())
							sb += "::";

						sb += static_cast<const AstText *>(it)->text;
					}
				}

				fname = sb.Get();
			}

			ts->SetFuncName(fname);

			if (ts->PeekToken().type == TOK_EQ_GT)
				fbody = ParseShorthandFunction(depth+1, ntype, fname);
			else
				fbody = ParseBlock(depth+1, true, false, (fqualifiers & AST_Q_STATE) != 0, &fname);

			ts->SetFuncName(String());

			LETHE_RET_FALSE(fbody);
		}
	}
	else
		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_SEMICOLON, "expected `;'"));

	AstNode *res = NewAstNode<AstFunc>(args->location);

	if (attributes)
	{
		for (auto &&it : attributes->tokens)
		{
			if (it.type == TOK_IDENT && StringRef(it.text) == "deprecated")
			{
				fqualifiers |= AST_Q_DEPRECATED;
				break;
			}
		}
	}

	Swap(AstStaticCast<AstFunc *>(res)->attributes, attributes);

	res->qualifiers |= fqualifiers;
	res->Add(ntype.Detach());
	res->Add(nname.Detach());
	res->Add(args.Detach());

	if (fbody)
	{
		fbody->scopeRef->node = res;
		res->Add(fbody.Detach());
	}

	return res;
}

bool Compiler::ValidateVirtualProp(const AstNode *pfun, bool isGetter)
{
	auto *fn = AstStaticCast<const AstFunc *>(pfun);
	auto *args = fn->nodes[AstFunc::IDX_ARGS];

	if (args->nodes.GetSize() != !isGetter)
	{
		ExpectPrev(false, "invalid argument count for virtual property get/set");
		return false;
	}

	return true;
}

AstNode *Compiler::ParseVarDecl(UniquePtr<AstNode> &ntype, UniquePtr<AstNode> &nname, Int depth,
								bool refFirstInit, bool initOnly)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	const bool isStateVar = (ntype->qualifiers & AST_Q_STATE) != 0;

	if (currentScope->IsLocal())
		ntype->qualifiers |= AST_Q_LOCAL_INT;

	if (currentScope->IsComposite())
		ntype->qualifiers |= structAccess;

	auto typeQualifiers = ntype->qualifiers & (AST_Q_STATIC | AST_Q_PRIVATE | AST_Q_PROTECTED);

	UniquePtr<AstNode> res = NewAstNode<AstVarDeclList>(ts->PeekToken().location);
	Swap(AstStaticCast<AstVarDeclList *>(res.Get())->attributes, attributes);

	ntype->flags |= AST_F_SKIP_CGEN;
	res->Add(ntype.Detach());
	Int count = 0;
	bool isCArray = 0;

	for (Int idx = 0;; idx++)
	{
		LETHE_RET_FALSE(Expect(nname->type == AST_IDENT, "var name must be an identifier"));

		const Token &nt = ts->PeekToken();

		if (count == 0 && nt.type == TOK_LARR)
		{
			LETHE_RET_FALSE(Expect(!(res->nodes[0]->qualifiers & AST_Q_REFERENCE), "arrays of references not allowed"));
			UniquePtr<AstNode> tmp = res->nodes[0];
			tmp->parent = 0;
			UniquePtr<AstNode> newtype = ParseArrayType(tmp, depth+1);
			tmp.Detach();

			if (newtype.IsEmpty())
				return 0;

			newtype->flags |= AST_F_SKIP_CGEN;

			isCArray = 1;
			res->nodes[0] = newtype.Detach();
			res->nodes[0]->parent = res.Get();
			continue;
		}

		UniquePtr<AstNode> init;

		auto nttype = nt.type;

		// note: force assignment for :=
		if (nttype == TOK_COLON_EQ)
			nttype = TOK_EQ;

		if (initOnly && !idx)
		{
			// note: TOK_COLON for range based for
			LETHE_RET_FALSE(Expect(nt.type == TOK_EQ || nt.type == TOK_COLON, "expected `='"));
		}

		if (nt.type == TOK_EQ)
		{
			ts->ConsumeToken();

			bool initializerList = false;

			if (ts->PeekToken().type == TOK_LBLOCK)
			{
				init = ParseInitializerList(depth+1);
				initializerList = true;
			}
			else
				init = ParseAssignExpression(depth+1);

			LETHE_RET_FALSE(init);

			// if type is single array ref, turn into static array
			auto &tnode = res->nodes[0];

			if (initializerList && tnode->type == AST_TYPE_ARRAY_REF)
			{
				auto tmp = NewAstNode<AstTypeArray>(tnode->location);
				tmp->qualifiers |= tnode->qualifiers;
				Swap(tmp->nodes, tnode->nodes);

				auto dimsnode = NewAstNode<AstConstInt>(tnode->location);
				dimsnode->num.i = init->nodes.GetSize();

				for (auto *it : tmp->nodes)
					it->parent = tmp;

				delete tnode;

				tmp->nodes.Add(dimsnode);

				tmp->scopeRef = arrayScope;

				tnode = tmp;
			}
		}

		// add var node
		UniquePtr<AstNode> vn = NewAstNode<AstVarDecl>(nname->location);
		// declaring var => resolved
		nname->flags |= AST_F_RESOLVED;

		const String &vname = AstStaticCast<const AstText *>(nname.Get())->text;

		if (!isStateVar)
			LETHE_RET_FALSE(AddScopeMember(vname, vn.Get()));

		if (refFirstInit && idx == 0 && init)
			vn->flags |= AST_F_REFERENCED;

		vn->qualifiers |= typeQualifiers;

		auto *varNameRef = nname.Get();

		vn->Add(nname.Detach());

		if (init)
			vn->Add(init.Detach());

		auto *vnptr = vn.Get();

		res->Add(vn.Detach());
		// now stop or continue parsing
		TokenType ntt = ts->PeekToken().type;

		// : { ... virtual_props ... } or : n (bitfields)
		// I decided to use extra colon so that I don't close doors to modern C++-style constructor calls, like int i{1};
		// also, ignored in local scope to avoid clashes with range based for loops
		if (ntt == TOK_COLON && !currentScope->IsLocal())
		{
			ts->ConsumeToken();

			if (ts->PeekToken().type == TOK_ULONG)
			{
				// bitfield!
				// [0] = type, [1+] = var decls
				if (res->nodes.GetSize() != 2)
				{
					ExpectPrev(false, "bitfields can only be defined for a single variable (parsing restriction)");
					return nullptr;
				}

				auto numTok = ts->GetToken();

				if (numTok.type != TOK_ULONG || numTok.number.l > 64)
					LETHE_RET_FALSE(ExpectPrev(false, "expected bit size or too large"));

				auto *tpe = res->nodes[0];

				if (tpe->qualifiers & AST_Q_STATIC)
					LETHE_RET_FALSE(ExpectPrev(false, "bitfield cannot be static"));

				tpe->qualifiers |= AST_Q_BITFIELD;
				tpe->num.i = res->nodes[1]->num.i = (Int)numTok.number.l;
				res->nodes[1]->qualifiers |= AST_Q_BITFIELD;
				res->nodes[1]->nodes[0]->qualifiers |= AST_Q_BITFIELD;

				if (ts->PeekToken().type == TOK_EQ)
				{
					ts->ConsumeToken();
					auto *iniExpr = ParseAssignExpression(depth+1);
					LETHE_RET_FALSE(iniExpr);
					res->nodes[1]->Add(iniExpr);
				}

				return res.Detach();
			}

			ntt = ts->PeekToken().type;

			if (ntt != TOK_LBLOCK)
			{
				Expect(false, "`{' expected to start a virtual property block");
				return nullptr;
			}

			// [0] = type, [1+] = var decls
			if (res->nodes.GetSize() != 2)
			{
				ExpectPrev(false, "virtual properties can only be defined for a single variable (parsing restriction)");
				return nullptr;
			}

			// virtual property
			if (vnptr->nodes.GetSize() > 1)
			{
				ExpectPrev(false, "virtual properties cannot be initialized");
				return nullptr;
			}

			const auto &varNameText = AstStaticCast<AstText *>(varNameRef)->text;

			ts->ConsumeToken();

			for (;;)
			{
				if (ts->PeekToken().type == TOK_RBLOCK)
				{
					ts->ConsumeToken();
					break;
				}

				if (ts->PeekToken().type == TOK_LARR)
					attributes = ParseAttributes();

				UniquePtr<AstNode>ftype = ParseType(depth+1);
				LETHE_RET_FALSE(ftype);
				UniquePtr<AstNode> fname = ParseName(depth+1);
				LETHE_RET_FALSE(fname);
				fname->flags |= AST_F_RESOLVED;
				auto *ftext = AstStaticCast<AstText *>(fname);
				StringBuilder sb;

				const bool isGetter = ftext->text == "get";

				if (ftext->text != "get" && ftext->text != "set")
				{
					ExpectPrev(false, "virtual property functions must be `set' or `get'");
					return nullptr;
				}

				if (ts->PeekToken().type != TOK_LBR)
				{
					Expect(false, "expected `('");
					return nullptr;
				}

				// build __get_varname etc.
				sb.AppendFormat("__%s_%s", ftext->text.Ansi(), varNameText.Ansi());
				ftext->text = AddStringRef(sb.Get());
				UniquePtr<AstNode> pfun = ParseFuncDecl(ftype, fname, depth+1);
				LETHE_RET_FALSE(pfun);

				// FIXME: a hack to always force virtual prop getters/setters to be referenced
				pfun->qualifiers |= AST_Q_FUNC_REFERENCED;

				LETHE_RET_FALSE(ValidateVirtualProp(pfun, isGetter));

				if (currentScope->IsComposite() && !(pfun->qualifiers & AST_Q_STATIC))
				{
					pfun->qualifiers |= AST_Q_METHOD;

					if (currentScope->type == NSCOPE_CLASS)
					{
						// mark non-final class methods as virtual
						if (!(pfun->qualifiers & AST_Q_FINAL))
							pfun->qualifiers |= AST_Q_VIRTUAL;
					}
				}

				AddScopeMember(ftext->text, pfun);

				auto *tnode = currentScope->node;

				// assume if no current scope node, assume global
				if (!tnode)
					tnode = currentProgram;

				tnode->Add(pfun.Detach());
			}

			// set qualifier flag
			vnptr->qualifiers |= AST_Q_PROPERTY;
			// mark type with property
			res->nodes[0]->qualifiers |= AST_Q_PROPERTY;

			return res.Detach();
		}

		if (ntt != TOK_COMMA)
		{
			if (!isStateVar)
				return res.Detach();

			// handle state vars here...
			auto *fscope = res->scopeRef->FindFunctionScope();
			const NamedScope *clsscope = nullptr;

			if (fscope && fscope->parent && fscope->parent->type == NSCOPE_ARGS)
			{
				clsscope = fscope->parent->parent;

				// don't allow global scope here
				if (clsscope && (!clsscope->IsComposite() || !clsscope->node))
					clsscope = nullptr;

				while (clsscope)
				{
					if (!(clsscope->node->qualifiers & AST_Q_STATE))
						break;

					clsscope = clsscope->parent;

					if (clsscope && (!clsscope->IsComposite() || !clsscope->node))
						clsscope = nullptr;
				}
			}

			if (!clsscope)
				return res.Detach();

			// inject using scopes

			StringBuilder sb;

			auto *nclsscope = const_cast<NamedScope *>(clsscope);

			// hack: create a virtual block to be processed properly
			UniquePtr<AstNode> typedefRoot = NewAstNode<AstNode>(AST_BLOCK, res->location);
			typedefRoot->flags |= AST_F_RESOLVED;

			// give a sane error message here rather than failing later with a cryptic resolve error
			// we don't support this and won't - for simplicity
			LETHE_RET_FALSE(ExpectPrev(res->nodes[0]->type != AST_TYPE_AUTO, "auto not allowed for state vars"));

			for (Int i=1; i<res->nodes.GetSize(); i++)
			{
				auto *vd = AstStaticCast<AstVarDecl *>(res->nodes[i]);

				// initializer must be injected as assignment for state vars to behave a bit more like locals
				auto *ini = vd->nodes.GetSize() > 1 ? vd->nodes[1] : nullptr;
				vd->qualifiers &= ~AST_Q_LOCAL_INT;
				auto *varname = AstStaticCast<AstText *>(vd->nodes[0]);
				sb.Clear();
				sb.Format("%s$%s", varname->text.Ansi(), fscope->name.Ansi());

				auto newname = AddStringRef(sb.Get());

				UniquePtr<AstNode> nres = NewAstNode<AstTypeDef>(res->location);
				nres->qualifiers |= AST_Q_SYMBOL_ALIAS;
				LETHE_RET_FALSE(AddScopeMember(varname->text, nres.Get()));

				nres->Add(NewAstText<AstSymbol>(newname.Ansi(), varname->location));

				auto *tdefName = NewAstText<AstText>(varname->text.Ansi(), AST_IDENT,  varname->location);
				tdefName->flags |= AST_F_RESOLVED;
				nres->Add(tdefName);

				nres->flags |= AST_F_SKIP_CGEN;

				auto *oscope = currentScope;
				currentScope = nclsscope;

				bool success = AddScopeMember(newname, vd);

				currentScope = oscope;

				LETHE_RET_FALSE(success);

				varname->text = newname;

				if (ini)
				{
					vd->nodes.Resize(1);
					// build ini expr
					auto *expr = NewAstNode<AstExpr>(ini->location);
					auto *asgn = NewAstNode<AstBinaryAssign>(ini->location);

					asgn->Add(NewAstText<AstSymbol>(newname.Ansi(), ini->location));
					ini->parent = nullptr;
					asgn->Add(ini);
					expr->Add(asgn);
					typedefRoot->Add(expr);
				}

				typedefRoot->Add(nres.Detach());
			}

			AstIterator ai(res.Get());

			while (auto *n = ai.Next())
			{
				if (n->scopeRef == currentScope)
					n->scopeRef = nclsscope;
			}

			nclsscope->node->Add(res.Detach());

			return typedefRoot.Detach();
		}

		LETHE_ASSERT(ntt == TOK_COMMA);

		ts->ConsumeToken();

		if (isCArray)
			LETHE_RET_FALSE(ExpectPrev(0, "cannot chain c-style array decl due to parsing restriction"));

		// parse name!
		nname = ParseName(depth+1);
		LETHE_RET_FALSE(nname);

		count++;
	}
}

AstNode *Compiler::ParseVarDeclOrExpr(Int depth, bool refFirstInit, bool initOnly)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	// FIXME: this is stupid, because it will allocate potentially unnecessary ast node
	// try var decl first (note: no local structs/enums, no local functions!)
	Int pos = ts->GetPosition();
	nofail++;
	UniquePtr<AstNode> ntype = ParseType(depth+1);
	UniquePtr<AstNode> nname;

	if (ntype)
	{
		if (ts->PeekToken().type == TOK_COLON_EQ)
		{
			Swap(ntype, nname);
			ntype = NewAstNode<AstTypeAuto>(nname->location);
			ntype->qualifiers = nname->qualifiers;
			nname->qualifiers = 0;
			--nofail;
			return ParseVarDecl(ntype, nname, depth + 1, refFirstInit, initOnly);
		}
		else
			nname = ParseName(depth+1);

		Int delta = ts->GetPosition() - pos;

		if (nname)
		{
			nofail--;

			// FIXME: only create a virtual scope if there is pending break/continue/return
			if (currentScope->needExtraScope)
			{
				ts->UngetToken(delta);
				// add virtual new scope
				AstNode *nres = ParseBlock(depth+1, 0, 1);
				ts->UngetToken(1);
				return nres;
			}

			nofail++;
			Int osize = currentScope->members.GetSize();
			AstNode *vres = ParseVarDecl(ntype, nname, depth + 1, refFirstInit, initOnly);

			if (vres)
			{
				nofail--;
				return vres;
			}

			// failing is problematic, we need to restore scope members here
			auto &smembers = currentScope->members;

			for (Int i = osize; i < smembers.GetSize(); i++)
			{
				smembers.Erase(smembers.Find(smembers.GetKey(i).key));
				i--;
			}

			delta = ts->GetPosition() - pos;
		}

		ts->UngetToken(delta);
	}
	else
		ts->UngetToken(ts->GetPosition() - pos);

	// it has to be an expression
	AstNode *expr = ParseExpression(depth+1);
	nofail--;

	if (expr)
		return expr;

	// if we get here, it's an error; so reparse and get proper error
	ts->UngetToken(ts->GetPosition() - pos);

	auto old = onError;

	struct ErrorInfo
	{
		String msg;
		TokenLocation loc;
	};

	Array<ErrorInfo> errors;

	onError = [&](const String &msg, const TokenLocation &loc)
	{
		errors.Add(ErrorInfo{msg, loc});
	};

	// get both errors. should actually use the one that goes further OR at least sort them by location position...
	ParseExpression(depth+1);
	ts->UngetToken(ts->GetPosition() - pos);

	ntype = ParseType(depth+1);

	if (ntype)
	{
		nname = ParseName(depth+1);
		if (nname)
			ParseVarDecl(ntype, nname, depth+1, refFirstInit, initOnly);
	}

	onError = old;

	// pick error that goes further and report that; may still be wrong but better than before
	auto sortLambda = [](const ErrorInfo &a, const ErrorInfo &b)->bool
	{
		if (a.loc.line != b.loc.line)
			return a.loc.line < b.loc.line;

		return a.loc.column < b.loc.column;
	};

	errors.Sort(sortLambda);

	if (!errors.IsEmpty())
		onError(errors.Back().msg, errors.Back().loc);

	return nullptr;
}

AstNode *Compiler::ParseFuncOrVarDecl(UniquePtr<AstNode> &ntype, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	// we expect ident now (or: qualified ident)

	bool isCtor = ts->PeekToken().type == TOK_LBR;
	bool isDtor = ts->PeekToken().type == TOK_NOT;

	if (isDtor)
		ts->ConsumeToken();
	else if (!isCtor && ts->PeekToken().type == TOK_IDENT)
	{
		// it can be a (dynamic) array, even static!
		LETHE_RET_FALSE(ParseArrayType(ntype, depth+1));
	}

	ntype->qualifiers |= ParseQualifiers(1);

	if (ts->PeekToken().type == TOK_NOT)
	{
		LETHE_RET_FALSE(Expect(!isDtor, "unexpected token"));
		ts->ConsumeToken();
		isDtor = 1;
	}

	if (isCtor)
	{
		auto qual = ntype->qualifiers;
		qual |= AST_Q_CTOR;
		ntype = NewAstNode<AstTypeVoid>(ntype->location);
		ts->UngetToken();
		ntype->qualifiers |= qual;
	}

	if (ts->PeekToken().type == TOK_COLON_EQ)
	{
		UniquePtr<AstNode> name;
		Swap(ntype, name);
		ntype = NewAstNode<AstTypeAuto>(name->location);
		ntype->qualifiers = name->qualifiers;
		name->qualifiers = 0;

		return ParseVarDecl(ntype, name, depth+1);
	}

	const char *isOperator = nullptr;

	bool gotOperator = ts->PeekToken().type == TOK_KEY_OPERATOR;

	if (gotOperator)
	{
		ts->ConsumeToken();
		// allowing operator overload definition: static mystruct +(mystruct a, mystruct b) inside struct
		switch(ts->PeekToken().type)
		{
		case TOK_EQ:
			// note: not really supported, but parsing
			isOperator = "=";
			break;

		case TOK_LARR:
			ts->ConsumeToken();

			if (ts->PeekToken().type == TOK_RARR)
				isOperator = "[]";

			break;

		case TOK_NOT:
			isOperator = "~";
			break;

		case TOK_LNOT:
			isOperator = "!";
			break;

		case TOK_INC:
			isOperator = "++";
			break;

		case TOK_DEC:
			isOperator = "--";
			break;

		case TOK_PLUS:
			isOperator = "+";
			break;

		case TOK_MINUS:
			isOperator = "-";
			break;

		case TOK_MUL:
			isOperator = "*";
			break;

		case TOK_DIV:
			isOperator = "/";
			break;

		case TOK_MOD:
			isOperator = "%";
			break;

		case TOK_AND:
			isOperator = "&";
			break;

		case TOK_OR:
			isOperator = "|";
			break;

		case TOK_XOR:
			isOperator = "^";
			break;

		case TOK_SHL:
			isOperator = "<<";
			break;

		case TOK_SHR:
			isOperator = ">>";
			break;

		case TOK_PLUS_EQ:
			isOperator = "+=";
			break;

		case TOK_MINUS_EQ:
			isOperator = "-=";
			break;

		case TOK_MUL_EQ:
			isOperator = "*=";
			break;

		case TOK_DIV_EQ:
			isOperator = "/=";
			break;

		case TOK_MOD_EQ:
			isOperator = "%=";
			break;

		case TOK_AND_EQ:
			isOperator = "&=";
			break;

		case TOK_OR_EQ:
			isOperator = "|=";
			break;

		case TOK_XOR_EQ:
			isOperator = "^=";
			break;

		case TOK_SHL_EQ:
			isOperator = "<<=";
			break;

		case TOK_SHR_EQ:
			isOperator = ">>=";
			break;

		case TOK_EQ_EQ:
			isOperator = "==";
			break;

		case TOK_NOT_EQ:
			isOperator = "!=";
			break;

		case TOK_LT:
			isOperator = "<";
			break;

		case TOK_LEQ:
			isOperator = "<=";
			break;

		case TOK_GT:
			isOperator = ">";
			break;

		case TOK_GEQ:
			isOperator = ">=";
			break;

		default:
			LETHE_RET_FALSE(Expect(false, "invalid operator"));
		}
	}

	if (isOperator)
	{
		ts->ConsumeToken();

		if (!currentScope || currentScope->type != NSCOPE_STRUCT)
			LETHE_RET_FALSE(ExpectPrev(false, "operators only allowed inside structs"));
	}

	UniquePtr<AstNode> name = isOperator
							  ? NewAstText<AstText>(isOperator, AST_IDENT, ts->PeekToken().location)
							  : ParseScopeResolution(depth+1);
	LETHE_RET_FALSE(name);

	if (isOperator)
		name->qualifiers |= AST_Q_OPERATOR;

	if (name->type != AST_OP_SCOPE_RES)
		name->flags |= AST_F_RESOLVED;

	if ((ntype->qualifiers & AST_Q_STATIC) != 0 && (isCtor || isDtor))
		LETHE_RET_FALSE(ExpectPrev(false, "static ctor/dtor not supoorted"));

	if (isDtor)
	{
		String &tname = AstStaticCast<AstText *>(name.Get())->text;
		tname = String("~") + tname;
		tname = AddString(tname.Ansi());
	}

	// now peek...
	if (ts->PeekToken().type == TOK_LBR)
	{
		if (isCtor || isDtor)
		{
			ts->ConsumeToken();
			LETHE_RET_FALSE(Expect(ts->PeekToken().type == TOK_RBR,
								  String::Printf("unexpected args for %s", isCtor ? "ctor" : "dtor").Ansi()));
			ts->UngetToken();
		}

		bool isOuter = name->type == AST_OP_SCOPE_RES;

		// it's a function decl
		String &fn = AstStaticCast<AstText *>(name.Get())->text;

		if (currentScope->IsGlobal() && (fn == "__init" || fn == "__exit"))
		{
			fn = String::Printf("%s$%d", fn.Ansi(), Atomic::Increment(*pstaticInitCounter));
		}

		UniquePtr<AstNode> fdecl = ParseFuncDecl(ntype, name, depth+1);
		LETHE_RET_FALSE(fdecl);

		if (isOperator && !(fdecl->qualifiers & AST_Q_STATIC))
			LETHE_RET_FALSE(ExpectPrev(false, "operators can only be static member functions"));

		fdecl->qualifiers &= ~(AST_Q_CTOR | AST_Q_DTOR);

		fdecl->qualifiers |= AST_Q_CTOR * isCtor;
		fdecl->qualifiers |= AST_Q_DTOR * isDtor;

		if (!isOuter && !isOperator)
			LETHE_RET_FALSE(AddScopeMember(fn, fdecl.Get(), isCtor));

		if (isOperator)
			currentScope->operators.Add(fdecl.Get());

		return fdecl.Detach();
	}

	if (isOperator)
		LETHE_RET_FALSE(ExpectPrev(false, "operators can only be functions"));

	return ParseVarDecl(ntype, name, depth+1);
}

AstNode *Compiler::ParseEnumDecl(UniquePtr<AstNode> &ntype, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	// after enum keyword

	auto enumClass = false;

	const auto ntok = ts->PeekToken().type;

	if (ntok == TOK_KEY_CLASS || ntok == TOK_KEY_STRUCT)
	{
		enumClass = true;
		ts->ConsumeToken();
		ntype->qualifiers |= AST_Q_ENUM_CLASS;
	}

	auto opos = ts->GetPosition();
	nofail++;
	UniquePtr<AstNode> nname = ParseName(depth+1);
	nofail--;

	String enumName;

	if (nname.IsEmpty())
	{
		if (enumClass)
			LETHE_RET_FALSE(ExpectPrev(false, "enum class must have a name"));

		nname = NewAstNode<AstNode>(AST_EMPTY, ntype->location);
		ts->UngetToken(ts->GetPosition() - opos);
	}
	else
	{
		LETHE_RET_FALSE(nname);
		enumName = AstStaticCast<const AstText *>(nname.Get())->text;

		LETHE_RET_FALSE(AddScopeMember(enumName, ntype.Get()));
	}

	auto *scp = currentScope;

	if (enumClass)
	{
		scp = new NamedScope;
		scp->parent = currentScope;
		scp->name = enumName;
		currentScope->namedScopes[enumName] = scp;
	}

	NamedScopeGuard nsg(this, scp);

	nname->flags |= AST_F_RESOLVED;
	ntype->Add(nname.Detach());

	UniquePtr<AstNode> underlying;

	// handle underlying type
	if (ts->PeekToken().type == TOK_COLON)
	{
		ts->ConsumeToken();
		underlying = ParseType(depth+1);
		LETHE_RET_FALSE(underlying);
	}
	else
	{
		underlying = NewAstNode<AstTypeInt>(ts->GetTokenLocation());
	}

	ntype->Add(underlying.Detach());

	// expect lblock
	LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));

	ntype->num.ul = 0;
	bool itemValueKnown = 1;

	AstText *lastItem = 0;

	for (;;)
	{
		// parse individual items
		const Token &nt = ts->PeekToken();

		if (nt.type == TOK_SHARP)
		{
			auto tokLine = nt.location.line;
			ts->ConsumeToken();
			LETHE_RET_FALSE(ParseDirective(tokLine));
			continue;
		}

		if (nt.type == TOK_RBLOCK)
		{
			ts->ConsumeToken();
			return ntype.Detach();
		}

		// expecting identifier
		LETHE_RET_FALSE(Expect(nt.type == TOK_IDENT, "expected identifier"));
		AstText *curItem = NewAstText<AstEnumItem>(nt.text, nt.location);
		UniquePtr<AstNode> item = curItem;
		item->flags |= AST_F_SKIP_CGEN;
		LETHE_RET_FALSE(AddScopeMember(nt.text, item));

		ts->ConsumeToken();
		// now check to see if we get assignment
		UniquePtr<AstNode> itemInit;

		if (ts->PeekToken().type == TOK_EQ)
		{
			ts->ConsumeToken();
			itemInit = ParseAssignExpression(depth+1);
			LETHE_RET_FALSE(itemInit);
			item->Add(itemInit.Detach());
			itemValueKnown = 0;
		}
		else if (itemValueKnown)
		{
			AstNode *val = NewAstNode<AstConstULong>(nt.location);
			val->num.ul = ntype->num.ul;
			item->Add(val);
			item->num.ul = ntype->num.ul++;
			item->flags |= AST_F_RESOLVED;
		}
		else
		{
			// FIXME: better
			AstText *prevSrc = lastItem;
			LETHE_RET_FALSE(prevSrc);
			AstNode *dummyInit = NewAstNode<AstBinaryOp>(AST_OP_ADD, nt.location);
			AstText *prev = NewAstText<AstSymbol>(prevSrc->text.Ansi(), nt.location);
			AstNode *incr = NewAstNode<AstConstInt>(nt.location);
			incr->num.i = 1;

			dummyInit->Add(prev);
			dummyInit->Add(incr);
			item->Add(dummyInit);
		}

		lastItem = curItem;

		// expect comma or RBR
		TokenType ntt = ts->PeekToken().type;
		LETHE_RET_FALSE(Expect(ntt == TOK_COMMA || ntt == TOK_RBLOCK, "unexpected token"));
		ts->ConsumeTokenIf(TOK_COMMA);
		ntype->Add(item.Detach());
	}
}

AstNode *Compiler::ParseQualifiedDecl(ULong qualifiers, Int depth)
{
	// note: this must be a type!
	LETHE_RET_FALSE(CheckDepth(depth));
	const Token &t = ts->PeekToken();

	switch(t.type)
	{
	case TOK_DOUBLE_COLON:
	case TOK_NOT:
	case TOK_IDENT:
	case TOK_KEY_THIS:
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
		// at this point, it could be variable decl or function decl
	{
		UniquePtr<AstNode> tmp;

		if (t.type == TOK_KEY_THIS)
		{
			tmp = NewAstNode<AstTypeVoid>(t.location);
			tmp->qualifiers |= AST_Q_CTOR;
			ts->ConsumeToken();
		}
		else if (t.type == TOK_NOT)
		{
			tmp = NewAstNode<AstTypeVoid>(t.location);
			tmp->qualifiers |= AST_Q_DTOR;
		}
		else
			tmp = ParseTypeWithQualifiers(depth+1, qualifiers, true);

		LETHE_RET_FALSE(tmp);
		tmp->qualifiers |= qualifiers;

		// copy const to funcptr/delegate elem type
		if (tmp->type == AST_TYPE_FUNC_PTR || tmp->type == AST_TYPE_DELEGATE)
			tmp->nodes[0]->qualifiers |= qualifiers & AST_Q_CONST;

		return ParseFuncOrVarDecl(tmp, depth+1);
	}
	break;

	case TOK_KEY_STRUCT:
	{
		UniquePtr<AstNode> tmp = NewAstNode<AstTypeStruct>(t.location);
		Swap(AstStaticCast<AstTypeStruct *>(tmp.Get())->attributes, attributes);

		// copy qualifiers
		tmp->qualifiers |= qualifiers;
		ts->ConsumeToken();
		return ParseStructDecl(tmp, depth+1);
	}
	break;

	case TOK_KEY_CLASS:
	{
		UniquePtr<AstNode> tmp = NewAstNode<AstTypeClass>(t.location);
		Swap(AstStaticCast<AstTypeClass *>(tmp.Get())->attributes, attributes);

		// copy qualifiers
		tmp->qualifiers |= qualifiers;
		ts->ConsumeToken();
		return ParseClassDecl(tmp, depth+1);
	}

	case TOK_KEY_ENUM:
	{
		UniquePtr<AstNode> tmp = NewAstNode<AstTypeEnum>(t.location);
		Swap(AstStaticCast<AstTypeEnum *>(tmp.Get())->attributes, attributes);

		// copy qualifiers
		tmp->qualifiers |= qualifiers;
		ts->ConsumeToken();
		return ParseEnumDecl(tmp, depth+1);
	}

	default:
		;
	}

	Expect(false, "unexpected token");
	return nullptr;
}

AstNode *Compiler::ParseStructDecl(UniquePtr<AstNode> &ntype, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	// after struct keyword

	if (currentScope->IsComposite())
		ntype->qualifiers |= structAccess;

	const auto &at = ts->PeekToken();

	if (at.type == TOK_IDENT && StringRef(at.text) == "alignas")
	{
		ts->ConsumeToken();
		auto *alignExpr = ParseExpression(depth+1);
		LETHE_RET_FALSE(alignExpr);

		AstStaticCast<AstTypeStruct *>(ntype.Get())->alignExpr = alignExpr;
	}

	ntype->qualifiers |= ParseQualifiers();
	UniquePtr<AstNode> nname = ParseStructName(depth+1);
	LETHE_RET_FALSE(nname);
	nname->flags |= AST_F_RESOLVED;

	// nobounds inherited to var decls
	auto inheritStructQualifiers = ntype->qualifiers & AST_Q_NOBOUNDS;

	// check if it's a struct template
	if (!nname->nodes.IsEmpty())
		ntype->qualifiers |= AST_Q_TEMPLATE;

	LETHE_ASSERT(nname->type == AST_IDENT);
	const String &sname = static_cast< const AstText * >(nname.Get())->text;

	SelfMacroGuard sg(ts, sname);

	auto *nnamePtr = nname.Get();
	ntype->Add(nname.Detach());
	// now we might support inheritance...
	UniquePtr<AstNode> base;
	const Token &bt = ts->PeekToken();
	AstNode *baseNamePtr = nullptr;

	if (bt.type == TOK_COLON)
	{
		base = NewAstNode<AstNode>(AST_BASE, bt.location);
		ts->ConsumeToken();
		ULong iqual = ParseQualifiers();
		UniquePtr<AstNode> baseName = ParseScopeResolution(depth+1);
		LETHE_RET_FALSE(baseName);

		baseNamePtr = baseName.Get();

		baseName->qualifiers |= iqual;
		base->Add(baseName.Detach());
	}
	else
	{
		if (ntype->type == AST_CLASS && !(ntype->qualifiers & AST_Q_INTRINSIC))
		{
			const char *baseName = "object";

			auto *scope = currentScope;
			const bool isState = ntype->qualifiers & AST_Q_STATE;

			if (isState)
			{
				// if it's a nested state class, auto-inject parent base

				while (scope)
				{
					if (scope->type == NSCOPE_CLASS)
						break;

					scope = scope->parent;
				}

				if (scope)
					baseName = scope->name.Ansi();
			}

			// force-inject base "object", must be defined in _native.script
			base = NewAstNode<AstNode>(AST_BASE, bt.location);
			auto defaultBase = NewAstNode<AstScopeResOp>(bt.location);
			auto defaultBaseSym = NewAstText<AstSymbol>(baseName, bt.location);
			auto dummy = NewAstNode<AstNode>(AST_NONE, bt.location);
			dummy->flags |= AST_F_RESOLVED;
			defaultBase->Add(dummy);

			if (isState && scope)
			{
				// we need to be fully qualified
				scope = scope->parent;

				StackArray<const NamedScope *, 16> scopeChain;

				while (scope)
				{
					if (scope->type == NSCOPE_CLASS || scope->type == NSCOPE_NAMESPACE)
						scopeChain.Add(scope);

					scope = scope->parent;
				}

				for (Int idx=scopeChain.GetSize()-1; idx>=0; idx--)
					defaultBase->Add(NewAstText<AstSymbol>(scopeChain[idx]->name.Ansi(), bt.location));
			}

			defaultBase->Add(defaultBaseSym);
			base->Add(defaultBase);
		}
		else
		{
			base = NewAstNode<AstNode>(AST_BASE_NONE, bt.location);
			// no base => nothing to resolve
			base->flags |= AST_F_RESOLVED;
		}
	}

	base->flags |= AST_F_SKIP_CGEN;
	ntype->Add(base.Detach());

	bool isClass = ntype->type == AST_CLASS;

	// expect lblock
	if (ts->PeekToken().type == TOK_SEMICOLON)
	{
		ts->ConsumeToken();
		ts->BeginMacroScope();
		ts->AppendEof(Token(TOK_RBLOCK));

		if (isClass)
			++classOpen;
	}
	else
		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));

	NamedScope *scp = AddUniqueNamedScope(sname);
	LETHE_RET_FALSE(scp);
	scp->node = ntype.Get();
	ntype->scopeRef = scp;
	NamedScopeGuard nsg(this, scp);

	bool hasInitializedMembers = false;
	bool hasCustomCtor = false;

	scp->type = isClass ? NSCOPE_CLASS : NSCOPE_STRUCT;

	if (ntype->qualifiers & AST_Q_TEMPLATE)
	{
		// inject typedefs if it's a template
		auto *structType = AstStaticCast<AstTypeStruct *>(ntype.Get());

		for (auto *it : nnamePtr->nodes)
		{
			const auto &argname = AstStaticCast<AstText *>(it)->text;

			AstTypeStruct::TemplateArg arg;
			arg.name = argname;

			arg.typedefNode = AstStaticCast<AstTypeDef *>(NewAstNode<AstTypeDef>(it->location));
			arg.typedefNode->Add(NewAstNode<AstTypeVoid>(it->location));

			auto *tdefName = NewAstText<AstText>(argname.Ansi(), AST_IDENT,  it->location);
			tdefName->flags |= AST_F_RESOLVED;
			arg.typedefNode->Add(tdefName);

			arg.typedefNode->flags |= AST_F_SKIP_CGEN;

			// mark template args same as struct as resolved, so that we can do B<T> : A<T> later
			if (baseNamePtr)
			{
				for (auto *nit : baseNamePtr->nodes)
				{
					LETHE_ASSERT(nit->type == AST_IDENT);

					// use fast compare => only possible because we collect unique refcounted strings
					if (AstStaticCast<AstText *>(nit)->text.Ansi() == argname.Ansi())
					{
						nit->scopeRef = ntype->scopeRef;
						break;
					}
				}
			}

			ntype->Add(arg.typedefNode);

			structType->templateArgs.Add(arg);

			LETHE_RET_FALSE(AddScopeMember(argname, arg.typedefNode));
		}
	}

	// parse struct body

	// reset struct access qualifiers
	AccessGuard acg(this, 0);

	for (;;)
	{
		const Token &nt = ts->PeekToken();

		if (nt.type == TOK_LARR)
		{
			attributes = ParseAttributes();
			LETHE_RET_FALSE(attributes);
			continue;
		}

		if (nt.type == TOK_SHARP)
		{
			auto tokLine = nt.location.line;
			ts->ConsumeToken();
			LETHE_RET_FALSE(ParseDirective(tokLine));
			continue;
		}

		if (nt.type == TOK_KEY_ENDCLASS)
		{
			if (classOpen <= 0)
			{
				ExpectPrev(false, "no class open");
				return nullptr;
			}

			--classOpen;
			ts->PopEofToken();
			ts->ConsumeToken();
			ts->ConsumeTokenIf(TOK_SEMICOLON);
			ts->EndMacroScope();
			break;
		}

		if (isClass && nt.type == TOK_KEY_IGNORES)
		{
			ts->ConsumeToken();
			// parse comma separated list

			for (;;)
			{
				const auto &itok = ts->GetToken();

				if (itok.type == TOK_IDENT)
				{
					AstStaticCast<AstTypeClass *>(ntype.Get())->ignores.Add(AddString(itok.text));
					ts->ConsumeTokenIf(TOK_COMMA);
					continue;
				}

				if (itok.type == TOK_SEMICOLON)
					break;

				ts->UngetToken();
				break;
			}

			continue;
		}

		if (nt.type == TOK_RBLOCK)
		{
			ts->ConsumeToken();
			break;
		}

		if (nt.type == TOK_SEMICOLON)
		{
			ts->ConsumeToken();
			continue;
		}

		if (nt.type == TOK_KEY_STATIC_ASSERT)
		{
			auto *sa = ParseStaticAssert(depth+1);
			LETHE_RET_FALSE(sa);
			ntype->Add(sa);
			continue;
		}

		if (nt.type == TOK_KEY_TYPEDEF)
		{
			auto *tdef = ParseTypeDef(depth+1);
			LETHE_RET_FALSE(tdef);
			ntype->Add(tdef);
			continue;
		}

		if (nt.type == TOK_KEY_USING)
		{
			auto *tdef = ParseUsing(depth+1);
			LETHE_RET_FALSE(tdef);
			ntype->Add(tdef);
			continue;
		}

		if (nt.type == TOK_KEY_MACRO)
		{
			LETHE_RET_FALSE(ParseMacro(depth+1));
			continue;
		}

		// try to parse C++'s public:, private: and so on
		if (nt.type == TOK_KEY_PUBLIC || nt.type == TOK_KEY_PRIVATE || nt.type == TOK_KEY_PROTECTED)
		{
			auto nttype = nt.type;

			ts->ConsumeToken();

			if (ts->PeekToken().type == TOK_COLON)
			{
				ts->ConsumeToken();

				switch(nttype)
				{
				case TOK_KEY_PRIVATE:
					structAccess = AST_Q_PRIVATE;
					break;

				case TOK_KEY_PROTECTED:
					structAccess = AST_Q_PROTECTED;
					break;

				default:;
					structAccess = 0;
				}
				continue;
			}
			else
				ts->UngetToken();
		}

		if (nt.type == TOK_KEY_DEFAULT)
		{
			// parse default initializer
			UniquePtr<AstNode> defInit = NewAstNode<AstDefaultInit>(nt.location);
			defInit->flags |= AST_F_SKIP_CGEN;
			ts->ConsumeToken();
			AstNode *stmt = ParseStatement(depth+1);
			LETHE_RET_FALSE(stmt);

			defInit->Add(stmt);

			ts->ConsumeTokenIf(TOK_SEMICOLON);
			ntype->Add(defInit.Detach());
			hasInitializedMembers = true;
			continue;
		}

		UniquePtr<AstNode> decl = ParseQualifiedDecl(ParseQualifiers(), depth+1);
		LETHE_RET_FALSE(decl);

		if (decl->type == AST_FUNC)
		{
			if (decl->qualifiers & (AST_Q_CTOR | AST_Q_DTOR))
				if (decl->nodes[AstFunc::IDX_RET]->type != AST_TYPE_VOID)
					LETHE_RET_FALSE(ExpectPrev(false, "invalid ctor/dtor return type"));

			// check ctor/dtor name
			if ((decl->qualifiers & AST_Q_CTOR) && sname != AstStaticCast<AstText *>(decl->nodes[1])->text)
				LETHE_RET_FALSE(ExpectPrev(false, "invalid constructor name"));

			auto isValidDtorName = [&sname](const String &dname)->bool
			{
				return dname.GetLength() == sname.GetLength()+1 && dname.StartsWith('~') && dname.EndsWith(sname);
			};

			if ((decl->qualifiers & AST_Q_DTOR) && !isValidDtorName(AstStaticCast<AstText *>(decl->nodes[1])->text))
				LETHE_RET_FALSE(ExpectPrev(false, "invalid destructor name"));

			hasCustomCtor = hasCustomCtor || (decl->qualifiers & AST_Q_CTOR) != 0;

			// mark as method for convenience
			if (!(decl->qualifiers & AST_Q_STATIC))
			{
				decl->qualifiers |= AST_Q_METHOD;

				if (isClass)
				{
					// mark non-final class methods and destructors as virtual
					if (!(decl->qualifiers & AST_Q_FINAL) || (decl->qualifiers & AST_Q_DTOR))
						decl->qualifiers |= AST_Q_VIRTUAL;
				}
			}
		}

		if (decl->type == AST_VAR_DECL_LIST)
		{
			decl->nodes[0]->qualifiers &= ~AST_Q_LOCAL_INT;
			decl->nodes[0]->qualifiers |= inheritStructQualifiers;
		}

		ntype->Add(decl.Detach());
	}

	for (auto *decl : ntype->nodes)
	{
		if (hasInitializedMembers)
			break;

		if (decl->type != AST_VAR_DECL_LIST)
			continue;

		// check for initialized vars...
		AstConstIterator ci(decl);
		const AstNode *n;

		while ((n = ci.Next()) != nullptr)
		{
			if (n->type != AST_VAR_DECL)
				continue;

			// initialized members flag ignored if static or constexpr (so that we don't force expensive custom ctor)
			hasInitializedMembers = hasInitializedMembers ||
				(n->nodes.GetSize() >= 2 && !(decl->nodes[0]->qualifiers & (AST_Q_CONSTEXPR | AST_Q_STATIC)));
		}
	}

	if (!hasInitializedMembers || hasCustomCtor)
		return ntype.Detach();

	// create empty constructor manually!
	AstNode *func;
	{
		NamedScopeGuard nsg2(this, currentScope->Add(new NamedScope(NSCOPE_ARGS)));

		func = NewAstNode<AstFunc>(ntype->location);
		func->Add(NewAstNode<AstTypeVoid>(ntype->location));
		auto fname = func->Add(NewAstText<AstSymbol>(sname.Ansi(), ntype->location));
		fname->flags |= AST_F_RESOLVED;
		fname->target = func;
		func->Add(NewAstNode<AstNode>(AST_ARG_LIST, ntype->location))->flags |= AST_F_RESOLVED;

		{
			NamedScopeGuard nsg3(this, currentScope->Add(new NamedScope(NSCOPE_FUNCTION)));

			func->Add(NewAstNode<AstFuncBody>(ntype->location));
		}
	}

	// note: since we only init, no need for fbody scope
	func->qualifiers |= AST_Q_METHOD | AST_Q_CTOR;
	ntype->Add(func);

	return ntype.Detach();
}

AstNode *Compiler::ParseClassDecl(UniquePtr<AstNode> &ntype, Int depth)
{
	auto res = ParseStructDecl(ntype, depth);

	// classes always have ctor
	if (res)
		res->qualifiers |= AST_Q_CTOR;

	return res;
}

}
