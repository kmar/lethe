#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeFloat : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeFloat)

	typedef AstBaseType Super;

	AstTypeFloat(const TokenLocation &nloc) : Super(AST_TYPE_FLOAT, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
