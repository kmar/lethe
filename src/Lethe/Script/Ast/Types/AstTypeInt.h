#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeInt : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeInt)

	typedef AstBaseType Super;

	AstTypeInt(const TokenLocation &nloc) : Super(AST_TYPE_INT, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
