#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstBool : public AstConstant
{
public:
	LETHE_AST_NODE(AstConstBool)

	typedef AstConstant Super;

	AstConstBool(const TokenLocation &nloc) : Super(AST_CONST_BOOL, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};

}
