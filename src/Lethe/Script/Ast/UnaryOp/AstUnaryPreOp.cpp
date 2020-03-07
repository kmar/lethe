#include "AstUnaryPreOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/CodeGenTables.h>

namespace lethe
{

// AstUnaryPreOp

bool AstUnaryPreOp::CodeGenRef(CompiledProgram &p, bool allowConst, bool)
{
	LETHE_RET_FALSE(CodeGen(p));
	p.EmitI24(OPC_POP, 1);
	p.PopStackType(1);
	return nodes[0]->CodeGenRef(p, allowConst);
}

bool AstUnaryPreOp::CodeGen(CompiledProgram &p)
{
	// FIXME: refactor!
	auto dt = nodes[0]->GetTypeDesc(p);

	if (!nodes[0]->IsConstExpr() && !dt.IsStruct())
	{
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));

		if (p.exprStack.IsEmpty() || !p.exprStack.Back().IsReference())
			return p.Error(this, "unary preop must return a reference value");

		p.PopStackType(1);
		p.Emit(OPC_POP + (1 << 8));
	}

	if (dt.IsStruct())
		return CodeGenOperator(p);

	auto tnode = nodes[0];

	while (tnode->type == AST_UOP_PREINC || tnode->type == AST_UOP_PREDEC)
		tnode = tnode->nodes[0];

	LETHE_RET_FALSE(tnode && tnode->CodeGenRef(p));

	if (dt.GetTypeEnum() <= DT_BOOL || dt.GetTypeEnum() >= DT_FLOAT)
		return p.Error(this, "invalid type for pre-op");

	const Int amt = type == AST_UOP_PREINC ? 1 : -1;

	bool pop = ShouldPop();
	Int &lins = p.instructions.Back();
	UInt linsOpc = lins & 255u;

	if ((dt.GetTypeEnum() == DT_INT || dt.GetTypeEnum() == DT_UINT) && linsOpc == OPC_LPUSHADR)
	{
		Int loffset = lins >> 8;

		if (loffset >= 0 && loffset <= 255)
		{
			// shortcut...
			lins = OPC_LIADD_ICONST + Int((UInt)amt << 24) + (loffset << 8) + (loffset << 16);

			if (!pop)
				p.EmitI24(OPC_LPUSH32, loffset);
			else
				p.PopStackType(1);

			return 1;
		}
	}

	p.EmitI24(opcodeRefInc[dt.GetTypeEnum()], amt);
	p.PopStackType(1);
	p.PushStackType(dt);
	return true;
}


}
