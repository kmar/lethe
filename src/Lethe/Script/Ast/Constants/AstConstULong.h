#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstULong : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstULong)

	typedef AstConstEnumBase Super;

	AstConstULong(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_ULONG, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
