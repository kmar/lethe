#pragma once

#include "AstControl.h"

namespace lethe
{

class LETHE_API AstWhile : public AstControl
{
public:
	SCRIPT_AST_NODE(AstWhile)

	typedef AstControl Super;

	AstWhile(const TokenLocation &nloc) : Super(AST_WHILE, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};


}
