#include "AstCommaOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstCommaOp

bool AstCommaOp::CodeGen(CompiledProgram &p)
{
	// need to evaluate in order, so need to evaluate node[0] first
	if (nodes[0]->HasSideEffects())
	{
		Int mark = p.ExprStackMark();
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		p.ExprStackCleanupTo(mark);
	}

	// if parent is comma or void cast, we can skip codegen if no side effects
	if (parent && !nodes[1]->HasSideEffects())
	{
		if (parent->type == AST_OP_COMMA)
			return true;

		if (parent->type == AST_CAST && parent->nodes[0]->type == AST_TYPE_VOID)
			return true;
	}

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));
	return true;
}

QDataType AstCommaOp::GetTypeDesc(const CompiledProgram &p) const
{
	return nodes[0]->GetTypeDesc(p);
}

const AstNode *AstCommaOp::GetTypeNode() const
{
	return nodes[0];
}


}
