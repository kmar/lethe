#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstLong : public AstConstEnumBase
{
public:
	LETHE_AST_NODE(AstConstLong)

	typedef AstConstEnumBase Super;

	AstConstLong(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_LONG, nloc, tref) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
