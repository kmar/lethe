#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstNull : public AstConstant
{
public:
	SCRIPT_AST_NODE(AstConstNull)

	typedef AstConstant Super;

	AstConstNull(const TokenLocation &nloc) : Super(AST_CONST_NULL, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
};


}
