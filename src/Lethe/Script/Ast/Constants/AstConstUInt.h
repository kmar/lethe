#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstUInt : public AstConstant
{
public:
	LETHE_AST_NODE(AstConstUInt)

	typedef AstConstant Super;

	AstConstUInt(const TokenLocation &nloc) : Super(AST_CONST_UINT, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
