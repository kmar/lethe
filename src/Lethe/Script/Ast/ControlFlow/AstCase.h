#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstCase : public AstNode
{
public:
	LETHE_AST_NODE(AstCase)

	typedef AstNode Super;

	AstCase(const TokenLocation &nloc) : Super(AST_CASE, nloc) {}
};


}
