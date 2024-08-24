#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstUInt : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstUInt)

	typedef AstConstEnumBase Super;

	AstConstUInt(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_UINT, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
