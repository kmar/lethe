#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryNew : public AstUnaryOp
{
public:
	SCRIPT_AST_NODE(AstUnaryNew)

	enum
	{
		IDX_CLASS
	};

	typedef AstUnaryOp Super;

	explicit AstUnaryNew(const TokenLocation &nloc) : Super(AST_NEW, nloc) {}

	bool IsConstExpr() const override
	{
		return false;
	}

	const AstNode *GetTypeNode() const override;
	AstNode *GetResolveTarget() const override;

	bool CodeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
};

}
