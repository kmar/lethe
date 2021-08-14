#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstControl : public AstNode
{
public:
	LETHE_AST_NODE(AstControl)

	typedef AstNode Super;

	static bool CodeGenBoolExpr(AstNode *n, CompiledProgram &p, bool varScope = 0);

	AstControl(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}
};


}
