#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstControl : public AstNode
{
public:
	SCRIPT_AST_NODE(AstControl)

	typedef AstNode Super;

	bool CodeGenBoolExpr(AstNode *n, CompiledProgram &p, bool varScope = 0);

	AstControl(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}
};


}
