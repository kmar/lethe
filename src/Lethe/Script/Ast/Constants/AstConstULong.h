#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstULong : public AstConstant
{
public:
	LETHE_AST_NODE(AstConstULong)

	typedef AstConstant Super;

	AstConstULong(const TokenLocation &nloc) : Super(AST_CONST_ULONG, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
