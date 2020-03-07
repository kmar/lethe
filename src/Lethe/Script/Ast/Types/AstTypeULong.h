#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeULong : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeULong)

	typedef AstBaseType Super;

	AstTypeULong(const TokenLocation &nloc) : Super(AST_TYPE_ULONG, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
