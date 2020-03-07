#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryNot : public AstUnaryOp
{
public:
	SCRIPT_AST_NODE(AstUnaryNot)

	typedef AstUnaryOp Super;

	explicit AstUnaryNot(const TokenLocation &nloc) : Super(AST_UOP_NOT, nloc) {}
};

}
