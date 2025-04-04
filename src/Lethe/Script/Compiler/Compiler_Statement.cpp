#include "Compiler.h"

#include "AstIncludes.h"

namespace lethe
{

AstNode *Compiler::ParseSwitchBody(Int depth, bool switchBreak)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = NewAstNode<AstSwitchBody>(ts->PeekToken().location);
	AstNode *defCase = nullptr;
	AstNode *curCase = nullptr;
	AstNode *lastAutoBreak = nullptr;

	auto testFallthrough = [&]()
	{
		const auto &nxt = ts->PeekToken();
		// if we get "case fallthrough expr:" or "default fallthrough:",
		// disable auto break
		if (nxt.type == TOK_IDENT && StringRef(nxt.text) == "fallthrough")
		{
			ts->ConsumeToken();

			if (lastAutoBreak)
			{
				lastAutoBreak->parent->ReplaceChild(lastAutoBreak, nullptr);
				delete lastAutoBreak;
			}
		}

		lastAutoBreak = nullptr;
	};

	for (;;)
	{
		const auto &nt = ts->PeekToken();

		if (nt.type == TOK_RBLOCK)
		{
			if (defCase && defCase->nodes.IsEmpty())
				defCase->flags |= AST_F_RESOLVED;

			LETHE_RET_FALSE(Expect(!res->nodes.IsEmpty(), "empty switch body"));
			ts->ConsumeToken();
			return res.Detach();
		}

		if (nt.type == TOK_KEY_CASE)
		{
			auto loc = nt.location;
			ts->ConsumeToken();

			testFallthrough();

			do
			{
				auto *expr = ParseAssignExpression(depth+1);
				LETHE_RET_FALSE(expr);
				auto *thisCase = NewAstNode<AstCase>(loc);
				thisCase->Add(expr);
				curCase = res->Add(thisCase);
			}
			while (ts->PeekToken().type == TOK_COMMA && ts->ConsumeToken());

			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_COLON, "expected `:`"));
			continue;
		}

		if (nt.type == TOK_KEY_DEFAULT)
		{
			LETHE_RET_FALSE(Expect(!defCase, "default case already specified"));
			ts->ConsumeToken();

			testFallthrough();

			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_COLON, "expected `:`"));
			defCase = NewAstNode<AstCaseDefault>(nt.location);
			curCase = res->Add(defCase);
			continue;
		}

		LETHE_RET_FALSE(Expect(curCase != nullptr, "no case specified"));

		if (switchBreak)
		{
			auto *blk = ParseSwitchBreakBlock(depth+1);
			LETHE_RET_FALSE(blk);

			const auto &nxt = ts->PeekToken();

			if (nxt.type == TOK_KEY_CASE || nxt.type == TOK_KEY_DEFAULT)
				lastAutoBreak = blk->Add(NewAstNode<AstBreak>(nxt.location));

			curCase->Add(blk);
			continue;
		}

		auto *stmt = ParseStatement(depth+1);
		LETHE_RET_FALSE(stmt);
		curCase->Add(stmt);
	}
}

bool Compiler::IsValidBreak(bool isCont) const
{
	NamedScope *ns = currentScope;

	while (ns)
	{
		if (ns->type == NSCOPE_LOOP || (!isCont && ns->type == NSCOPE_SWITCH))
			return 1;

		ns = ns->parent;
	}

	return 0;
}

AstNode *Compiler::ParseNoBreakStatement(Int depth)
{
	// break scope can access loop scope variables (this is a feature, not a bug)
	return ParseStatement(depth);
}

AstNode *Compiler::ParseAnonStructLiteral(Int depth)
{
	auto loc = ts->PeekToken().location;
	auto il = ParseInitializerList(depth+1);
	LETHE_RET_FALSE(il);
	AstNode *tmp = NewAstNode<AstStructLiteral>(loc);

	if (il->nodes.IsEmpty())
		il->flags |= AST_F_RESOLVED;

	// the tricky part is how to resolve this
	auto *sname = NewAstText<AstSymbol>("", loc);

	tmp->Add(sname);
	tmp->Add(il);
	return tmp;
}

bool Compiler::ParseReturn(Int depth, UniquePtr<AstNode> &res)
{
	if (ts->PeekToken().type == TOK_SEMICOLON)
	{
		ts->ConsumeToken();
		res->flags |= AST_F_RESOLVED;
		return true;
	}

	// try anonymous struct literal
	UniquePtr<AstNode> tmp = ts->PeekToken().type == TOK_LBLOCK ?
		ParseAnonStructLiteral(depth+1) :
		ParseExpression(depth+1);

	LETHE_RET_FALSE(tmp);

	NamedScope *s = currentScope;

	while (s->type != NSCOPE_ARGS)
	{
		s = s->parent;
		LETHE_ASSERT(s);
	}

	if (tmp->type == AST_STRUCT_LITERAL)
	{
		tmp->flags |= AST_F_NRVO;
		tmp->target = s->resultPtr;
	}
	else
	{
		AstNode *asgn = NewAstNode<AstBinaryAssign>(res->location);
		asgn->qualifiers |= AST_Q_CAN_MODIFY_CONSTANT;
		AstNode *resIdent = NewAstText<AstSymbol>("", res->location);
		resIdent->flags |= AST_F_RESOLVED;
		resIdent->qualifiers |= AST_Q_CAN_MODIFY_CONSTANT;

		resIdent->target = s->resultPtr;
		resIdent->qualifiers |= AST_Q_LOCAL_INT;
		asgn->Add(resIdent);
		asgn->Add(tmp.Detach());
		// add virtual expr node, helps peephole optimizer
		tmp = NewAstNode<AstExpr>(res->location);
		tmp->Add(asgn);
	}

	ts->ConsumeTokenIf(TOK_SEMICOLON);
	res->Add(tmp.Detach());
	return true;
}

AstNode *Compiler::ParseStatement(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	const Token &nt = ts->PeekToken();
	UniquePtr<AstNode> res;

	switch(nt.type)
	{
	case TOK_KEY_BREAK:
		if (!IsValidBreak(0))
		{
			ErrorLoc("illegal break", nt.location);
			return 0;
		}

		res = NewAstNode<AstBreak>(nt.location);
		ts->ConsumeToken();
		ts->ConsumeTokenIf(TOK_SEMICOLON);
		break;

	case TOK_KEY_CONTINUE:
		if (!IsValidBreak(1))
		{
			ErrorLoc("illegal continue", nt.location);
			return 0;
		}

		res = NewAstNode<AstContinue>(nt.location);
		ts->ConsumeToken();
		ts->ConsumeTokenIf(TOK_SEMICOLON);
		break;

	case TOK_KEY_RETURN:
	{
		res = NewAstNode<AstReturn>(nt.location);
		ts->ConsumeToken();

		LETHE_RET_FALSE(ParseReturn(depth+1, res));
	}
	break;

	case TOK_KEY_DO:
	{
		// FIXME: this is only here because of break/continue
		NamedScopeGuard ng(this, currentScope->Add(new NamedScope(NSCOPE_LOOP)));
		UniquePtr<AstNode> scope = NewAstNode<AstBlock>(nt.location);

		res = NewAstNode<AstDo>(nt.location);
		ts->ConsumeToken();
		UniquePtr<AstNode> body = ParseStatement(depth+1);
		LETHE_RET_FALSE(body);
		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_KEY_WHILE, "expected `while'"));
		UniquePtr<AstNode> tmp = ParseExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		ts->ConsumeTokenIf(TOK_SEMICOLON);
		res->Add(body.Detach());
		res->Add(tmp.Detach());

		if (ts->PeekToken().type == TOK_KEY_NOBREAK)
		{
			ts->ConsumeToken();
			auto nobreak = ParseNoBreakStatement(depth + 1);
			LETHE_RET_FALSE(nobreak);
			res->Add(nobreak);
		}

		scope->Add(res.Detach());
		res = scope.Detach();
	}
	break;

	case TOK_KEY_WHILE:
	{
		NamedScopeGuard ng(this, currentScope->Add(new NamedScope(NSCOPE_LOOP)));
		UniquePtr<AstNode> scope = NewAstNode<AstBlock>(nt.location);

		res = NewAstNode<AstWhile>(nt.location);
		ts->ConsumeToken();
		bool extra = ts->PeekToken().type == TOK_LBR;
		ts->ConsumeTokenIf(TOK_LBR);
		UniquePtr<AstNode> tmp = ParseVarDeclOrExpr(depth+1, 1);
		LETHE_RET_FALSE(tmp);

		if (tmp->type == AST_VAR_DECL_LIST && tmp->nodes.GetSize() != 2)
			LETHE_RET_FALSE(ExpectPrev(false, "only one initialized variable allowed"));

		if (extra)
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));

		UniquePtr<AstNode> body = ParseStatement(depth+1);
		LETHE_RET_FALSE(body);
		// note: not consuming potential semicolon here!, already consumed by ParseStatement

		if (tmp->type == AST_VAR_DECL_LIST)
		{
			// simply convert into a for loop
			res = NewAstNode<AstFor>(res->location);

			// synthesize cond!
			auto *src = AstStaticCast<AstText *>(tmp->nodes[1]->nodes[0]);
			UniquePtr<AstNode> cond = NewAstTextRef<AstSymbol>(StringRef(src->text.Ansi()), src->location);

			res->Add(tmp.Detach());
			res->Add(cond.Detach());
			// empty inc
			res->Add(NewAstNode<AstConstant>(AST_EMPTY, ts->PeekToken().location));
			res->Add(body.Detach());
		}
		else
		{
			res->Add(tmp.Detach());
			res->Add(body.Detach());
		}

		if (ts->PeekToken().type == TOK_KEY_NOBREAK)
		{
			ts->ConsumeToken();
			auto nobreak = ParseNoBreakStatement(depth + 1);
			LETHE_RET_FALSE(nobreak);
			res->Add(nobreak);
		}

		scope->Add(res.Detach());
		res = scope.Detach();
	}
	break;

	case TOK_KEY_FOR:
	{
		NamedScopeGuard ng(this, currentScope->Add(new NamedScope(NSCOPE_LOOP)));
		UniquePtr<AstNode> scope = NewAstNode<AstBlock>(nt.location);

		res = NewAstNode<AstFor>(nt.location);
		ts->ConsumeToken();
		bool extra = ts->PeekToken().type == TOK_LBR;
		ts->ConsumeTokenIf(TOK_LBR);

		// parse interior stuff
		UniquePtr<AstNode> initExpr;

		if (ts->PeekToken().type != TOK_SEMICOLON)
		{
			initExpr = ParseVarDeclOrExpr(depth+1);
			LETHE_RET_FALSE(initExpr);
		}
		else
			initExpr = NewAstNode<AstConstant>(AST_EMPTY, ts->PeekToken().location);

		const auto &tok = ts->GetToken();

		UniquePtr<AstNode> cond, inc;

		if (tok.type == TOK_COLON)
		{
			// range based for or int range...
			res->type = AST_FOR_RANGE;

			// problems:
			// + auto is broken as we fake init with int const of 0 => auto implies int, may not be what was intended!
			//		I have to fix this, how: use a new qualifier for auto vardecl inside range-based for!
			// + doesn't work for initializers
			// + no support for iterating arrays by value/reference
			//		(actually, no support for iterating custom containers like maps/sets!)
			// x? doesn't work for enums (actually, this may be desired as we don't know where to start, enum-init works though)

			// note: AST_BLOCK is hacky because of state vars
			if (initExpr->type == AST_IDENT || initExpr->type == AST_OP_ASSIGN || initExpr->type == AST_BLOCK)
			{
				const bool initialized = initExpr->type == AST_OP_ASSIGN;
				const bool state_var_block = initExpr->type == AST_BLOCK;

				if (initialized)
					LETHE_RET_FALSE(ExpectPrev(initExpr->nodes[0]->type == AST_IDENT, "invalid range based for"));

				if (state_var_block)
					LETHE_RET_FALSE(ExpectPrev(!initExpr->nodes.IsEmpty() && initExpr->nodes.Back()->type == AST_TYPEDEF, "invalid range based for"));

				AstNode *sym = initialized ? initExpr->nodes[0] : state_var_block ? initExpr->nodes.Back()->nodes[1] : initExpr.Get();

				cond = NewAstNode<AstBinaryOp>(AST_OP_LT, initExpr->location);
				auto *cmpop = cond.Get();
				cmpop->Add(NewAstText<AstSymbol>(AstStaticCast<AstText *>(sym)->text.Ansi(), initExpr->location));

				AstNode *cmpexpr = ParseExpression(depth+1);
				LETHE_RET_FALSE(cmpexpr);

				cmpop->Add(cmpexpr);

				// and finally increment!
				inc = NewAstNode<AstExpr>(initExpr->location);

				auto *incop = inc->Add(NewAstNode<AstUnaryPreOp>(AST_UOP_PREINC, initExpr->location));
				incop->Add(NewAstText<AstSymbol>(AstStaticCast<AstText *>(sym)->text.Ansi(), initExpr->location));

				if (!initialized)
				{
					// build fake assignment
					auto *tmp = initExpr.Get();

					auto *asgn = NewAstNode<AstBinaryAssign>(initExpr->location);

					if (state_var_block)
					{
						if (initExpr->nodes.GetSize() < 2)
						{
							// note: cloning sym here won't work
							auto *csym = initExpr->nodes[0]->nodes[0]->Clone();

							asgn->scopeRef = initExpr->scopeRef;
							asgn->Add(csym);
							asgn->Add(NewAstNode<AstConstInt>(tmp->location));
							initExpr->Add(asgn);
						}
						else
							delete asgn;
					}
					else
					{
						asgn->parent = initExpr->parent;
						initExpr->parent = nullptr;
						asgn->Add(initExpr.Detach());
						asgn->Add(NewAstNode<AstConstInt>(tmp->location));
						initExpr = asgn;
					}
				}
			}
			else
			{
				LETHE_RET_FALSE(ExpectPrev(
					initExpr->type == AST_VAR_DECL_LIST &&
					initExpr->nodes.GetSize() == 2 &&
					initExpr->nodes[1]->type == AST_VAR_DECL &&
					initExpr->nodes[1]->nodes.GetSize() >= 1,
				"invalid range based for"));

				const bool initialized = initExpr->nodes[1]->nodes.GetSize() > 1;

				if (!initialized && initExpr->nodes[0]->type == AST_TYPE_AUTO)
					initExpr->nodes[0]->qualifiers |= AST_Q_AUTO_RANGE_FOR;

				auto *vdecl = initExpr->nodes[1];

				if (!initialized)
					vdecl->Add(NewAstNode<AstConstInt>(vdecl->location));

				cond = NewAstNode<AstBinaryOp>(AST_OP_LT, vdecl->location);
				auto *cmpop = cond.Get();
				cmpop->Add(NewAstText<AstSymbol>(AstStaticCast<AstText *>(vdecl->nodes[0])->text.Ansi(), vdecl->location));

				AstNode *cmpexpr = ParseExpression(depth+1);
				LETHE_RET_FALSE(cmpexpr);

				cmpop->Add(cmpexpr);

				// and finally increment!
				inc = NewAstNode<AstExpr>(vdecl->location);

				auto *incop = inc->Add(NewAstNode<AstUnaryPreOp>(AST_UOP_PREINC, vdecl->location));
				incop->Add(NewAstText<AstSymbol>(AstStaticCast<AstText *>(vdecl->nodes[0])->text.Ansi(), vdecl->location));
			}
		}
		else
		{
			// standard for
			LETHE_RET_FALSE(ExpectPrev(tok.type == TOK_SEMICOLON, "expected `;`"));

			if (ts->PeekToken().type != TOK_SEMICOLON)
			{
				cond = ParseExpression(depth+1);
				LETHE_RET_FALSE(cond);
			}
			else
				cond = NewAstNode<AstConstant>(AST_EMPTY, ts->PeekToken().location);

			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_SEMICOLON, "expected `;`"));
			TokenType ntt = ts->PeekToken().type;

			if (ntt != TOK_RBR && ntt != TOK_LBLOCK)
			{
				inc = ParseExpression(depth+1);
				LETHE_RET_FALSE(inc);
				AstNode *tmp = NewAstNode<AstExpr>(inc->location);
				tmp->Add(inc.Detach());
				inc = tmp;
			}
			else
				inc = NewAstNode<AstConstant>(AST_EMPTY, ts->PeekToken().location);
		}

		if (extra)
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));

		UniquePtr<AstNode> body = ParseStatement(depth+1);
		LETHE_RET_FALSE(body);

		res->Add(initExpr.Detach());
		res->Add(cond.Detach());
		res->Add(inc.Detach());
		res->Add(body.Detach());

		if (ts->PeekToken().type == TOK_KEY_NOBREAK)
		{
			ts->ConsumeToken();
			auto nobreak = ParseNoBreakStatement(depth+1);
			LETHE_RET_FALSE(nobreak);
			res->Add(nobreak);
		}

		scope->Add(res.Detach());
		res = scope.Detach();
	}
	break;

	case TOK_KEY_IF:
	{
		NamedScopeGuard ng(this, currentScope->Add(new NamedScope(NSCOPE_LOCAL)));
		UniquePtr<AstNode> scope = NewAstNode<AstBlock>(nt.location);

		res = NewAstNode<AstIf>(nt.location);
		ts->ConsumeToken();
		bool extra = ts->PeekToken().type == TOK_LBR;
		ts->ConsumeTokenIf(TOK_LBR);
		UniquePtr<AstNode> tmp = ParseVarDeclOrExpr(depth+1, 1);
		LETHE_RET_FALSE(tmp);

		if (tmp->type == AST_VAR_DECL_LIST && ts->PeekToken().type == TOK_SEMICOLON)
		{
			// C++17 style if with initializer
			ts->GetToken();
			scope->Add(tmp.Detach());
			tmp = ParseExpression(depth+1);
			LETHE_RET_FALSE(tmp);
		}

		if (extra)
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));

		UniquePtr<AstNode> body = ParseStatement(depth+1);
		LETHE_RET_FALSE(body);
		UniquePtr<AstNode> elseBody;

		// check for else
		if (ts->PeekToken().type == TOK_KEY_ELSE)
		{
			ts->ConsumeToken();
			elseBody = ParseStatement(depth+1);
			LETHE_RET_FALSE(elseBody);
		}

		// note: not consuming potential semicolon here!, already consumed by ParseStatement
		res->Add(tmp.Detach());
		res->Add(body.Detach());

		if (!elseBody.IsEmpty())
			res->Add(elseBody.Detach());

		scope->Add(res.Detach());
		res = scope.Detach();
	}
	break;

	case TOK_KEY_SWITCH:
	{
		NamedScopeGuard ng(this, currentScope->Add(new NamedScope(NSCOPE_SWITCH)));
		UniquePtr<AstNode> scope = NewAstNode<AstBlock>(nt.location);

		res = NewAstNode<AstSwitch>(nt.location);
		ts->ConsumeToken();

		// a new sane form of switch: switch break(x) with automatic implied break
		// and local scope after each case
		bool switchBreak = ts->PeekToken().type == TOK_KEY_BREAK;

		if (switchBreak)
			ts->ConsumeToken();

		bool extra = ts->PeekToken().type == TOK_LBR;
		ts->ConsumeTokenIf(TOK_LBR);
		UniquePtr<AstNode> tmp = ParseVarDeclOrExpr(depth+1, 1);
		LETHE_RET_FALSE(tmp);

		if (tmp->type == AST_VAR_DECL_LIST && ts->PeekToken().type == TOK_SEMICOLON)
		{
			// C++17 style switch with initializer
			ts->GetToken();
			scope->Add(tmp.Detach());
			tmp = ParseExpression(depth+1);
			LETHE_RET_FALSE(tmp);
		}

		if (extra)
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));

		LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_LBLOCK, "expected `{`"));
		UniquePtr<AstNode> body = ParseSwitchBody(depth+1, switchBreak);
		// note: ParseSwitchBody consumes final `}`
		LETHE_RET_FALSE(body);
		// note: not consuming potential semicolon here!, already consumed by ParseStatement
		res->Add(tmp.Detach());
		res->Add(body.Detach());

		scope->Add(res.Detach());
		res = scope.Detach();
	}
	break;

	case TOK_KEY_GOTO:
	{
		ts->ConsumeToken();
		const auto &target = ts->GetToken();
		LETHE_RET_FALSE(ExpectPrev(target.type == TOK_IDENT, "identifier expected"));

		res = NewAstText<AstGoto>(target.text, nt.location);
		ts->ConsumeTokenIf(TOK_SEMICOLON);
	}
	break;

	case TOK_LBLOCK:
		res = ParseBlock(depth+1);
		break;

	case TOK_SEMICOLON:
		// FIXME: force resolved
		res = NewAstNode<AstConstant>(AST_EMPTY, nt.location);
		ts->ConsumeToken();
		break;

	case TOK_KEY_STATIC_ASSERT:
	{
		AstNode *sa = ParseStaticAssert(depth+1);
		LETHE_RET_FALSE(sa);
		res = sa;
		ts->ConsumeTokenIf(TOK_SEMICOLON);
		break;
	}

	case TOK_STRING:
	{
		// this is non-niggerlicious HolyC's shortcut printf (RIP Terry A. Davis)
		UniquePtr<AstNode> tmp = NewAstNode<AstCall>(ts->GetTokenLocation());
		tmp->Add(NewAstText<AstSymbol>("printf", ts->GetTokenLocation()));

		while(true)
		{
			auto *arg = ParseAssignExpression(depth+1);
			LETHE_RET_FALSE(arg);
			tmp->Add(arg);

			if (ts->PeekToken().type != TOK_COMMA)
				break;

			ts->ConsumeToken();
		}

		res = tmp.Detach();
		ts->ConsumeTokenIf(TOK_SEMICOLON);
		break;
	}

	default:
	{
		UniquePtr<AstNode> expr = ParseExpression(depth+1);
		LETHE_RET_FALSE(expr);
		ts->ConsumeTokenIf(TOK_SEMICOLON);

		res = NewAstNode<AstExpr>(expr->location);
		res->Add(expr.Detach());
	}
	}

	return res.Detach();
}

AstNode *Compiler::ParseBlock(Int depth)
{
	return ParseBlockInternal(depth);
}

AstNode *Compiler::ParseVirtualBlock(Int depth)
{
	return ParseBlockInternal(depth, false, true, false, nullptr, false, true);
}

AstNode *Compiler::ParseSwitchBreakBlock(Int depth)
{
	return ParseBlockInternal(depth+1, false, true, false, nullptr, true);
}

AstNode *Compiler::ParseFuncBlock(Int depth, bool isState, const String *fname)
{
	return ParseBlockInternal(depth+1, true, false, isState, fname);
}

AstNode *Compiler::ParseBlockInternal(Int depth, bool isFunc, bool noCheck, bool isStateFunc,
	const String *fname, bool isSwitchBreak, bool isVirtualBlock)
{
	LETHE_RET_FALSE(CheckDepth(depth));

	NamedScopeGuard nsg(this, currentScope->Add(new NamedScope(isFunc ? NSCOPE_FUNCTION : NSCOPE_LOCAL)));
	currentScope->needExtraScope = false;
	currentScope->virtualScope = isVirtualBlock;

	if (fname)
		currentScope->name = *fname;

	const Token &t = noCheck ? ts->PeekToken() : ts->GetToken();
	LETHE_ASSERT(noCheck || t.type == TOK_LBLOCK);
	UniquePtr<AstNode> res;

	if (!isFunc)
		res = NewAstNode<AstBlock>(t.location);
	else
	{
		res = NewAstNode<AstFuncBody>(t.location);

		if (isStateFunc)
		{
			auto *deferNode = NewAstNode<AstDefer>(t.location);
			auto *exprNode = NewAstNode<AstExpr>(t.location);
			auto *callNode = NewAstNode<AstCall>(t.location);
			auto *identNode = NewAstText<AstSymbol>("end_state", t.location);

			// fname is fully qualified now, so only use the last part after "::"
			auto rbeg = fname->ReverseFind(':');
			auto *beg = fname->Ansi();

			if (rbeg >= 0)
				beg += (size_t)rbeg+1;

			auto *argNode = NewAstText<AstSymbol>(beg, t.location);

			callNode->Add(identNode);
			callNode->Add(argNode);
			exprNode->Add(callNode);
			exprNode->qualifiers |= AST_Q_NOSTATEBREAK;
			deferNode->Add(exprNode);

			currentScope->deferred.Add(exprNode);

			res->Add(deferNode);
		}
	}

	for (;;)
	{
		const Token &nt = ts->PeekToken();

		if (nt.type == TOK_RBLOCK)
		{
			AstStaticCast<AstBlock *>(res.Get())->endOfBlockLocation = nt.location;

			if (!isSwitchBreak)
				ts->ConsumeToken();
			break;
		}

		switch(nt.type)
		{
		case TOK_KEY_CASE:
		case TOK_KEY_DEFAULT:
			AstStaticCast<AstBlock *>(res.Get())->endOfBlockLocation = nt.location;
			return res.Detach();

		case TOK_SHARP:
		{
			auto tokLine = nt.location.line;
			ts->ConsumeToken();
			LETHE_RET_FALSE(ParseDirective(tokLine));
			continue;
		}

		case TOK_KEY_STATIC_ASSERT:
		{
			AstNode *sa = ParseStaticAssert(depth+1);
			LETHE_RET_FALSE(sa);
			res->Add(sa);
			continue;
		}

		case TOK_KEY_TYPEDEF:
		case TOK_KEY_USING:
		{
			auto *tmp = nt.type == TOK_KEY_TYPEDEF ? ParseTypeDef(depth+1) : ParseUsing(depth+1);
			LETHE_RET_FALSE(tmp);
			res->Add(tmp);
			continue;
		}

		case TOK_KEY_MACRO:
			LETHE_RET_FALSE(ParseMacro(depth+1));
			continue;

		case TOK_IDENT:
			// this stupid and potentially slow check is necessary to detect labels
			// FIXME: better!
			// should use PeekNextToken() or PeekToken(1)
			ts->ConsumeToken();

			if (ts->PeekToken().type != TOK_COLON)
				ts->UngetToken();
			else
			{
				// it's a label!
				ts->ConsumeToken();
				UniquePtr<AstText> label = NewAstText<AstLabel>(nt.text, nt.location);
				LETHE_RET_FALSE(AddScopeLabel(label->text, label));
				label->flags |= AST_F_RESOLVED;
				res->Add(label.Detach());
				continue;
			}
			break;

		case TOK_KEY_DEFER:
		{
			ts->ConsumeToken();
			UniquePtr<AstNode> def = NewAstNode<AstDefer>(nt.location);
			AstNode *tmp = ParseStatement(depth + 1);
			LETHE_RET_FALSE(tmp);
			def->Add(tmp);
			currentScope->deferred.Add(tmp);
			res->Add(def.Detach());
		}

		continue;

		case TOK_KEY_IF:
		case TOK_KEY_GOTO:
		case TOK_KEY_FOR:
		case TOK_KEY_WHILE:
		case TOK_KEY_DO:
		case TOK_KEY_BREAK:
		case TOK_KEY_CONTINUE:
		case TOK_KEY_RETURN:
		case TOK_KEY_SWITCH:
		case TOK_LBLOCK:
		{
			currentScope->needExtraScope = true;
			AstNode *tmp = ParseStatement(depth+1);
			LETHE_RET_FALSE(tmp);
			res->Add(tmp);
			continue;
		}
		break;

		case TOK_SEMICOLON:
			ts->ConsumeToken();
			continue;

		default:
			;
		}

		AstNode *tmp;

		if (ts->PeekToken().type == TOK_STRING)
		{
			// this is non-niggerlicious HolyC's shortcut printf (RIP Terry A. Davis)
			tmp = ParseStatement(depth+1);
		}
		else
		{
			tmp = ParseVarDeclOrExpr(depth+1, 0, 0);
		}

		LETHE_RET_FALSE(tmp);

		// how can this be a block? if needExtraScope was 1 when parsing varDecl
		if (tmp->type != AST_VAR_DECL_LIST && tmp->type != AST_BLOCK)
		{
			// must be an expression!
			AstNode *tmp2 = NewAstNode<AstExpr>(tmp->location);
			tmp2->Add(tmp);
			tmp = tmp2;
			currentScope->needExtraScope = true;
		}

		res->Add(tmp);
	}

	return res.Detach();
}


}
