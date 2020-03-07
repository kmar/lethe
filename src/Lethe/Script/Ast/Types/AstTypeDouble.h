#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeDouble : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeDouble)

	typedef AstBaseType Super;

	AstTypeDouble(const TokenLocation &nloc) : Super(AST_TYPE_DOUBLE, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};

}
