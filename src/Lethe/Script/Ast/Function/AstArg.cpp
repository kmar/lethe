#include "AstArg.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstArg

const AstNode *AstArg::GetTypeNode() const
{
	auto res = nodes[0]->target ? nodes[0]->target : nodes[0]->GetTypeNode();

	if (res && res->type == AST_TYPEDEF)
		res = res->GetTypeNode();

	return res;
}

QDataType AstArg::GetTypeDesc(const CompiledProgram &p) const
{
	auto res = nodes[0]->GetTypeDesc(p);

	// FIXME: this is to work around refcounting opts and modification of argvalue!
	if (!res.IsReference() && res.IsPointer() && res.GetTypeEnum() != DT_RAW_PTR)
		res.qualifiers |= AST_Q_NOCOPY;

	return res;
}

bool AstArg::CodeGen(CompiledProgram &p)
{
	// [2] = default expr; we don't want to codegen here...
	for (Int i=0; i<nodes.GetSize(); i++)
	{
		if (i == 0 || i == 2)
			continue;

		LETHE_RET_FALSE(nodes[i]->CodeGen(p));
	}

	return true;
}


}
