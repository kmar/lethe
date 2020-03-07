#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstChar : public AstConstant
{
public:
	SCRIPT_AST_NODE(AstConstChar)

	typedef AstConstant Super;

	AstConstChar(const TokenLocation &nloc) : Super(AST_CONST_CHAR, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
