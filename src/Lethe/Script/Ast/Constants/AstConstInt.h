#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstInt : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstInt)

	typedef AstConstEnumBase Super;

	AstConstInt(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_INT, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
