#include "AstIf.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstIf

bool AstIf::CodeGen(CompiledProgram &p)
{
	// [0] = cond, [1] = taken_stmt, [2] = optional else stmt
	Int bconst = nodes[0]->ToBoolConstant(p);

	if (bconst == 1)
	{
		// just generate true part
		return nodes[1]->CodeGen(p);
	}
	else if (bconst == 0)
	{
		// generate else part or nothing
		return nodes.GetSize() > 2 ? nodes[2]->CodeGen(p) : 1;
	}

	// FIXME: superhack! I should better come up with a general solution to this, everywhere!
	while (nodes.GetSize() < 3 && nodes[0]->type == AST_OP_LAND)
	{
		// convert if (a && b) c; to if (a) if (b) c;
		AstIf *tmp = new AstIf(location);
		tmp->nodes.Resize(2);

		tmp->nodes[0] = nodes[0]->nodes[1];
		tmp->nodes[0]->parent = tmp;

		tmp->nodes[1] = nodes[1];
		tmp->nodes[1]->parent = tmp;

		auto left = nodes[0]->nodes[0];

		nodes[0]->parent = nullptr;
		nodes[0]->nodes.Clear();
		delete nodes[0];

		nodes[0] = left;
		nodes[0]->parent = this;

		nodes[1] = tmp;
		nodes[1]->parent = this;
	}

	LETHE_RET_FALSE(CodeGenBoolExpr(nodes[0], p, 1));
	// now emit forward jump
	DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
	Int fwd = p.EmitForwardJump(p.ConvJump(dt, OPC_IBZ_P));
	p.PopStackType(1);
	LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (nodes.GetSize() > 2)
	{
		// handle else branch
		bool noSkipElse = p.CanOptPrevious() && (p.instructions.Back() & 255) == OPC_RET;
		Int skipElse = noSkipElse ? -1 : p.EmitForwardJump(OPC_BR);
		LETHE_RET_FALSE(p.FixupForwardTarget(fwd));
		LETHE_RET_FALSE(nodes[2]->CodeGen(p));

		if (skipElse >= 0)
			LETHE_RET_FALSE(p.FixupForwardTarget(skipElse));
	}
	else
		LETHE_RET_FALSE(p.FixupForwardTarget(fwd));

	return true;
}


}
