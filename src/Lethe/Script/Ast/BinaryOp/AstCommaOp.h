#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

class LETHE_API AstCommaOp : public AstNode
{
public:
	SCRIPT_AST_NODE(AstCommaOp)

	typedef AstNode Super;

	AstCommaOp(const TokenLocation &nloc) : Super(AST_OP_COMMA, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	const AstNode *GetTypeNode() const override;
};


}
