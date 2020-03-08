#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

class LETHE_API AstUnaryOp : public AstNode
{
public:
	LETHE_AST_NODE(AstUnaryOp)

	typedef AstNode Super;

	AstUnaryOp(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	bool FoldConst(const CompiledProgram &p) override;
	bool CodeGen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	const AstNode *GetTypeNode() const override;

protected:
	const AstNode *FindUserDefOperatorType(const AstNode *tpe) const;

	bool CodeGenOperator(CompiledProgram &p);
	const char *GetOpName() const;
};

}
