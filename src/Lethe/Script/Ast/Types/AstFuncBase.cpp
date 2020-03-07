#include "AstFuncBase.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstFuncBase

AstNode *AstFuncBase::GetResolveTarget() const
{
	return nodes[IDX_RET]->GetResolveTarget();
}

bool AstFuncBase::HasEllipsis() const
{
	const AstNode *args = GetArgs();
	return !args->nodes.IsEmpty() && args->nodes.Back()->type == AST_ARG_ELLIPSIS;
}

// get minimum number of args
Int AstFuncBase::GetMinArgs(bool nodefault) const
{
	const AstNode *args = GetArgs();

	if (args->nodes.GetSize() == 1 && args->nodes[0]->type == AST_TYPE_VOID)
		return 0;

	Int i = args->nodes.GetSize() - HasEllipsis();

	while (!nodefault && i > 0 && args->nodes[i-1]->nodes.GetSize() > 2)
		i--;

	return i;
}


}
