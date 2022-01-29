#include "AstUnaryOp.h"
#include "../NamedScope.h"
#include <Lethe/Script/Ast/Function/AstCall.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Types/AstTypeBool.h>
#include <Lethe/Script/Ast/Types/AstTypeInt.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Ast/AstText.h>

#include <Lethe/Script/Vm/Stack.h>

namespace lethe
{

// AstUnaryOp

bool AstUnaryOp::FoldConst(const CompiledProgram &p)
{
	bool res = Super::FoldConst(p);

	if (!nodes[0]->IsConstant())
		return res;

	LETHE_ASSERT(parent);
	const QDataType &dtn = nodes[0]->GetTypeDesc(p);

	switch(type)
	{
	case AST_UOP_PLUS:

		// always remove unary plus
		if (dtn.GetTypeEnum() > DT_DOUBLE)
			return res;

		nodes[0]->parent = parent;
		LETHE_VERIFY(parent->ReplaceChild(this, nodes[0]));
		nodes.Clear();
		delete this;
		return 1;
		break;

	case AST_UOP_MINUS:
		if (dtn.GetTypeEnum() > DT_DOUBLE)
			return res;

		if (nodes[0]->GetTypeDesc(p).GetTypeEnum() < DT_INT)
			LETHE_RET_FALSE(nodes[0]->ConvertConstTo(DT_INT, p));

		if (dtn.GetTypeEnum() == DT_DOUBLE)
			nodes[0]->num.d *= -1.0;
		else if (dtn.GetTypeEnum() == DT_FLOAT)
			nodes[0]->num.f *= -1.0f;
		else if (dtn.GetTypeEnum() == DT_LONG || dtn.GetTypeEnum() == DT_ULONG)
			nodes[0]->num.l *= -1;
		else
			nodes[0]->num.i *= -1;

		LETHE_VERIFY(parent->ReplaceChild(this, nodes[0]));
		nodes.Clear();
		delete this;
		return 1;

	case AST_UOP_NOT:
		if (dtn.GetTypeEnum() >= DT_FLOAT)
			return res;

		if (nodes[0]->GetTypeDesc(p).GetTypeEnum() < DT_INT)
			LETHE_RET_FALSE(nodes[0]->ConvertConstTo(DT_INT, p));

		if (dtn.GetTypeEnum() >= DT_LONG)
			nodes[0]->num.ul = ~nodes[0]->num.ul;
		else
			nodes[0]->num.ui = ~nodes[0]->num.ui;

		LETHE_VERIFY(parent->ReplaceChild(this, nodes[0]));
		nodes.Clear();
		delete this;
		return 1;

	case AST_UOP_LNOT:
		if (!nodes[0]->ConvertConstTo(DT_BOOL, p))
			return res;

		nodes[0]->num.i = !nodes[0]->num.i;
		nodes[0]->parent = parent;
		LETHE_VERIFY(parent->ReplaceChild(this, nodes[0]));
		nodes.Clear();
		delete this;
		return 1;

	default:
		;
	}

	return 0;
}

QDataType AstUnaryOp::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;

	auto qdt = nodes[0]->GetTypeDesc(p);

	if (qdt.IsStruct())
	{
		// it has to be an operator
		auto *tnode = GetTypeNode();

		LETHE_ASSERT(tnode);

		if (!tnode)
			return res;

		return tnode ? tnode->GetTypeDesc(p) : res;
	}

	if (type == AST_UOP_LNOT)
		res.ref = &p.elemTypes[DT_BOOL];
	else
	{
		res = qdt;
		// for smaller int types, the type is int!
		if (qdt.IsSmallNumber())
			res.ref = &p.elemTypes[DT_INT];
	}

	return res;
}

const char *AstUnaryOp::GetOpName() const
{
	const char *opName = nullptr;

	switch (type)
	{
	case AST_UOP_PLUS:
		opName = "+";
		break;

	case AST_UOP_MINUS:
		opName = "-";
		break;

	case AST_UOP_NOT:
		opName = "~";
		break;

	case AST_UOP_LNOT:
		opName = "!";
		break;

	case AST_UOP_PREINC:
		opName = "++";
		break;

	case AST_UOP_PREDEC:
		opName = "--";
		break;

	default:
		;
	}

	return opName;
}

bool AstUnaryOp::CodeGenOperator(CompiledProgram &p)
{
	auto left = nodes[0]->GetTypeDesc(p);

	AstNode *op = nullptr;

	auto opName = GetOpName();

	if (!opName)
		return p.Error(this, "invalid operator type");

	if (left.IsStruct())
	{
		auto scope = left.GetType().structScopeRef;

		if (!scope)
			return p.Error(this, "no struct scope");

		op = scope->FindOperator(p, opName, left);
	}

	if (!op)
		return p.Error(this, "matching operator not found!");

	return AstCall::CallOperator(1, p, this, op);
}

bool AstUnaryOp::CodeGen(CompiledProgram &p)
{
	auto dt = nodes[0]->GetTypeDesc(p);

	if (type == AST_OP_SWAP_NULL)
	{
		if (dt.IsReference())
			return true;

		const Int nwords = (dt.GetSize() + Stack::WORD_SIZE-1) / Stack::WORD_SIZE;

		p.EmitI24(OPC_PUSHZ_RAW, nwords);

		p.EmitI24(OPC_LPUSHADR, 0);

		auto fctor = dt.GetType().funCtor;

		if (fctor >= 0)
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_CALL, fctor));

		if (dt.IsPointer())
			dt.qualifiers &= ~AST_Q_SKIP_DTOR;

		p.PushStackType(dt);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_RAW_PTR]));

		LETHE_RET_FALSE(nodes[0]->CodeGenRef(p));

		p.EmitU24(OPC_PSWAP, dt.GetSize());

		p.PopStackType(true);
		p.PopStackType();

		return true;
	}

	if (dt.IsStruct())
		return CodeGenOperator(p);

	LETHE_RET_FALSE(nodes[0]->CodeGen(p));

	// TODO: conversions!
	switch(type)
	{
	case AST_UOP_PLUS:
		if (dt.GetTypeEnum() == DT_NONE || dt.GetTypeEnum() > DT_DOUBLE)
			return p.Error(this, "illegal type for unary +");

		break;

	case AST_UOP_MINUS:
		if (dt.GetTypeEnum() == DT_NONE || dt.GetTypeEnum() > DT_DOUBLE)
			return p.Error(this, "illegal type for unary -");

		switch(dt.GetTypeEnum())
		{
		case DT_LONG:
		case DT_ULONG:
			p.Emit(OPC_LNEG);
			break;

		case DT_FLOAT:
			p.Emit(OPC_FNEG);
			break;

		case DT_DOUBLE:
			p.Emit(OPC_DNEG);
			break;

		default:
			p.Emit(OPC_INEG);
		}
		break;

	case AST_UOP_NOT:
		if (dt.GetTypeEnum() == DT_NONE || dt.GetTypeEnum() > DT_ULONG)
			return p.Error(this, "illegal type for unary ~");

		p.Emit(dt.GetTypeEnum() >= DT_LONG ? (Int)OPC_LNOT : (Int)OPC_INOT);
		break;

	case AST_UOP_LNOT:
		if (!dt.IsNumber())
			LETHE_RET_FALSE(p.EmitConv(nodes[0], dt, QDataType::MakeConstType(p.elemTypes[DT_BOOL])));

		switch(dt.GetTypeEnum())
		{
		case DT_FLOAT:
			p.Emit(OPC_FCMPZ);
			break;

		case DT_DOUBLE:
			p.Emit(OPC_DCMPZ);
			break;

		case DT_LONG:
		case DT_ULONG:
			p.Emit(OPC_LCMPZ);
			break;

		default:
			p.Emit(OPC_ICMPZ);
		}
		p.PopStackType(1);
		p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_BOOL]));
		break;

	default:
		;
	}

	return true;
}

const AstNode *AstUnaryOp::FindUserDefOperatorType(const AstNode *tpe) const
{
	auto *opName = GetOpName();

	if (tpe->type != AST_STRUCT || !tpe->scopeRef)
		return nullptr;

	for (auto &&it : tpe->scopeRef->operators)
	{
		if (it->type != AST_FUNC)
			continue;

		auto *fn = AstStaticCast<AstFunc *>(it);

		const auto &fname = AstStaticCast<AstText *>(fn->nodes[AstFunc::IDX_NAME])->text;

		// operator name doesn't match => ignore this one
		if (fname != opName)
			continue;

		auto *args = fn->GetArgs();

		// try to match arguments...
		if (!args || args->nodes.GetSize() != 1)
			continue;

		auto *argType = args->nodes[0]->GetTypeNode();

		if (!argType || argType->type != tpe->type)
			continue;

		// still need to match structs exactly
		if (argType->type == AST_STRUCT && argType != tpe)
			continue;

		// match found
		return fn->GetResult()->GetTypeNode();
	}

	return nullptr;
}

AstNode *AstUnaryOp::GetResolveTarget() const
{
	return nodes[0]->GetResolveTarget();
}

const AstNode *AstUnaryOp::GetTypeNode() const
{
	auto *tpe = nodes[0]->GetTypeNode();

	if (tpe && tpe->type == AST_STRUCT)
	{
		// try user-defined operators...
		return FindUserDefOperatorType(tpe);
	}

	if (type == AST_UOP_LNOT)
	{
		// return bool
		static AstTypeBool tnode{TokenLocation()};
		return &tnode;
	}

	if (tpe && (type == AST_UOP_PLUS || type == AST_UOP_MINUS))
	{
		auto dte = TypeEnumFromNode(tpe);

		if (dte >= DT_BOOL && dte < DT_INT)
		{
			static AstTypeInt tnode{TokenLocation()};
			return &tnode;
		}
	}

	return tpe;
}


}
