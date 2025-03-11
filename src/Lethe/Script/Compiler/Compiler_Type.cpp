#include "Compiler.h"
#include "Warnings.h"

#include "AstIncludes.h"

namespace lethe
{

ULong Compiler::ParseQualifiers(bool ref)
{
	ULong res = 0;

	for (;;)
	{
		const Token &t = ts->PeekToken();

		switch(t.type)
		{
		case TOK_KEY_FORMAT:
			res |= AST_Q_FORMAT;
			break;

		case TOK_KEY_INTRINSIC:
			res |= AST_Q_INTRINSIC;
			break;

		case TOK_KEY_ASSERT:
			res |= AST_Q_ASSERT;
			break;

		case TOK_KEY_CONST:
			res |= AST_Q_CONST;
			break;

		case TOK_KEY_CONSTEXPR:
			res |= AST_Q_CONST | AST_Q_CONSTEXPR;
			break;

		case TOK_KEY_STATIC:
			res |= AST_Q_STATIC;
			break;

		case TOK_KEY_NATIVE:
			res |= AST_Q_NATIVE;
			break;

		case TOK_KEY_NOCOPY:
			res |= AST_Q_NOCOPY;
			break;

		case TOK_KEY_NOTEMP:
			res |= AST_Q_NOTEMP;
			break;

		case TOK_KEY_NOBOUNDS:
			res |= AST_Q_NOBOUNDS;
			break;

		case TOK_KEY_NOINIT:
			res |= AST_Q_NOINIT;
			break;

		case TOK_KEY_NONTRIVIAL:
			res |= AST_Q_NONTRIVIAL;
			break;

		case TOK_KEY_NODISCARD:
			res |= AST_Q_NODISCARD;
			break;

		case TOK_KEY_MAYBE_UNUSED:
			res |= AST_Q_MAYBE_UNUSED;
			break;

		case TOK_KEY_RAW:
			res |= AST_Q_RAW;
			break;

		case TOK_KEY_WEAK:
			res |= AST_Q_WEAK;
			break;

		case TOK_KEY_TRANSIENT:
			res |= AST_Q_TRANSIENT;
			break;

		case TOK_KEY_FINAL:
			res |= AST_Q_FINAL;
			break;

		case TOK_KEY_INLINE:
			res |= AST_Q_INLINE;
			break;

		case TOK_KEY_LATENT:
			res |= AST_Q_LATENT;
			break;

		case TOK_KEY_STATE:
			res |= AST_Q_STATE;
			break;

		case TOK_KEY_STATEBREAK:
			res |= AST_Q_STATEBREAK;
			break;

		case TOK_KEY_PUBLIC:
			res |= AST_Q_PUBLIC;
			break;

		case TOK_KEY_PROTECTED:
			res |= AST_Q_PROTECTED;
			break;

		case TOK_KEY_PRIVATE:
			res |= AST_Q_PRIVATE;
			break;

		case TOK_KEY_OVERRIDE:
			res |= AST_Q_OVERRIDE;
			break;

		case TOK_KEY_EDITABLE:
			res |= AST_Q_EDITABLE;
			break;

		case TOK_KEY_PLACEABLE:
			res |= AST_Q_PLACEABLE;
			break;

		case TOK_AND:
			if (ref)
				res |= AST_Q_REFERENCE;

			ts->ConsumeToken();
			return res;
			break;

		default:
			return res;
		}

		ts->ConsumeToken();
	}
}

AstNode *Compiler::ParseArrayType(UniquePtr<AstNode> &ntype, Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	do
	{
		const Token &nt = ts->PeekToken();

		// C-emulated array ref: type *
		bool caref = false;

		if (nt.type != TOK_LARR)
		{
			if (allowCEmulation && nt.type == TOK_MUL)
				caref = true;
			else
				break;
		}

		ts->ConsumeToken();
		const Token &t = ts->PeekToken();

		if (ntype->type == AST_TYPE_AUTO)
			LETHE_RET_FALSE(ExpectPrev(false, "auto[] not allowed"));

		UniquePtr<AstNode> tmp;
		UniquePtr<AstNode> expr;

		if (caref || t.type == TOK_RARR)
		{
			tmp = NewAstNode<AstTypeArrayRef>(nt.location);

			// inject resolve scope
			tmp->scopeRef = arrayRefScope;

			if (!caref)
				ts->ConsumeToken();
		}
		else
		{
			tmp = NewAstNode<AstTypeArray>(nt.location);
			expr = ParseExpression(depth+1);
			LETHE_RET_FALSE(expr);
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RARR, "expected `]`"));

			tmp->scopeRef = arrayScope;
		}

		tmp->qualifiers |= ntype->qualifiers;

		LETHE_RET_FALSE(ParseArrayType(ntype, depth+1));

		tmp->Add(ntype.Detach());

		if (!expr.IsEmpty())
			tmp->Add(expr.Detach());

		ntype = tmp.Detach();
	}
	while(false);

	return ntype;
}

AstNode *Compiler::ParseName(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	const Token &t = ts->GetToken();
	LETHE_RET_FALSE(ExpectPrev(t.type == TOK_IDENT, "expected identifier"));
	return NewAstText<AstText>(t.text, AST_IDENT, t.location);
}

AstNode *Compiler::ParseStructName(Int depth)
{
	auto *res = ParseName(depth);
	LETHE_RET_FALSE(res);

	if (ts->PeekToken().type != TOK_LT)
		return res;

	// parse struct template
	ts->ConsumeToken();

	for (;;)
	{
		auto *argname = ParseName(depth+1);
		LETHE_RET_FALSE(argname);

		argname->flags |= AST_F_RESOLVED | AST_F_SKIP_CGEN;

		res->Add(argname);

		auto nexttype = ts->GetToken().type;

		if (nexttype == TOK_GT)
			break;

		if (nexttype != TOK_COMMA)
		{
			delete res;
			ExpectPrev(false, "unexpected token");
			return nullptr;
		}
	}

	return res;
}

AstNode *Compiler::ParseScopeResolution(Int depth)
{
	UniquePtr<AstNode> res;
	LETHE_RET_FALSE(CheckDepth(depth));
	const Token &t = ts->GetToken();

	if (t.type == TOK_KEY_THIS)
	{
		// inline ctor/dtor
		auto *tnode = currentScope->FindThis();
		LETHE_RET_FALSE(ExpectPrev(tnode != nullptr, "couldn't find this"));
		res = NewAstText<AstSymbol>(tnode->name.Ansi(), t.location);
		return res.Detach();
	}

	LETHE_RET_FALSE(ExpectPrev(t.type == TOK_IDENT || t.type == TOK_DOUBLE_COLON, "expected identifier or `::`"));

	auto checkTemplate = [&](AstNode *tmp)
	{
		// perfomance note: this is quite slow... but it's tricky to parse this otherwise...
		// this is necessary for new template<type>
		if (ts->PeekToken().type != TOK_LT)
			return;

		// this could be a template....
		auto opos = ts->GetPosition();
		// ignore errors
		nofail++;
		bool isTempl = ParseSimpleTemplateType(tmp, depth+1, !templateAccum);
		nofail--;

		if (!isTempl)
			ts->UngetToken(ts->GetPosition() - opos);
	};

	if (t.type == TOK_DOUBLE_COLON)
	{
		ts->UngetToken();
		res = NewAstNode<AstNode>(AST_NONE, t.location);
	}
	else
	{
		res = NewAstText<AstSymbol>(t.text, t.location);
		checkTemplate(res);
	}

	// simplifying scope res AST node, now one node with ident list
	String tmpname;
	AstNode *scopeRoot = 0;

	while (ts->PeekToken().type == TOK_DOUBLE_COLON)
	{
		ts->ConsumeToken();

		bool dtorName = false;

		if (ts->PeekToken().type == TOK_NOT)
		{
			ts->ConsumeToken();
			dtorName = true;
		}

		const Token &nt = ts->GetToken();
		LETHE_RET_FALSE(ExpectPrev(nt.type == TOK_IDENT, "expected identifier"));

		StringRef sr(nt.text);

		if (dtorName)
		{
			tmpname = '~';
			tmpname += nt.text;
			sr = tmpname;
		}

		AstNode *sym = nullptr;

		if (!scopeRoot)
		{
			AstNode *left = res.Detach();
			res = NewAstNode<AstScopeResOp>(t.location);
			res->nodes.Reserve(2);
			res->Add(left);
			res->Add(sym = NewAstText<AstSymbol>(sr.Ansi(), nt.location));
			scopeRoot = res;
		}
		else
			scopeRoot->Add(sym = NewAstText<AstSymbol>(sr.Ansi(), nt.location));

		checkTemplate(sym);
	}

	return res.Detach();
}

AstNode *Compiler::CreateConstNumber(const Token &t)
{
	if (t.type == TOK_KEY_TRUE || t.type == TOK_KEY_FALSE)
	{
		// FIXME: or int?!
		AstNode *res = NewAstNode<AstConstBool>(t.location);
		res->num.i = t.type == TOK_KEY_TRUE;
		return res;
	}

	LETHE_ASSERT(t.IsNumber());

	if (t.type == TOK_DOUBLE)
	{
		bool isDouble = (t.numberFlags & TOKF_DOUBLE_SUFFIX) != 0;

		AstNode *res;

		if (!isDouble)
		{
			res = NewAstNode<AstConstFloat>(t.location);
			res->num.f = (Float)t.number.d;
		}
		else
		{
			res = NewAstNode<AstConstDouble>(t.location);
			res->num.d = t.number.d;
		}
		return res;
	}

	LETHE_ASSERT(t.type == TOK_ULONG || t.type == TOK_CHAR);

	// see what fits...
	ULong l = t.number.l;
	UInt f = t.numberFlags;

	if (t.type == TOK_CHAR)
	{
		if ((UInt)l != l)
			WarningLoc("integer constant overflow", t.location, WARN_OVERFLOW);

		AstNode *res;
		res = NewAstNode<AstConstChar>(t.location);
		res->num.i = (Int)l;
		return res;
	}

	if (l <= (ULong)Limits<Int>::MAX+1 && !(f & (TOKF_LONG_SUFFIX|TOKF_UNSIGNED_SUFFIX)))
	{
		AstNode *res = NewAstNode<AstConstInt>(t.location);

		if ((Int)l != (Long)l && l != 0x80000000u)
			WarningLoc("integer constant overflow", t.location, WARN_OVERFLOW);

		res->num.i = (Int)l;
		return res;
	}

	if (l <= Limits<UInt>::Max() && !(f & TOKF_LONG_SUFFIX) && (f & (TOKF_UNSIGNED_SUFFIX)))
	{
		AstNode *res = NewAstNode<AstConstUInt>(t.location);
		res->num.ui = (UInt)l;
		return res;
	}

	if (!(f & TOKF_UNSIGNED_SUFFIX))
	{
		AstNode *res = NewAstNode<AstConstLong>(t.location);
		res->num.l = (Long)l;
		return res;
	}

	AstNode *res = NewAstNode<AstConstULong>(t.location);
	res->num.ul = l;
	return res;
}

AstNode *Compiler::NewAstType(TokenType tt, const TokenLocation &nloc) const
{
	switch(tt)
	{
	case TOK_KEY_TYPE_VOID:
		return NewAstNode<AstTypeVoid>(nloc);

	case TOK_KEY_TYPE_BOOL:
		return NewAstNode<AstTypeBool>(nloc);

	case TOK_KEY_TYPE_BYTE:
		return NewAstNode<AstTypeByte>(nloc);

	case TOK_KEY_TYPE_SBYTE:
		return NewAstNode<AstTypeSByte>(nloc);

	case TOK_KEY_TYPE_SHORT:
		return NewAstNode<AstTypeShort>(nloc);

	case TOK_KEY_TYPE_USHORT:
		return NewAstNode<AstTypeUShort>(nloc);

	case TOK_KEY_TYPE_CHAR:
		return NewAstNode<AstTypeChar>(nloc);

	case TOK_KEY_TYPE_INT:
		return NewAstNode<AstTypeInt>(nloc);

	case TOK_KEY_TYPE_UINT:
		return NewAstNode<AstTypeUInt>(nloc);

	case TOK_KEY_TYPE_LONG:
		return NewAstNode<AstTypeLong>(nloc);

	case TOK_KEY_TYPE_ULONG:
		return NewAstNode<AstTypeULong>(nloc);

	case TOK_KEY_TYPE_FLOAT:
		return NewAstNode<AstTypeFloat>(nloc);

	case TOK_KEY_TYPE_DOUBLE:
		return NewAstNode<AstTypeDouble>(nloc);

	case TOK_KEY_TYPE_NAME:
		return NewAstNode<AstTypeName>(nloc);

	case TOK_KEY_TYPE_STRING:
		return NewAstNode<AstTypeString>(nloc);

	case TOK_KEY_AUTO:
		return NewAstNode<AstTypeAuto>(nloc);

	default:
		;
	}

	return 0;
}

AstNode *Compiler::ParseType(Int depth, bool init)
{
	return ParseTypeWithQualifiers(depth, 0, init);
}

AstNode *Compiler::ParseTypeWithQualifiers(Int depth, ULong nqualifiers, bool init)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	if (init)
		templateAccum = 0;

	ULong qualifiers = ParseQualifiers() | nqualifiers;

	if ((qualifiers & (AST_Q_RAW | AST_Q_WEAK)) == (AST_Q_RAW | AST_Q_WEAK))
		LETHE_RET_FALSE(ExpectPrev(false, "raw and weak are mutually exclusive"));

	const auto &it = ts->PeekToken();

	if (it.type == TOK_IDENT)
	{
		StringRef srarray(it.text);
		bool isDynArray = srarray == "array";
		bool isArrayView = srarray == "array_view";

		if (isDynArray || isArrayView)
		{
			// parse dynamic array type as array<type>; this is a new way
			// because I plan to use type[] for array refs (array_view is simply array ref alias)
			ts->ConsumeToken();
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LT, "expected `<' after array"));
			UniquePtr<AstNode> sub = ParseType(depth+1, 0);
			LETHE_RET_FALSE(sub);

			if (sub->qualifiers & AST_Q_REFERENCE)
				LETHE_RET_FALSE(ExpectPrev(false, "arrays of references not allowed"));

			auto arrClose = ts->PeekToken().type;

			LETHE_RET_FALSE(Expect(arrClose == TOK_GT || arrClose == TOK_SHR || arrClose == TOK_SHRU, "expected `>' to close array"));
			ts->ConsumeTokenIf(TOK_GT);

			if (!templateAccum)
				templateAccum = arrClose == TOK_SHR ? 1 : (arrClose == TOK_SHRU ? 2 : 0);
			else if (!--templateAccum)
				ts->ConsumeToken();

			UniquePtr<AstNode> atype = isDynArray ? NewAstNode<AstTypeDynamicArray>(it.location) : NewAstNode<AstTypeArrayRef>(it.location);
			atype->qualifiers |= isDynArray*AST_Q_DTOR | qualifiers;

			if (ts->PeekToken().type == TOK_AND)
			{
				ts->ConsumeToken();
				atype->qualifiers |= AST_Q_REFERENCE;
			}

			// inject resolve scope
			atype->scopeRef = isDynArray ? dynamicArrayScope : arrayRefScope;

			atype->Add(sub.Detach());

			LETHE_RET_FALSE(ParseArrayType(atype, depth+1));
			atype->qualifiers |= ParseQualifiers(1);

			return atype.Detach();
		}
	}

	UniquePtr<AstNode> res = ParseSimpleType(depth+1, qualifiers);
	LETHE_RET_FALSE(res);
	res->qualifiers |= qualifiers;

	// check if it's a func ptr or delegate...
	const Token &t = ts->PeekToken();

	if (t.type != TOK_IDENT)
		return res.Detach();

	StringRef sr(t.text);
	bool funPtr = sr == "function";
	bool dgPtr = sr == "delegate";

	if (!funPtr && !dgPtr)
		return res.Detach();

	ts->ConsumeToken();
	const Token &nt2 = ts->PeekToken();

	if (nt2.type != TOK_LBR)
	{
		ts->UngetToken();
		return res.Detach();
	}

	// it's indeed a funptr/delegate
	// note: ignoring nofail here...
	ts->ConsumeToken();
	UniquePtr<AstNode> args = ParseFuncArgsDecl(depth+1);
	LETHE_RET_FALSE(args);

	// modern C++ style auto func()->res decl
	if (res->type == AST_TYPE_AUTO && ts->PeekToken().type == TOK_C_DEREF)
	{
		ts->ConsumeToken();
		UniquePtr<AstNode> typeover = ParseType(depth+1);
		LETHE_RET_FALSE(typeover);
		typeover->qualifiers |= res->qualifiers;
		typeover->scopeRef = res->scopeRef;
		typeover.SwapWith(res);
	}

	UniquePtr<AstNode> nres;

	if (funPtr)
		nres = NewAstNode<AstTypeFuncPtr>(res->location);
	else
		nres = NewAstNode<AstTypeDelegate>(res->location);

	// copy __format to func ptrs as well
	nres->qualifiers = res->qualifiers & (AST_Q_FORMAT);

	nres->Add(res.Detach());
	nres->Add(args.Detach());
	Swap(res, nres);

	LETHE_RET_FALSE(ParseArrayType(res, depth+1));
	res->qualifiers |= ParseQualifiers(1);

	return res.Detach();
}

bool Compiler::ParseSimpleTemplateType(AstNode *tmp, Int depth, bool init)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	// parse template type (=instance candidate)
	ts->ConsumeToken();

	// find rightmost tmp
	AstNode *rightmost = tmp;

	while (rightmost)
	{
		if (rightmost->nodes.IsEmpty())
			break;

		rightmost = rightmost->nodes.Back();
	}

	LETHE_RET_FALSE(rightmost && rightmost->type == AST_IDENT);

	auto cleanupSize = rightmost->nodes.GetSize();

	auto cleanup = [&]()
	{
		for (Int i=cleanupSize; i<rightmost->nodes.GetSize(); i++)
		{
			rightmost->nodes[i]->parent = nullptr;
			delete rightmost->nodes[i];
		}

		rightmost->nodes.Resize(cleanupSize);
	};

	for (;;)
	{
		UniquePtr<AstNode> arg = ParseType(depth+1, init);
		init = false;

		if (!arg)
		{
			cleanup();
			return false;
		}

		rightmost->Add(arg.Detach());

		auto nexttype = ts->GetToken().type;

		if (nexttype == TOK_GT)
			break;

		if (nexttype == TOK_SHR || nexttype == TOK_SHRU)
		{
			ts->UngetToken();

			if (!templateAccum)
				templateAccum = nexttype == TOK_SHR ? 1 : (nexttype == TOK_SHRU ? 2 : 0);
			else if (!--templateAccum)
				ts->ConsumeToken();

			break;
		}

		if (nexttype != TOK_COMMA)
		{
			cleanup();
			return ExpectPrev(false, "unexpected token");
		}
	}

	rightmost->flags |= AST_F_TEMPLATE_INSTANCE;

	return true;
}

AstNode *Compiler::ParseSimpleType(Int depth, ULong nqualifiers)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	ULong qualifiers = nqualifiers | ParseQualifiers();
	const Token &t = ts->PeekToken();

	TokenType tokType = t.type;

	if (tokType == TOK_IDENT && StringRef(t.text) == "name")
		tokType = TOK_KEY_TYPE_NAME;

	switch(tokType)
	{
	case TOK_DOUBLE_COLON:
	case TOK_IDENT:
	{
		UniquePtr<AstNode> tmp = ParseScopeResolution(depth+1);
		LETHE_RET_FALSE(tmp);
		// copy qualifiers
		tmp->qualifiers |= qualifiers;
		LETHE_RET_FALSE(ParseArrayType(tmp, depth+1));
		tmp->qualifiers |= ParseQualifiers(1);

		return tmp.Detach();
	}
	break;

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
	{
		UniquePtr<AstNode> tmp = NewAstType(tokType, t.location);
		LETHE_ASSERT(tmp);

		// inject resolve scope
		// note: since auto is only compatible with string, simply inject string scope for auto
		tmp->scopeRef = (tokType == TOK_KEY_TYPE_STRING || tokType == TOK_KEY_AUTO) ? stringScope : nullScope;

		ts->ConsumeToken();

		// elementary type always resolved
		if (tokType != TOK_KEY_AUTO)
			tmp->flags |= AST_F_RESOLVED;

		// copy qualifiers
		tmp->qualifiers |= qualifiers;
		LETHE_RET_FALSE(ParseArrayType(tmp, depth+1));
		tmp->qualifiers |= ParseQualifiers(1);
		return tmp.Detach();
	}
	break;

	default:
		;
	}

	Expect(false, "unexpected token");
	return nullptr;
}

AstNode *Compiler::ParseInitializerList(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	auto *ilist = AstStaticCast<AstInitializerList *>(NewAstNode<AstInitializerList>(ts->PeekToken().location));
	UniquePtr<AstNode> res = ilist;
	LETHE_ASSERT(ts->PeekToken().type == TOK_LBLOCK);
	ts->ConsumeToken();

	for (;;)
	{
		// we can have either nested initializer or assign expr
		TokenType tt = ts->PeekToken().type;

		// note: we support both C-style designators and direct assignment-based designators (sexier!);
		// for assignment expressions you have to use parentheses if you must

		bool isDesignator = tt == TOK_DOT;

		if (isDesignator)
		{
			ts->ConsumeToken();
		}
		else if (tt == TOK_IDENT)
		{
			ts->GetToken();
			isDesignator = ts->PeekToken().type == TOK_EQ;
			ts->UngetToken(1);
		}

		if (isDesignator)
		{
			// designator
			const auto &dname = ts->GetToken();
			LETHE_RET_FALSE(ExpectPrev(dname.type == TOK_IDENT, "expected identifier"));
			const auto &dstr = AddString(dname.text);

			if (ts->PeekToken().type != TOK_EQ)
			{
				// not a designator - could be context-dependent enum class symbol
				// note: at this point we know it's a C-style designator, so unget(2) is fine
				ts->UngetToken(2);
			}
			else
			{
				// make sure it's not a dup-name
				for (Int i=0; i<ilist->designators.GetSize(); i++)
					if (ilist->designators[i].name == dstr)
						LETHE_RET_FALSE(ExpectPrev(false, "designator redefinition"));

				// skip =
				ts->ConsumeToken();

				auto didx = res->nodes.GetSize();
				ilist->designators.ResizeToFit(didx);
				auto &dst = ilist->designators[didx];
				dst.name = dstr;
				tt = ts->PeekToken().type;
			}
		}

		if (tt == TOK_LBLOCK)
		{
			AstNode *subList = ParseInitializerList(depth+1);
			LETHE_RET_FALSE(subList);
			res->Add(subList);
			ts->ConsumeTokenIf(TOK_COMMA);
			continue;
		}

		if (tt == TOK_RBLOCK)
		{
			ts->ConsumeToken();
			break;
		}

		AstNode *iniExpr = ParseAssignExpression(depth+1);
		LETHE_RET_FALSE(iniExpr);
		res->Add(iniExpr);
		ts->ConsumeTokenIf(TOK_COMMA);
	}

	// assume empty initializer lists are resolved
	if (res->nodes.IsEmpty())
		res->flags |= AST_F_RESOLVED;

	return res.Detach();
}

AstNode *Compiler::ParseTypeDef(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	UniquePtr<AstNode> tdef = NewAstNode<AstTypeDef>(ts->PeekToken().location);

	ts->ConsumeToken();
	UniquePtr<AstNode> tmp = ParseType(depth + 1);
	LETHE_RET_FALSE(tmp);

	if (currentScope->IsComposite())
		tmp->qualifiers |= structAccess;

	UniquePtr<AstNode> name = ParseName(depth + 1);
	LETHE_RET_FALSE(name);

	name->flags |= AST_F_RESOLVED;

	ts->ConsumeTokenIf(TOK_SEMICOLON);

	LETHE_RET_FALSE(AddScopeMember(AstStaticCast<AstText *>(name.Get())->text, tdef));

	tdef->Add(tmp.Detach());
	tdef->Add(name.Detach());

	tdef->flags |= AST_F_SKIP_CGEN;
	return tdef.Detach();
}

AstNode *Compiler::ParseUsing(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	UniquePtr<AstNode> tdef = NewAstNode<AstTypeDef>(ts->PeekToken().location);

	ts->ConsumeToken();

	UniquePtr<AstNode> name = ParseName(depth + 1);
	LETHE_RET_FALSE(name);

	LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_EQ, "expected `='"));

	UniquePtr<AstNode> tmp = ParseType(depth + 1);
	LETHE_RET_FALSE(tmp);

	if (currentScope->IsComposite())
		tmp->qualifiers |= structAccess;

	name->flags |= AST_F_RESOLVED;

	ts->ConsumeTokenIf(TOK_SEMICOLON);

	LETHE_RET_FALSE(AddScopeMember(AstStaticCast<AstText *>(name.Get())->text, tdef));

	tdef->Add(tmp.Detach());
	tdef->Add(name.Detach());

	tdef->flags |= AST_F_SKIP_CGEN;
	return tdef.Detach();
}


}
