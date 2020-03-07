#pragma once

#include <Lethe/Script/Ast/AstNode.h>

namespace lethe
{

// constants or literals
class LETHE_API AstConstant : public AstNode
{
public:
	SCRIPT_AST_NODE(AstConstant)

	typedef AstNode Super;

	AstConstant(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	AstNode *ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p) override;
	AstNode *ConvertConstNode(const DataType &dt, DataTypeEnum dte, const CompiledProgram &p) const;
	Int ToBoolConstant(const CompiledProgram &p) override;
};

}
