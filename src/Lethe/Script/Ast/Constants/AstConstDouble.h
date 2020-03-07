#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstDouble : public AstConstant
{
public:
	SCRIPT_AST_NODE(AstConstDouble)

	typedef AstConstant Super;

	AstConstDouble(const TokenLocation &nloc) : Super(AST_CONST_DOUBLE, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
