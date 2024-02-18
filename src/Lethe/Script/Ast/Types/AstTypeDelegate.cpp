#include "AstTypeDelegate.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeDelegate

bool AstTypeDelegate::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));
	// [0] = return type
	// [1] = arglist
	// try to create virtual type

	typeRef.qualifiers = qualifiers;
	typeRef.ref = p.AddType(GenFuncType(this, p, nodes[0], nodes[1]->nodes, true));
	return true;
}

const AstNode *AstTypeDelegate::GetTypeNode() const
{
	return this;
}

AstNode *AstTypeDelegate::GetResolveTarget() const
{
	return (AstNode *)this;
}


}
