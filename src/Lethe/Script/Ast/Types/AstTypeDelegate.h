#pragma once

#include "AstTypeFuncPtr.h"

namespace lethe
{

class LETHE_API AstTypeDelegate : public AstTypeFuncPtr
{
public:
	LETHE_AST_NODE(AstTypeDelegate)

	typedef AstTypeFuncPtr Super;

	AstTypeDelegate(const TokenLocation &nloc) : Super(nloc)
	{
		type = AST_TYPE_DELEGATE;
	}

	bool TypeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};


}
