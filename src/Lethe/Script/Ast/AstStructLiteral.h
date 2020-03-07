#pragma once

#include "AstNode.h"

namespace lethe
{

class LETHE_API AstStructLiteral : public AstNode
{
public:
	SCRIPT_AST_NODE(AstStructLiteral)

	typedef AstNode Super;

	AstStructLiteral(const TokenLocation &nloc) : Super(AST_STRUCT_LITERAL, nloc) {}

	const AstNode *GetTypeNode() const override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool CodeGen(CompiledProgram &p) override;
};

}
