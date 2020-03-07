#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstBreak : public AstNode
{
public:
	SCRIPT_AST_NODE(AstBreak)

	typedef AstNode Super;

	AstBreak(const TokenLocation &nloc) : Super(AST_BREAK, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	bool CodeGen(CompiledProgram &p) override;
};


}
