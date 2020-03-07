#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeLong : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeLong)

	typedef AstBaseType Super;

	AstTypeLong(const TokenLocation &nloc) : Super(AST_TYPE_LONG, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
