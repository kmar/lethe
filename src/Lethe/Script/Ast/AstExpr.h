#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstExpr : public AstNode
{
public:
	SCRIPT_AST_NODE(AstExpr)

	typedef AstNode Super;

	AstExpr(const TokenLocation &nloc) : Super(AST_EXPR, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
};

}
