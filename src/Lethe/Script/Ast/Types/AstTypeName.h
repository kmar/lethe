#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeName : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeName)

	typedef AstBaseType Super;

	AstTypeName(const TokenLocation &nloc) : Super(AST_TYPE_NAME, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
