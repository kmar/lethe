#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstDefer : public AstNode
{
public:
	LETHE_AST_NODE(AstDefer)

	typedef AstNode Super;

	explicit AstDefer(const TokenLocation &nloc) : AstNode(AST_DEFER, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
};

}
