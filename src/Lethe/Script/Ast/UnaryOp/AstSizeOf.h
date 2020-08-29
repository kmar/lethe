#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstSizeOf : public AstNode
{
public:
	LETHE_AST_NODE(AstSizeOf)

	typedef AstNode Super;

	explicit AstSizeOf(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) { }

	bool FoldConst(const CompiledProgram &p) override;

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

private:
	static bool TryTypeGen(AstNode *node, const CompiledProgram &p);
};

}
