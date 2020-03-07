#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstSwitch : public AstNode
{
public:
	SCRIPT_AST_NODE(AstSwitch)

	typedef AstNode Super;

	AstSwitch(const TokenLocation &nloc) : Super(AST_SWITCH, nloc) {}

	enum
	{
		IDX_EXPR,
		IDX_BODY
	};

	bool CodeGen(CompiledProgram &p) override;

private:
	static bool CompareConst(AstNode *n0, AstNode *n1);
};


}
