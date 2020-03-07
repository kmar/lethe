#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstProgram : public AstNode
{
public:
	SCRIPT_AST_NODE(AstProgram)

	typedef AstNode Super;

	explicit AstProgram(const TokenLocation &nloc) : AstNode(AST_PROGRAM, nloc) {}

	bool ResolveNode(const ErrorHandler &e) override;
};

}
