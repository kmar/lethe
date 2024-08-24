#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstBool : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstBool)

	typedef AstConstEnumBase Super;

	AstConstBool(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_BOOL, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};

}
