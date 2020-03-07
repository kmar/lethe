#pragma once

#include "../AstNode.h"

namespace lethe
{

class LETHE_API AstSizeOf : public AstNode
{
public:
	SCRIPT_AST_NODE(AstSizeOf)

	typedef AstNode Super;

	explicit AstSizeOf(const TokenLocation &nloc) : Super(AST_SIZEOF, nloc) { }

	bool FoldConst(const CompiledProgram &p) override;

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
};

}
