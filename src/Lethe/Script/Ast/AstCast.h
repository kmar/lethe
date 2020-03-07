#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstCast : public AstNode
{
public:
	SCRIPT_AST_NODE(AstCast)

	typedef AstNode Super;

	explicit AstCast(const TokenLocation &nloc) : Super(AST_CAST, nloc) {	}

	bool FoldConst(const CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
	const AstNode *GetTypeNode() const override;
};

}
