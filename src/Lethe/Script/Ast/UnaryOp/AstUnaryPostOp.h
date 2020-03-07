#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryPostOp : public AstUnaryOp
{
public:
	SCRIPT_AST_NODE(AstUnaryPostOp)

	typedef AstUnaryOp Super;

	explicit AstUnaryPostOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool IsConstExpr() const override
	{
		return false;
	}

	bool CodeGen(CompiledProgram &p) override;
};

}
