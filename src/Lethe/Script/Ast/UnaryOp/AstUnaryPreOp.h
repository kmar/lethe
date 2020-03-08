#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryPreOp : public AstUnaryOp
{
public:
	LETHE_AST_NODE(AstUnaryPreOp)

	typedef AstUnaryOp Super;

	explicit AstUnaryPreOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool IsConstExpr() const override
	{
		return false;
	}

	bool CodeGen(CompiledProgram &p) override;
	bool CodeGenRef(CompiledProgram &p, bool allowConst = 0, bool derefPtr = 0) override;

	bool CanPassByReference(const CompiledProgram &) const override
	{
		return true;
	}
};

}
