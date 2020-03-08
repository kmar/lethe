#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeUInt : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeUInt)

	typedef AstBaseType Super;

	AstTypeUInt(const TokenLocation &nloc) : Super(AST_TYPE_UINT, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
