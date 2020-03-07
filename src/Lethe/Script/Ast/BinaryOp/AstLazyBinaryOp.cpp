#include "AstLazyBinaryOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstLazyBinaryOp

QDataType AstLazyBinaryOp::GetTypeDesc(const CompiledProgram &p) const
{
	return QDataType::MakeType(p.elemTypes[DT_BOOL]);
}

bool AstLazyBinaryOp::FoldConst(const CompiledProgram &p)
{
	bool res = nodes[0]->FoldConst(p) || nodes[1]->FoldConst(p);

	if (!nodes[0]->IsConstant() || !nodes[0]->ConvertConstTo(DT_BOOL, p))
		return res;

	// 0 || expr => expr
	// 1 || expr => true
	// 0 && expr => false
	// 1 && expr => expr
	Int idx = !nodes[0]->num.i;

	if (type == AST_OP_LAND)
		idx ^= 1;

	parent->ReplaceChild(this, nodes[idx]);
	nodes[idx] = nullptr;
	delete this;
	return res;
}

// lazy binary op needs either int or float... => strings must be converted to int
bool AstLazyBinaryOp::CodeGen(CompiledProgram &p)
{
	while (nodes[0]->type == type)
	{
		// reorder!
		// (a && b) && c => a && (b && c) to simplify bailouts a bit
		auto *child = nodes[0];
		auto *c = nodes[1];
		auto *a = child->nodes[0];
		auto *b = child->nodes[1];

		nodes[1] = child;
		nodes[0] = a;
		a->parent = this;
		child->nodes[0] = b;
		child->nodes[1] = c;
		b->parent = c->parent = child;
	}

	LETHE_RET_FALSE(nodes[0]->CodeGen(p));

	if (p.exprStack.GetSize() < 1)
		return p.Error(nodes[0], "lhs must return a value");

	auto dt0 = p.exprStack.Back();

	if (!dt0.IsNumber() || dt0.IsLongInt())
		LETHE_RET_FALSE(p.EmitConv(nodes[0], dt0, p.elemTypes[DT_BOOL]));

	// now jump based on results
	UInt jmpIns = type == AST_OP_LAND ? OPC_ICMPNZ_BZ : OPC_ICMPNZ_BNZ;

	if (p.CanOptPrevious())
	{
		if (CompiledProgram::IsConvToBool(p.instructions.Back()))
		{
			// no need to convert to int...
			// but alas, I don't have such instructions!
			jmpIns = type == AST_OP_LAND ? OPC_IBZ : OPC_IBNZ;
			dt0 = QDataType::MakeConstType(p.elemTypes[DT_INT]);
		}
	}

	p.PopStackType();
	Int jtarg = p.EmitForwardJump(jmpIns);

	auto inc = 2*(dt0.GetTypeEnum() == DT_FLOAT) + 4*(dt0.GetTypeEnum() == DT_DOUBLE);

	if (inc)
	{
		// FIXME: hack!
		p.instructions.Back() += inc;
	}

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (p.exprStack.GetSize() < 1)
		return p.Error(nodes[1], "rhs must return a value");

	auto dt1 = p.exprStack.Back();

	if (!dt1.IsNumber() || dt1.IsLongInt())
		LETHE_RET_FALSE(p.EmitConv(nodes[1], dt1, p.elemTypes[DT_BOOL]));
	else
	{
		// convert to bool
		p.Emit(OPC_ICMPNZ);

		inc = 2*(dt1.GetTypeEnum() == DT_FLOAT) + 4*(dt1.GetTypeEnum() == DT_DOUBLE);

		if (inc)
		{
			// FIXME: hack!
			p.instructions.Back() += inc;
		}
	}

	p.PopStackType();
	p.PushStackType(QDataType::MakeConstType(p.elemTypes[DT_BOOL]));

	p.FixupForwardTarget(jtarg);
	return true;
}


}
