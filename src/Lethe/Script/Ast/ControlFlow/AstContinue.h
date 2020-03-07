#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstContinue : public AstNode
{
public:
	SCRIPT_AST_NODE(AstContinue)

	typedef AstNode Super;

	AstContinue(const TokenLocation &nloc) : Super(AST_CONTINUE, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	bool CodeGen(CompiledProgram &p) override;
};


}
