#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstBaseType : public AstNode
{
public:
	LETHE_AST_NODE(AstBaseType)

	typedef AstNode Super;

	AstBaseType(AstNodeType ntype, const TokenLocation &nloc)
		: Super(ntype, nloc)
	{
	}

	AstNode *GetResolveTarget() const override;

	const AstNode *GetTypeNode() const override
	{
		return this;
	}
};


}
