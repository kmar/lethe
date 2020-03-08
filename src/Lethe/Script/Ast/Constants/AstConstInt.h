#pragma once

#include "AstConstant.h"

namespace lethe
{

class LETHE_API AstConstInt : public AstConstant
{
	LETHE_BUCKET_ALLOC(AstConstInt)
public:
	LETHE_AST_NODE(AstConstInt)

	typedef AstConstant Super;

	AstConstInt(const TokenLocation &nloc, const DataType *tref = nullptr)
		: Super(AST_CONST_INT, nloc)
		, typeRef(tref)
	{
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;

	void CopyTo(AstNode *n) const override;

private:
	friend class AstEnumItem;
	friend class AstSymbol;

	const DataType *typeRef = nullptr;
};


}
