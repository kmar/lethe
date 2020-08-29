#include "Compiler.h"

#include "AstIncludes.h"

namespace lethe
{

AstNode *Compiler::ParsePriority2Operators(Int depth, UniquePtr<AstNode> &first)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res;
	Swap(res, first);
	bool done = 0;

	while (!done)
	{
		const Token &t = ts->PeekToken();

		switch(t.type)
		{
		case TOK_DOT:
		{
			UniquePtr<AstNode> nres = NewAstNode<AstDotOp>(t.location);
			ts->ConsumeToken();
			AstNode *tmp = ParsePriority2(depth+1, 1);
			LETHE_RET_FALSE(tmp);
			nres->nodes.Reserve(2);
			nres->Add(res.Detach());
			nres->Add(tmp);
			Swap(res, nres);
		}
		break;

		case TOK_LARR:
			// array subscript
		{
			UniquePtr<AstNode> nres = NewAstNode<AstSubscriptOp>(t.location);
			ts->ConsumeToken();
			AstNode *tmp = ParseExpression(depth+1);
			LETHE_RET_FALSE(tmp);
			nres->nodes.Reserve(2);
			nres->Add(res.Detach());
			nres->Add(tmp);
			Swap(res, nres);
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_RARR, "expected `]`"));
		}
		break;

		case TOK_INC:
		case TOK_DEC:
		{
			AstNode *tmp = NewAstNode<AstUnaryPostOp>(t.type == TOK_INC ? AST_UOP_POSTINC : AST_UOP_POSTDEC, t.location);
			ts->ConsumeToken();
			tmp->Add(res.Detach());
			res = tmp;
		}
		break;

		case TOK_LBR:
			// function call...
		{
			UniquePtr<AstNode> fcall = NewAstNode<AstCall>(t.location);
			LETHE_RET_FALSE(fcall);
			ts->ConsumeToken();
			fcall->Add(res.Detach());

			if (ts->PeekToken().type == TOK_RBR)
			{
				ts->ConsumeToken();
				res = fcall;
				break;
			}

			TokenType tt = TOK_INVALID;

			for (;;)
			{
				AstNode *tmp = ParseAssignExpression(depth+1);
				LETHE_RET_FALSE(tmp);
				fcall->Add(tmp);
				tt = ts->PeekToken().type;

				if (tt != TOK_COMMA)
					break;

				ts->ConsumeToken();
			}

			LETHE_RET_FALSE(ExpectPrev(tt == TOK_RBR, "expected `)`"));
			ts->ConsumeToken();
			res = fcall;
		}
		break;

		default:
			;
			done = 1;
		}
	}

	return res.Detach();
}

// priority 2 group
AstNode *Compiler::ParsePriority2(Int depth, bool termOnly)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res;
	const Token &t = ts->PeekToken();

	switch(t.type)
	{
	case TOK_IDENT:
	case TOK_DOUBLE_COLON:
	{
		UniquePtr<AstNode> tmp = ParseScopeResolution(depth+1);
		LETHE_RET_FALSE(tmp);
		UniquePtr<AstNode> tmp2;

		// try to parse struct literal
		if (ts->PeekToken().type == TOK_LBLOCK)
		{
			auto il = ParseInitializerList(depth+1);
			LETHE_RET_FALSE(il);
			tmp2 = NewAstNode<AstStructLiteral>(tmp->location);
			tmp2->Add(tmp.Detach());

			if (il->nodes.IsEmpty())
				il->flags |= AST_F_RESOLVED;

			tmp2->Add(il);
			Swap(tmp, tmp2);
		}

		// ok we now have tmp; need to bind
		if (!res)
			res = tmp.Detach();
		else
		{
			// bind...
			res->Add(tmp.Detach());
		}
	}
	break;

	case TOK_KEY_THIS:
	case TOK_KEY_SUPER:
		// just create, scope resolution not possible
	{
		ts->ConsumeToken();
		AstNode *tmp;

		if (t.type == TOK_KEY_THIS)
			tmp = NewAstNode<AstThis>(t.location);
		else
			tmp = NewAstNode<AstNode>(AST_SUPER, t.location);

		LETHE_RET_FALSE(tmp);

		// ok we now have tmp; need to bind
		if (!res)
			res = tmp;
		else
		{
			// bind...
			res->Add(tmp);
		}
	}
	break;

	default:
		if (res.IsEmpty())
		{
			Error("unexpected token");
			return res;
		}
	}

	if (!termOnly)
	{
		switch(ts->PeekToken().type)
		{
		case TOK_DOT:
		case TOK_LARR:
		case TOK_LBR:
		case TOK_INC:
		case TOK_DEC:
			res = ParsePriority2Operators(depth + 1, res);
			break;

		default:
			;
		}
	}

	return res.Detach();
}

AstNode *Compiler::ParseUnaryExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res;
	AstNode *bottom = nullptr;

	for(;;)
	{
		const Token &t = ts->PeekToken();

		TokenType castExpect = TOK_INVALID;

		switch(t.type)
		{
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
		{
			if (bottom && bottom->type == AST_SIZEOF)
			{
				auto *tmp = ParseType(depth+1);
				LETHE_RET_FALSE(tmp);
				bottom->Add(tmp);
				bottom = tmp;
			}
			else
			{
				if (res.IsEmpty())
					LETHE_RET_FALSE(Expect(false, "unexpected token"));
			}

			break;
		}

		case TOK_LBR:
		{
			ts->ConsumeToken();
			UniquePtr<AstNode> tmp = ParseExpression(depth+1);
			LETHE_RET_FALSE(tmp && ExpectPrev(ts->GetToken().type == TOK_RBR, "expected `)`"));
			// this is necessary for right-assoc ops
			tmp->flags |= AST_F_SUBEXPR;

			tmp = ParsePriority2Operators(depth+1, tmp);
			LETHE_RET_FALSE(tmp);

			bool cont = ts->PeekToken().type == TOK_LBR;

			if (cont)
			{
				// convert this into cast_or_call
				AstNode *castOrCall = NewAstNode<AstNode>(AST_CAST_OR_CALL, tmp->location);
				castOrCall->Add(tmp.Detach());
				tmp = castOrCall;
			}

			AstNode *nbottom = tmp.Detach();

			if (bottom)
				bottom->Add(nbottom);
			else
			{
				LETHE_ASSERT(res.IsEmpty());
				res = nbottom;
			}

			bottom = nbottom;

			if (!cont)
				return res.Detach();
		}
		break;

		case TOK_KEY_THIS:
		case TOK_IDENT:
		case TOK_DOUBLE_COLON:
		{
			AstNode *tmp = ParsePriority2(depth+1);
			LETHE_RET_FALSE(tmp);

			if (bottom)
				bottom->Add(tmp);
			else
			{
				LETHE_ASSERT(res.IsEmpty());
				res = tmp;
			}

			return res.Detach();
		}
		break;

		case TOK_KEY_NULL:
		{
			AstNode *nptr = NewAstNode<AstConstNull>(t.location);
			LETHE_RET_FALSE(nptr);
			ts->ConsumeToken();

			if (bottom)
				bottom->Add(nptr);
			else
			{
				LETHE_ASSERT(res.IsEmpty());
				res = nptr;
			}

			return res.Detach();
		}
		break;

		case TOK_KEY_TRUE:
		case TOK_KEY_FALSE:
		case TOK_CHAR:
		case TOK_ULONG:
		case TOK_DOUBLE:
		{
			ts->ConsumeToken();
			AstNode *num = CreateConstNumber(t);
			LETHE_RET_FALSE(num);

			if (bottom)
				bottom->Add(num);
			else
			{
				LETHE_ASSERT(res.IsEmpty());
				res = num;
			}

			return res.Detach();
		}
		break;

		case TOK_NAME:
		case TOK_STRING:
		{
			AstNode *lit = t.type == TOK_NAME ?
						   AstStaticCast<AstText *>(NewAstTextRef<AstConstName>(StringRef(t.text, t.length), t.location)) :
						   AstStaticCast<AstText *>(NewAstTextRef<AstConstString>(StringRef(t.text, t.length), t.location));
			LETHE_RET_FALSE(lit);
			ts->ConsumeToken();

			// concatenate sequence of strings; ideally it would be done in lexer, but we do it here, hopefully making things easier
			if (t.type == TOK_STRING)
			{
				for (;;)
				{
					const Token &st = ts->PeekToken();

					if (st.type != TOK_STRING)
						break;

					ts->ConsumeToken();
					// FIXME: should be able to add string refs!
					AstStaticCast<AstText *>(lit)->text += StringRef(st.text, st.length).ToString();
				}
			}

			if (ts->PeekToken().type == TOK_LARR)
			{
				UniquePtr<AstNode> ulit = lit;
				AstNode *tmp = ParsePriority2Operators(depth + 1, ulit);
				LETHE_RET_FALSE(tmp);
				ulit.Detach();
				lit = tmp;
			}

			if (bottom)
				bottom->Add(lit);
			else
			{
				LETHE_ASSERT(res.IsEmpty());
				res = lit;
			}

			return res.Detach();
		}
		break;

		case TOK_MINUS:
		case TOK_PLUS:
		case TOK_LNOT:
		case TOK_NOT:
		case TOK_INC:
		case TOK_DEC:
		case TOK_KEY_NEW:
		case TOK_KEY_SIZEOF:
		case TOK_KEY_OFFSETOF:
		case TOK_KEY_ALIGNOF:
		case TOK_KEY_TYPEID:
		{
			AstNode *tmp = 0;

			switch(t.type)
			{
			case TOK_MINUS:
				tmp = NewAstNode<AstUnaryMinus>(t.location);
				break;

			case TOK_PLUS:
				tmp = NewAstNode<AstUnaryPlus>(t.location);
				break;

			case TOK_LNOT:
				tmp = NewAstNode<AstUnaryLNot>(t.location);
				break;

			case TOK_NOT:
				tmp = NewAstNode<AstUnaryNot>(t.location);
				break;

			case TOK_INC:
				tmp = NewAstNode<AstUnaryPreOp>(AST_UOP_PREINC, t.location);
				break;

			case TOK_DEC:
				tmp = NewAstNode<AstUnaryPreOp>(AST_UOP_PREDEC, t.location);
				break;

			case TOK_KEY_NEW:
				tmp = NewAstNode<AstUnaryNew>(t.location);
				break;

			case TOK_KEY_SIZEOF:
				tmp = NewAstNode<AstSizeOf>(AST_SIZEOF, t.location);
				break;

			case TOK_KEY_ALIGNOF:
				tmp = NewAstNode<AstSizeOf>(AST_ALIGNOF, t.location);
				break;

			case TOK_KEY_OFFSETOF:
				tmp = NewAstNode<AstSizeOf>(AST_OFFSETOF, t.location);
				break;

			case TOK_KEY_TYPEID:
				tmp = NewAstNode<AstSizeOf>(AST_TYPEID, t.location);
				break;

			default:
				;
			}

			LETHE_RET_FALSE(tmp);
			ts->ConsumeToken();

			if (bottom)
				bottom->Add(tmp);
			else
				res = tmp;

			bottom = tmp;
		}
		break;

		case TOK_KEY_CAST:
			// cast<type> or cast type or cast(type)
			ts->ConsumeToken();

			switch (ts->PeekToken().type)
			{
			case TOK_LT:
				castExpect = TOK_GT;
				ts->ConsumeToken();
				break;

			case TOK_LBR:
				castExpect = TOK_RBR;
				ts->ConsumeToken();
				break;

			default:
				;
			}

			// cast...
			{
				UniquePtr<AstNode> tmp = NewAstNode<AstCast>(t.location);
				AstNode *tmpType;
				tmpType = ParseType(depth+1);
				LETHE_RET_FALSE(tmpType);
				auto q = tmpType->qualifiers;
				tmp->Add(tmpType);

				if (q & AST_Q_REFERENCE)
				{
					Expect(false, "cannot cast to reference type");
					return nullptr;
				}

				if (bottom)
					bottom->Add(tmp.Get());
				else
					res = tmp.Get();

				bottom = tmp.Detach();

				if (castExpect != TOK_INVALID)
					LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == castExpect, "expected cast close token"));
			}
			break;

		default:
			if (res.IsEmpty())
				LETHE_RET_FALSE(Expect(false, "unexpected token"));

			if (bottom && bottom->IsUnaryOp())
			{
				if (bottom->nodes.IsEmpty() || (bottom->type == AST_CAST && bottom->nodes.GetSize() < 2))
					LETHE_RET_FALSE(Expect(0, "expected unary op target"));
			}

			return res.Detach();
		}
	}
}

AstNode *Compiler::ParseMultExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseUnaryExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		UniquePtr<AstNode> nres;

		switch(t.type)
		{
		case TOK_MUL:
			nres = NewAstNode<AstBinaryOp>(AST_OP_MUL, t.location);
			break;

		case TOK_DIV:
			nres = NewAstNode<AstBinaryOp>(AST_OP_DIV, t.location);
			break;

		case TOK_MOD:
			nres = NewAstNode<AstBinaryOp>(AST_OP_MOD, t.location);
			break;

		default:
			;
			return res.Detach();
		}

		ts->ConsumeToken();
		AstNode *tmp = ParseUnaryExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseAddExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseMultExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		UniquePtr<AstNode> nres;

		switch(t.type)
		{
		case TOK_PLUS:
			nres = NewAstNode<AstBinaryOp>(AST_OP_ADD, t.location);
			break;

		case TOK_MINUS:
			nres = NewAstNode<AstBinaryOp>(AST_OP_SUB, t.location);
			break;

		default:
			;
			return res.Detach();
		}

		ts->ConsumeToken();
		AstNode *tmp = ParseMultExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseShiftExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseAddExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype;

		switch(t.type)
		{
		case TOK_SHL:
			ntype = AST_OP_SHL;
			break;

		case TOK_SHR:
			ntype = AST_OP_SHR;
			break;

		default:
			;
			return res.Detach();
		}

		UniquePtr<AstNode> nres = NewAstNode<AstBinaryOp>(ntype, t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseAddExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseLtGtExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseShiftExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype = AST_NONE;

		switch(t.type)
		{
		case TOK_LT:
			ntype = AST_OP_LT;
			break;

		case TOK_LEQ:
			ntype = AST_OP_LEQ;
			break;

		case TOK_GT:
			ntype = AST_OP_GT;
			break;

		case TOK_GEQ:
			ntype = AST_OP_GEQ;
			break;

		default:
			;
			return res.Detach();
		}

		UniquePtr<AstNode> nres = NewAstNode<AstBinaryOp>(ntype, t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseShiftExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseEqExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseLtGtExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype = AST_NONE;

		switch(t.type)
		{
		case TOK_EQ_EQ:
			ntype = AST_OP_EQ;
			break;

		case TOK_NOT_EQ:
			ntype = AST_OP_NEQ;
			break;

		default:
			;
			return res.Detach();
		}

		UniquePtr<AstNode> nres = NewAstNode<AstBinaryOp>(ntype, t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseLtGtExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseAndExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseEqExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		UniquePtr<AstNode> nres;

		switch(t.type)
		{
		case TOK_AND:
			nres = NewAstNode<AstBinaryOp>(AST_OP_AND, t.location);
			break;

		default:
			;
			return res.Detach();
		}

		ts->ConsumeToken();
		AstNode *tmp = ParseEqExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		res = nres;
	}
}

AstNode *Compiler::ParseXorExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseAndExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		UniquePtr<AstNode> nres;

		switch(t.type)
		{
		case TOK_XOR:
			nres = NewAstNode<AstBinaryOp>(AST_OP_XOR, t.location);
			break;

		default:
			;
			return res.Detach();
		}

		ts->ConsumeToken();
		AstNode *tmp = ParseAndExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseOrExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseXorExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		UniquePtr<AstNode> nres;

		switch(t.type)
		{
		case TOK_OR:
			nres = NewAstNode<AstBinaryOp>(AST_OP_OR, t.location);
			break;

		default:
			;
			return res.Detach();
		}

		ts->ConsumeToken();
		AstNode *tmp = ParseXorExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseLAndExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseOrExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype = AST_NONE;

		switch(t.type)
		{
		case TOK_LAND:
			ntype = AST_OP_LAND;
			break;

		default:
			;
			return res.Detach();
		}

		UniquePtr<AstNode> nres = NewAstNode<AstLazyBinaryOp>(ntype, t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseOrExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseLOrExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseLAndExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype = AST_NONE;

		switch(t.type)
		{
		case TOK_LOR:
			ntype = AST_OP_LOR;
			break;

		default:
			;
			return res.Detach();
		}

		UniquePtr<AstNode> nres = NewAstNode<AstLazyBinaryOp>(ntype, t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseLAndExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseAssignExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseLOrExpression(depth+1);
	LETHE_RET_FALSE(res);
	AstNode *target = res;

	for(;;)
	{
		const Token &t = ts->PeekToken();
		AstNodeType ntype = AST_NONE;
		UniquePtr<AstNode> op;

		switch(t.type)
		{
		case TOK_EQ:
			ntype = AST_OP_ASSIGN;
			op = NewAstNode<AstBinaryAssign>(t.location);
			break;

		case TOK_PLUS_EQ:
			ntype = AST_OP_ADD_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_MINUS_EQ:
			ntype = AST_OP_SUB_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_MUL_EQ:
			ntype = AST_OP_MUL_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_DIV_EQ:
			ntype = AST_OP_DIV_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_MOD_EQ:
			ntype = AST_OP_MOD_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_SHL_EQ:
			ntype = AST_OP_SHL_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_SHR_EQ:
			ntype = AST_OP_SHR_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_AND_EQ:
			ntype = AST_OP_AND_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_XOR_EQ:
			ntype = AST_OP_XOR_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_OR_EQ:
			ntype = AST_OP_OR_ASSIGN;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_SWAP:
			ntype = AST_OP_SWAP;
			op = NewAstNode<AstAssignOp>(ntype, t.location);
			break;

		case TOK_QUESTION:
			ntype = AST_OP_TERNARY;
			op = NewAstNode<AstTernaryOp>(t.location);
			break;

		default:
			;
			return res.Detach();
		}

		TokenLocation nloc = t.location;
		ts->ConsumeToken();
		UniquePtr<AstNode> tmp = ntype == AST_OP_TERNARY ? ParseExpression(depth+1) : ParseLOrExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		AstNode *tmp2 = 0;

		if (ntype == AST_OP_TERNARY)
		{
			LETHE_RET_FALSE(ExpectPrev(ts->GetToken().type == TOK_COLON, "expected `:`"));
			tmp2 = ParseLOrExpression(depth+1);

			if (!tmp2)
				return 0;
		}

		// these are right-associative!!!
		AstNode *nres = op.Detach();
		LETHE_ASSERT(nres);

		if (!(target->flags & AST_F_SUBEXPR) && target->IsRightAssocBinaryOp())
		{
			Int pos = target->nodes.GetSize()-1;
			LETHE_ASSERT(pos == 1 || pos == 2);
			AstNode *right = target->UnbindNode(pos);
			target->BindNode(pos, nres);
			nres->nodes.Reserve(2);
			nres->Add(right);
			nres->Add(tmp.Detach());
		}
		else
		{
			nres->nodes.Reserve(2);
			nres->Add(res.Detach());
			nres->Add(tmp.Detach());
			res = nres;
		}

		if (tmp2)
			nres->Add(tmp2);

		target = nres;
	}
}

AstNode *Compiler::ParseCommaExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	UniquePtr<AstNode> res = ParseAssignExpression(depth+1);
	LETHE_RET_FALSE(res);

	for(;;)
	{
		const Token &t = ts->PeekToken();

		if (t.type != TOK_COMMA)
			return res.Detach();

		UniquePtr<AstNode> nres = NewAstNode<AstCommaOp>(t.location);
		ts->ConsumeToken();
		AstNode *tmp = ParseAssignExpression(depth+1);
		LETHE_RET_FALSE(tmp);
		nres->nodes.Reserve(2);
		nres->Add(res.Detach());
		nres->Add(tmp);
		Swap(res, nres);
	}
}

AstNode *Compiler::ParseExpression(Int depth)
{
	LETHE_RET_FALSE(CheckDepth(depth));
	return ParseCommaExpression(depth+1);
}

}
