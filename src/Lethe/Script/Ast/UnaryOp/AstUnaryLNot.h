#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryLNot : public AstUnaryOp
{
public:
	SCRIPT_AST_NODE(AstUnaryLNot)

	typedef AstUnaryOp Super;

	explicit AstUnaryLNot(const TokenLocation &nloc) : Super(AST_UOP_LNOT, nloc) {}
};

}
