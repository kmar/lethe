#pragma once

#include "AstControl.h"

namespace lethe
{

class LETHE_API AstDo : public AstControl
{
public:
	SCRIPT_AST_NODE(AstDo)

	typedef AstControl Super;

	AstDo(const TokenLocation &nloc) : Super(AST_DO, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};


}
