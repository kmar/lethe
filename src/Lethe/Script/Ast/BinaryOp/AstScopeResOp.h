#pragma once

#include "../AstSymbol.h"

namespace lethe
{

// note: based on symbol to allow AST collapsing
class LETHE_API AstScopeResOp : public AstSymbol
{
public:
	SCRIPT_AST_NODE(AstScopeResOp)

	typedef AstSymbol Super;

	AstScopeResOp(const TokenLocation &nloc) : Super("", nloc)
	{
		type = AST_OP_SCOPE_RES;
	}

	enum NodeIndices
	{
		IDX_LEFT,
		IDX_RIGHT
	};

	bool ResolveNode(const ErrorHandler &e) override;

	AstNode *ResolveScope(const NamedScope *scope, Int idx, ULong stopMask = 0) const;

	AstNode *ResolveTemplateScope(AstNode *&text) const override;

private:

	AstNode *ResolveScopeInternal(bool wantTemplate = false, const NamedScope *rscope = nullptr) const;
};


}
