#pragma once

#include "../AstText.h"

namespace lethe
{

class LETHE_API AstTextConstant : public AstText
{
public:
	SCRIPT_AST_NODE(AstTextConstant)

	typedef AstText Super;

	explicit AstTextConstant(const String &ntext, AstNodeType ntype, const TokenLocation &nloc) :
		AstText(ntext, ntype, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	AstNode *ConvertConstTo(DataTypeEnum dte, const CompiledProgram &p) override;
	Int ToBoolConstant(const CompiledProgram &p) override;
};

}
