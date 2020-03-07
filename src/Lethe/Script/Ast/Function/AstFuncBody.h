#pragma once

#include "../AstBlock.h"

namespace lethe
{

class LETHE_API AstFuncBody : public AstBlock
{
public:
	SCRIPT_AST_NODE(AstFuncBody)

	typedef AstBlock Super;

	AstFuncBody(const TokenLocation &nloc) : Super(AST_FUNC_BODY, nloc) {}
};

}
