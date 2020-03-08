#pragma once

#include "../AstText.h"

namespace lethe
{

class LETHE_API AstGoto : public AstText
{
public:
	LETHE_AST_NODE(AstGoto)

	typedef AstText Super;

	AstGoto(const String &ntext, const TokenLocation &nloc) : Super(ntext, AST_GOTO, nloc) {}

	bool ResolveNode(const ErrorHandler &) override;
	bool CodeGen(CompiledProgram &p) override;
};


}
