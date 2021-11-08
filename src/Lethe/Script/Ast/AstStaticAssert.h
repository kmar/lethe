#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstStaticAssert : public AstNode
{
public:
	LETHE_AST_NODE(AstStaticAssert)

	typedef AstNode Super;

	enum
	{
		IDX_COND,
		IDX_MESSAGE_OPT
	};

	AstStaticAssert(const TokenLocation &nloc) : Super(AST_STATIC_ASSERT, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};

}
