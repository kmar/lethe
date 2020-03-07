#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstDefaultInit : public AstNode
{
public:
	SCRIPT_AST_NODE(AstDefaultInit)

	typedef AstNode Super;

	explicit AstDefaultInit(const TokenLocation &nloc) : AstNode(AST_DEFAULT_INIT, nloc) {}
};

}
