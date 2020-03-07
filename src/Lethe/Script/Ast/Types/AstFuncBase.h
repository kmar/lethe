#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstFuncBase : public AstNode
{
public:
	SCRIPT_AST_NODE(AstFuncBase)

	enum
	{
		IDX_RET = 0
	};

	typedef AstNode Super;

	AstFuncBase(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	// get minimum number of args
	Int GetMinArgs(bool nodefault = 1) const;
	bool HasEllipsis() const;

	inline const AstNode *GetResult() const
	{
		return nodes[0];
	}

	AstNode *GetResolveTarget() const override;

	virtual AstNode *GetArgs() const {return nullptr;}
};


}
