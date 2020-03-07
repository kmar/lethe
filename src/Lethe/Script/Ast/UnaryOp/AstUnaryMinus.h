#pragma once

#include "AstUnaryOp.h"

namespace lethe
{

class LETHE_API AstUnaryMinus : public AstUnaryOp
{
public:
	SCRIPT_AST_NODE(AstUnaryMinus)

	typedef AstUnaryOp Super;

	explicit AstUnaryMinus(const TokenLocation &nloc) : Super(AST_UOP_MINUS, nloc) {}
};

}
