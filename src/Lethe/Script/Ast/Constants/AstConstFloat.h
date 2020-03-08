#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstFloat : public AstConstant
{
public:
	LETHE_AST_NODE(AstConstFloat)

	typedef AstConstant Super;

	AstConstFloat(const TokenLocation &nloc) : Super(AST_CONST_FLOAT, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
