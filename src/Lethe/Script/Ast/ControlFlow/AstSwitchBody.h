#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstSwitchBody : public AstNode
{
public:
	SCRIPT_AST_NODE(AstSwitchBody)

	typedef AstNode Super;

	AstSwitchBody(const TokenLocation &nloc) : Super(AST_SWITCH_BODY, nloc) {}
};


}
