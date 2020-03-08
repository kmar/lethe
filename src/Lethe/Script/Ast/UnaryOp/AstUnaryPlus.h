#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryPlus : public AstUnaryOp
{
public:
	LETHE_AST_NODE(AstUnaryPlus)

	typedef AstUnaryOp Super;

	explicit AstUnaryPlus(const TokenLocation &nloc) : Super(AST_UOP_PLUS, nloc) {}
};

}
