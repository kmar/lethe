#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstBlock : public AstNode
{
	LETHE_BUCKET_ALLOC(AstBlock)

public:
	LETHE_AST_NODE(AstBlock)

	typedef AstNode Super;

	AstBlock(const TokenLocation &nloc) : Super(AST_BLOCK, nloc) {}

	AstBlock(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool ResolveNode(const ErrorHandler &e) override;
	bool CodeGen(CompiledProgram &p) override;

	TokenLocation endOfBlockLocation;
};

}
