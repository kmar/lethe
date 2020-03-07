#include "AstCommaOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstCommaOp

bool AstCommaOp::CodeGen(CompiledProgram &p)
{
	// need to evaluate in order, so need to evaluate node[0] first
	if (!nodes[0]->IsConstant())
	{
		Int mark = p.ExprStackMark();
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		p.ExprStackCleanupTo(mark);
	}

	LETHE_RET_FALSE(nodes[1]->CodeGen(p));
	return 1;
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
