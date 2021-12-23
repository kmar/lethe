#include "AstAssignOp.h"
#include "../CodeGenTables.h"
#include "../NamedScope.h"
#include "../AstSymbol.h"
#include <Lethe/Script/Ast/Function/AstCall.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstAssignOp

const char *AstAssignOp::GetOpName() const
{
	const char *opName = nullptr;

	switch (type)
	{
	case AST_OP_ADD_ASSIGN:
		opName = "+=";
		break;

	case AST_OP_SUB_ASSIGN:
		opName = "-=";
		break;

	case AST_OP_MUL_ASSIGN:
		opName = "*=";
		break;

	case AST_OP_DIV_ASSIGN:
		opName = "/=";
		break;

	case AST_OP_MOD_ASSIGN:
		opName = "%=";
		break;

	case AST_OP_AND_ASSIGN:
		opName = "&=";
		break;

	case AST_OP_OR_ASSIGN:
		opName = "|=";
		break;

	case AST_OP_XOR_ASSIGN:
		opName = "^=";
		break;

	case AST_OP_SHL_ASSIGN:
		opName = "<<=";
		break;

	case AST_OP_SHR_ASSIGN:
		opName = ">>=";
		break;

	default:
		;
	}

	return opName;
}

Int AstAssignOp::CodeGenOperator(CompiledProgram &p)
{
	auto left = nodes[IDX_LEFT]->GetTypeDesc(p);
	auto right = nodes[IDX_RIGHT]->GetTypeDesc(p);

	AstNode *op = nullptr;

	auto opName = GetOpName();

	if (!opName)
		return -1;

	LETHE_ASSERT(left.IsStruct());

	auto scope = left.GetType().structScopeRef;
	LETHE_ASSERT(scope);
	op = scope->FindOperator(p, opName, left, right);

	if (!op)
		return p.Error(this, "matching operator not found!");

	return AstCall::CallOperator(2, p, this, op);
}

bool AstAssignOp::BeginCodegen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::BeginCodegen(p));

	auto leftType = nodes[IDX_LEFT]->GetTypeDesc(p);

	if (leftType.IsSwitchable())
		LETHE_RET_FALSE(nodes[IDX_RIGHT]->ConvertConstTo(leftType.GetTypeEnum(), p));

	return true;
}

bool AstAssignOp::CodeGen(CompiledProgram &p)
{
	// FIXME: currently a bit hacky!
	LETHE_ASSERT(type != AST_OP_ASSIGN);

	auto *rnode = nodes[IDX_LEFT];

	if (rnode->type == AST_OP_DOT)
		rnode = rnode->nodes[1];

	if (rnode->qualifiers & AST_Q_PROPERTY)
	{
		AstBinaryOp binop(type, location);
		binop.nodes = nodes;

		auto res = AstSymbol::CallPropertySetter(p, nodes[0], &binop);

		binop.nodes.Clear();

		return res;
	}

	auto leftType = nodes[IDX_LEFT]->GetTypeDesc(p);

	if (leftType.IsStruct())
	{
		auto res = CodeGenOperator(p);

		if (res >= 0)
			return res > 0;
	}

	bool pop = ShouldPop();

	if (type == AST_OP_SWAP)
	{
		auto rightType = nodes[IDX_RIGHT]->GetTypeDesc(p);

		if (leftType.ref != rightType.ref)
			return p.Error(this, "invalid type for swap operator");

		LETHE_RET_FALSE(nodes[IDX_LEFT]->CodeGenRef(p));
		LETHE_RET_FALSE(nodes[IDX_RIGHT]->CodeGenRef(p));

		leftType.RemoveReference();
		p.EmitU24(OPC_PSWAP, leftType.GetSize());

		p.PopStackType(1);

		if (pop)
			p.PopStackType(1);
		else
			return p.Error(this, "swap operator must be a standalone expression");

		return true;
	}

	if (type == AST_OP_ADD_ASSIGN && leftType.GetTypeEnum() == DT_ARRAY_REF)
	{
		LETHE_RET_FALSE(nodes[IDX_LEFT]->CodeGenRef(p));
		LETHE_RET_FALSE(nodes[IDX_RIGHT]->CodeGen(p));
		LETHE_RET_FALSE(p.EmitConv(nodes[IDX_RIGHT], nodes[IDX_RIGHT]->GetTypeDesc(p), QDataType::MakeConstType(p.elemTypes[DT_UINT])));
		p.EmitIntConst(leftType.GetType().elemType.GetSize());
		p.EmitI24(OPC_BCALL, BUILTIN_SLICEFWD_INPLACE);
		p.PopStackType(true);

		if (pop)
		{
			p.PopStackType(true);
			p.EmitI24(OPC_POP, 1);
		}

		return true;
	}

	if (type == AST_OP_ADD_ASSIGN && leftType.GetTypeEnum() == DT_STRING)
	{
		// special case for strings because current approach (load/op/store) is very slow!
		LETHE_RET_FALSE(nodes[IDX_LEFT]->CodeGenRef(p));
		LETHE_RET_FALSE(nodes[IDX_RIGHT]->CodeGen(p));
		LETHE_RET_FALSE(p.EmitConv(nodes[IDX_RIGHT], nodes[IDX_RIGHT]->GetTypeDesc(p), leftType));
		p.EmitI24(OPC_BCALL, pop ? BUILTIN_PSTRADD_ASSIGN : BUILTIN_PSTRADD_ASSIGN_LOAD);
		p.PopStackType(1);
		p.PopStackType(1);

		if (!pop)
		{
			leftType.RemoveReference();
			p.PushStackType(leftType);
		}

		return true;
	}

	// FIXME: this only works for up to 4-byte numbers!
	bool useRef;// = nodes[IDX_LEFT]->type != AST_IDENT && (leftType.IsNumber() || leftType.GetTypeEnum() == DT_STRING);
	// FIXME! hmm, this opt seems to cripple pokey player (Synthy Gambol -- something is out of sync)
	// => disabling for now...
	useRef = false;

	LETHE_RET_FALSE(AstBinaryOp::CodeGenCommon(p, useRef));
	LETHE_RET_FALSE(CodeGenCommon(p, true, useRef));

	if (useRef)
	{
		if (!pop)
			p.Emit((leftType.IsNumber() ? OPC_LMOVE32 : OPC_LMOVEPTR) + 1*256 + 0*65536);

		p.EmitI24(OPC_POP, 1);
	}

	return true;
}

bool AstAssignOp::CodeGenRef(CompiledProgram &p, bool allowConst, bool)
{
	// not supported if parent is binary_assign
	if (parent->type == AST_OP_ASSIGN || parent->type == AST_OP_SWAP || parent->type == AST_CALL)
		return p.Error(this, "unsupported construct");

	return nodes[0]->CodeGenRef(p, allowConst);
}

bool AstAssignOp::CodeGenPrepareAssign(CompiledProgram &p, QDataType &rhs, bool &structOnStack, Int &ssize)
{
	// should generate refptr now...
	rhs = p.exprStack.Back();

	structOnStack = !rhs.IsReference() && (rhs.IsStruct() || rhs.GetTypeEnum() == DT_DYNAMIC_ARRAY);
	ssize = 0;

	if (structOnStack)
	{
		ssize = (rhs.GetSize() + Stack::WORD_SIZE-1) / Stack::WORD_SIZE;

		p.Emit(OPC_LPUSHADR);
		QDataType tmp = rhs;
		tmp.qualifiers |= AST_Q_REFERENCE;
		p.PushStackType(tmp);
	}

	return 1;
}

bool AstAssignOp::CodeGenDoAssign(AstNode *n, CompiledProgram &p, const QDataType &dst, const QDataType &rhs, bool pop, bool structOnStack, Int ssize)
{
	if (structOnStack)
	{
		QDataType tmp = p.exprStack.Back();
		p.PopStackType(1);
		p.PopStackType(1);
		p.PushStackType(tmp);
	}

	LETHE_ASSERT(p.exprStack.GetSize() >= 1);

	QDataType lv = dst;
	lv.RemoveReference();
	DataTypeEnum dte = lv.GetTypeEnum();

	if (rhs.GetTypeEnum() == DT_DYNAMIC_ARRAY && !rhs.IsReference())
	{
		// note: this hack makes sure we handle copying returned dynarrays properly
		dte = DT_STRUCT;
	}

	// cannot assign static arrays, plain class shouldn't happen here and neither does null, everything else is assignable
	if (dte == DT_STATIC_ARRAY || dte == DT_NULL || dte == DT_CLASS)
		return p.Error(n, "cannot assign to this type");

	if (dte == DT_DYNAMIC_ARRAY)
	{
		Int toPop = 1 + pop;

		if (!rhs.IsReference())
		{
			LETHE_ASSERT(rhs.GetTypeEnum() == DT_ARRAY_REF);
			p.EmitI24(OPC_LPUSHADR, 1);
			p.Emit(OPC_LSWAPPTR);
			toPop += 1 + pop;
		}

		p.EmitBackwardJump(OPC_CALL, lv.GetType().funAssign);
		p.EmitU24(OPC_POP, toPop);
	}
	else if (dte == DT_ARRAY_REF || dte == DT_DELEGATE)
	{
		if (!rhs.IsReference())
			p.EmitU24(OPC_LPUSHADR, 1);

		p.EmitU24(OPC_PCOPY_REV, lv.GetSize());

		if (pop)
			p.EmitU24(OPC_POP, rhs.IsReference() ? 1 : 2);
	}
	else if (dte == DT_STRUCT)
	{
		if (rhs.IsReference())
		{
			// copying ...
			if (lv.HasDtor())
			{
				p.EmitBackwardJump(OPC_CALL, lv.GetType().funAssign);
				p.EmitU24(OPC_POP, 1+pop);
			}
			else
				p.EmitU24(pop ? OPC_PCOPY : OPC_PCOPY_NP, lv.GetSize());
		}
		else
		{
			// if pop then just MOVE
			if (pop)
			{
				if (lv.HasDtor())
					p.EmitBackwardJump(OPC_CALL, lv.GetType().funDtor);

				p.EmitU24(OPC_PCOPY, lv.GetSize());
				// but we have to clean up stack!
				p.EmitU24(OPC_POP, ssize);
			}
			else
			{
				// this one is tricky!
				if (lv.HasDtor())
				{
					p.EmitBackwardJump(OPC_CALL, lv.GetType().funAssign);
					p.EmitI24(OPC_POP, 2);
				}
				else
					p.EmitU24(OPC_PCOPY, lv.GetSize());
			}
		}
	}
	else if (dte == DT_STRING)
	{
		// special handling for string
		// TODO: byte-uopt
		p.Emit(OPC_PUSH_ICONST);
		p.EmitI24(OPC_BCALL, pop ? BUILTIN_PSTRSTORE_IMM : BUILTIN_PSTRSTORE_IMM_NP);
	}
	else if (dte == DT_STRONG_PTR || dte == DT_WEAK_PTR)
	{
		p.EmitBackwardJump(OPC_CALL, lv.GetType().funAssign);
		p.PopStackType(1);
		p.EmitI24(OPC_POP, 1);

		if (pop)
		{
			p.PopStackType();
			p.EmitI24(OPC_POP, 1);
		}

		return true;
	}
	else
	{
		// perform simple bytecode u-opts now
		Int lins = p.instructions.Back();
		UInt linsType = (UInt)lins & 255u;

		if (linsType == OPC_LPUSHADR)
		{
			p.instructions.Pop();

			// helps JIT a bit
			if (p.GetJitFriendly() && (lv.qualifiers & AST_Q_LOCAL_INT) && dte <= DT_USHORT)
				dte = DT_INT;

			if (lv.IsMethodPtr())
				return p.Error(n, "cannot store method");

			p.EmitU24(opcodeLocalStore[!pop][dte], Int((UInt)lins >> 8));
		}
		else if (linsType == OPC_GLOADADR)
		{
			p.instructions.Pop();

			if (lv.IsMethodPtr())
				return p.Error(n, "cannot store method");

			p.EmitU24(opcodeGlobalStore[!pop][dte], Int((UInt)lins >> 8));
		}
		else
		{
			if (lv.IsMethodPtr())
				return p.Error(n, "cannot store method");

			p.Emit(opcodeRefStore[!pop][dte]);
		}

		LETHE_ASSERT((p.instructions.Back() & 255u) != OPC_HALT);
	}

	p.PopStackType(1);

	if (pop)
		p.PopStackType(1);

	return true;
}

bool AstAssignOp::CodeGenCommon(CompiledProgram &p, bool needConv, bool asRef, bool allowConst)
{
	if (type == AST_OP_ASSIGN && nodes[0]->type == AST_THIS)
	{
		auto thisScope = scopeRef->FindThis();

		LETHE_ASSERT(thisScope);

		if (thisScope->type == NSCOPE_CLASS)
			return p.Error(nodes[0], "cannot assign to this inside class");
	}

	auto src = nodes[needConv]->GetTypeDesc(p);
	const QDataType &dst = nodes[0]->GetTypeDesc(p);

	if (needConv)
		src = p.exprStack.Back();

	if (dst.qualifiers & AST_Q_NOCOPY)
		return p.Error(this, "cannot assign to nocopy variable");

	if (!allowConst && dst.IsConst() && !(qualifiers & AST_Q_CAN_MODIFY_CONSTANT))
		return p.Error(this, "cannot modify a constant");

	auto ste = src.GetTypeEnum();
	auto dte = dst.GetTypeEnum();

	if (dte != ste && !(dte == DT_DYNAMIC_ARRAY && ste == DT_ARRAY_REF))
	{
		// need conversion...
		LETHE_RET_FALSE(p.EmitConv(this, src, dst));

		auto dstt = p.exprStack.Back();
		p.PopStackType(1);

		// only pointer types can be grabbed from expr stack this way
		if (!dst.IsPointer())
			dstt = dst;

		// need to remove reference (because of array refs)
		dstt.RemoveReference();

		p.PushStackType(dstt);
	}

	if (!dst.CanAssign(src, true))
		return p.Error(this, "incompatible types");

	bool pop = ShouldPop();

	QDataType rhs;
	Int ssize;
	bool structOnStack;

	const auto argSize = p.exprStack.Back().GetSize();

	LETHE_RET_FALSE(CodeGenPrepareAssign(p, rhs, structOnStack, ssize));

	if (asRef)
	{
		auto qdt = dst;
		qdt.qualifiers |= AST_Q_REFERENCE;
		p.EmitI24(OPC_LPUSHPTR, (argSize + Stack::WORD_SIZE-1) / Stack::WORD_SIZE + structOnStack);
		p.PushStackType(qdt);
		structOnStack = true;
	}
	else
		LETHE_RET_FALSE(nodes[0]->CodeGenRef(p, allowConst));

	return CodeGenDoAssign(nodes[IDX_LEFT], p, dst, rhs, pop, structOnStack, ssize);
}


}
