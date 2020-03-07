#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeUShort : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeUShort)

	typedef AstBaseType Super;

	AstTypeUShort(const TokenLocation &nloc) : Super(AST_TYPE_USHORT, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
