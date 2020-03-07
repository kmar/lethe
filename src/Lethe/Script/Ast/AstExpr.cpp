#include "AstExpr.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstExpr

QDataType AstExpr::GetTypeDesc(const CompiledProgram &p) const
{
	return nodes[0]->GetTypeDesc(p);
}

bool AstExpr::CodeGen(CompiledProgram &p)
{
	Int mark = p.ExprStackMark();
	p.SetLocation(nodes[0]->location);
	LETHE_RET_FALSE(nodes[0]->CodeGen(p));
	p.ExprStackCleanupTo(mark);
	return true;
}


}
