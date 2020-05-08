#include "Compiler.h"

#include "AstIncludes.h"

namespace lethe
{

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
			res->Add(NewAstNode<AstConstant>(AST_ARG_ELLIPSIS, nt.location));
			ts->ConsumeToken();
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
			argInit = ParseAssignExpression(depth+1);
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

AstNode *Compiler::ParseFuncDecl(UniquePtr<AstNode> &ntype, UniquePtr<AstNode> &nname, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

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
			LETHE_RET_FALSE(Expect(ts->PeekToken().type == TOK_LBLOCK, "expected `{'"));

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
			fbody = ParseBlock(depth+1, true, false, (fqualifiers & AST_Q_STATE) != 0);
			LETHE_RET_FALSE(fbody);
		}
	}
	else
		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_SEMICOLON, "expected `;'"));

	AstNode *res = NewAstNode<AstFunc>(args->location);
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

AstNode *Compiler::ParseVarDecl(UniquePtr<AstNode> &ntype, UniquePtr<AstNode> &nname, Int depth,
								bool refFirstInit, bool initOnly)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	if (currentScope->IsLocal())
		ntype->qualifiers |= AST_Q_LOCAL_INT;

	auto staticVar = ntype->qualifiers & AST_Q_STATIC;

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
		LETHE_RET_FALSE(AddScopeMember(vname, vn.Get()));

		if (refFirstInit && idx == 0 && init)
			vn->flags |= AST_F_REFERENCED;

		vn->qualifiers |= staticVar;

		vn->Add(nname.Detach());

		if (init)
			vn->Add(init.Detach());

		res->Add(vn.Detach());
		// now stop or continue parsing
		TokenType ntt = ts->PeekToken().type;

		if (ntt == TOK_SEMICOLON || ntt == TOK_RBR || ntt == TOK_RBLOCK || ntt == TOK_COLON)
			return res.Detach();

		LETHE_RET_FALSE(Expect(ntt == TOK_COMMA, "expected `,`"));
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

	// if we get here, it's probably invalid var decl; so reparse and get proper error
	ts->UngetToken(ts->GetPosition() - pos);

	ntype = ParseType(depth+1);
	LETHE_RET_FALSE(ntype);
	nname = ParseName(depth+1);
	LETHE_RET_FALSE(nname);

	return ParseVarDecl(ntype, nname, depth+1, refFirstInit, initOnly);
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
							  ? NewAstText<AstText>(isOperator, AST_NONE, ts->PeekToken().location)
							  : ParseScopeResolution(depth+1);
	LETHE_RET_FALSE(name);

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
	// expect lblock
	LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));

	ntype->num.i = 0;
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
			AstNode *val = NewAstNode<AstConstInt>(nt.location);
			val->num.i = ntype->num.i;
			item->Add(val);
			item->num.i = ntype->num.i++;
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

		if (t.type == TOK_NOT)
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

	// check if it's a struct template
	if (!nname->nodes.IsEmpty())
		ntype->qualifiers |= AST_Q_TEMPLATE;

	LETHE_ASSERT(nname->type == AST_IDENT);
	const String &sname = static_cast< const AstText * >(nname.Get())->text;
	const String sdname = "~" + sname;

	auto *nnamePtr = nname.Get();
	ntype->Add(nname.Detach());
	// now we might support inheritance...
	UniquePtr<AstNode> base;
	const Token &bt = ts->PeekToken();

	if (bt.type == TOK_COLON)
	{
		base = NewAstNode<AstNode>(AST_BASE, bt.location);
		ts->ConsumeToken();
		ULong iqual = ParseQualifiers();
		UniquePtr<AstNode> baseName = ParseScopeResolution(depth+1);
		LETHE_RET_FALSE(baseName);
		baseName->qualifiers |= iqual;
		base->Add(baseName.Detach());
	}
	else
	{
		if (ntype->type == AST_CLASS && !(ntype->qualifiers & AST_Q_INTRINSIC))
		{
			const char *baseName = "object";

			if (ntype->qualifiers & AST_Q_STATE)
			{
				// if it's a nested state class, auto-inject parent base
				auto *scope = currentScope;

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
			arg.typedefNode = nullptr;

			arg.typedefNode = AstStaticCast<AstTypeDef *>(NewAstNode<AstTypeDef>(nnamePtr->location));
			arg.typedefNode->Add(NewAstNode<AstTypeVoid>(nnamePtr->location));

			auto *tdefName = NewAstText<AstText>(argname.Ansi(), AST_IDENT,  nnamePtr->location);
			tdefName->flags |= AST_F_RESOLVED;
			arg.typedefNode->Add(tdefName);

			arg.typedefNode->flags |= AST_F_SKIP_CGEN;

			ntype->Add(arg.typedefNode);

			structType->templateArgs.Add(arg);

			LETHE_RET_FALSE(AddScopeMember(argname, arg.typedefNode));
		}
	}

	// parse struct body
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

		if (nt.type == TOK_KEY_TYPEDEF)
		{
			UniquePtr<AstNode> tdef = ParseTypeDef(depth+1);
			LETHE_RET_FALSE(tdef);
			ntype->Add(tdef.Detach());
			continue;
		}

		if (nt.type == TOK_KEY_USING)
		{
			UniquePtr<AstNode> tdef = ParseUsing(depth+1);
			LETHE_RET_FALSE(tdef);
			ntype->Add(tdef.Detach());
			continue;
		}

		if (nt.type == TOK_KEY_DEFAULT)
		{
			// parse default initializer
			UniquePtr<AstNode> defInit = NewAstNode<AstDefaultInit>(nt.location);
			defInit->flags |= AST_F_SKIP_CGEN;
			ts->ConsumeToken();

			for (;;)
			{
				UniquePtr<AstNode> varName = ParseScopeResolution(depth + 1);
				LETHE_RET_FALSE(varName);
				LETHE_RET_FALSE(Expect(ts->GetToken().type == TOK_EQ, "expected default assignment"));

				UniquePtr<AstNode> expr = ParseAssignExpression(depth + 1);
				LETHE_RET_FALSE(expr);

				defInit->Add(varName.Detach());
				defInit->Add(expr.Detach());

				if (ts->PeekToken().type != TOK_COMMA)
					break;

				ts->ConsumeToken();
			}

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

			if ((decl->qualifiers & AST_Q_DTOR) && sdname != AstStaticCast<AstText *>(decl->nodes[1])->text)
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

		ntype->Add(decl.Detach());
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
