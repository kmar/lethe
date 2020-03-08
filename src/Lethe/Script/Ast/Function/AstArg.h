#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstArg : public AstNode
{
public:
	LETHE_AST_NODE(AstArg)

	typedef AstNode Super;

	AstArg(const TokenLocation &nloc) : Super(AST_ARG, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
