#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

// && ||
class LETHE_API AstLazyBinaryOp : public AstNode
{
public:
	SCRIPT_AST_NODE(AstLazyBinaryOp)

	typedef AstNode Super;

	AstLazyBinaryOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool CodeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool FoldConst(const CompiledProgram &p) override;
};

}
