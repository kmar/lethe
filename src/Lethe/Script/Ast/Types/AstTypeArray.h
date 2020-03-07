#pragma once

#include "AstCustomType.h"

namespace lethe
{

class LETHE_API AstTypeArray : public AstCustomType
{
public:
	SCRIPT_AST_NODE(AstTypeArray)

	typedef AstCustomType Super;

	AstTypeArray(const TokenLocation &nloc) : Super(AST_TYPE_ARRAY, nloc)
	{
		flags |= AST_F_SKIP_CGEN;
	}

	bool TypeGen(CompiledProgram &p) override;
	bool CodeGenComposite(CompiledProgram &p) override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
