#pragma once

#include "AstControl.h"

namespace lethe
{

class LETHE_API AstIf : public AstControl
{
public:
	LETHE_AST_NODE(AstIf)

	typedef AstControl Super;

	AstIf(const TokenLocation &nloc) : Super(AST_IF, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};


}
