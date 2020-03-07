#pragma once

#include "AstCustomType.h"

namespace lethe
{

class LETHE_API AstTypeArrayRef : public AstCustomType
{
public:
	SCRIPT_AST_NODE(AstTypeArrayRef)

	enum
	{
		IDX_TYPE
	};

	typedef AstCustomType Super;

	AstTypeArrayRef(const TokenLocation &nloc) : Super(AST_TYPE_ARRAY_REF, nloc)
	{
		flags |= AST_F_SKIP_CGEN;
	}

	bool TypeGen(CompiledProgram &p) override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
