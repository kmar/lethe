#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstCaseDefault : public AstNode
{
public:
	SCRIPT_AST_NODE(AstCaseDefault)

	typedef AstNode Super;

	AstCaseDefault(const TokenLocation &nloc) : Super(AST_CASE_DEFAULT, nloc) {}
};


}
