#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeShort : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeShort)

	typedef AstBaseType Super;

	AstTypeShort(const TokenLocation &nloc) : Super(AST_TYPE_SHORT, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
