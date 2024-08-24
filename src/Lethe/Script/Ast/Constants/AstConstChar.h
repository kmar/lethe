#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstChar : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstChar)

	typedef AstConstEnumBase Super;

	AstConstChar(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_CHAR, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
