#pragma once

#include "AstControl.h"

namespace lethe
{

class LETHE_API AstFor : public AstControl
{
public:
	LETHE_AST_NODE(AstFor)

	typedef AstControl Super;

	AstFor(const TokenLocation &nloc) : Super(AST_FOR, nloc) {}

	bool CodeGen(CompiledProgram &p) override;

	bool ResolveNode(const ErrorHandler &e) override;

	static bool CodeGenCommon(NamedScope *nscopeRef, CompiledProgram &p, ArrayRef<AstNode *> nnodes);

private:
	bool ConvertRangeBasedFor(const ErrorHandler &p, AstNodeType itertype);
};


}
