#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstLong : public AstConstant
{
public:
	SCRIPT_AST_NODE(AstConstLong)

	typedef AstConstant Super;

	AstConstLong(const TokenLocation &nloc) : Super(AST_CONST_LONG, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
