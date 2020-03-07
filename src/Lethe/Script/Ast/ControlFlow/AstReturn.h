#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstReturn : public AstNode
{
public:
	SCRIPT_AST_NODE(AstReturn)

	typedef AstNode Super;

	AstReturn(const TokenLocation &nloc) : Super(AST_RETURN, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};


}
