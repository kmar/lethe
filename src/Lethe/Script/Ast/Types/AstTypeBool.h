#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeBool : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeBool)

	typedef AstBaseType Super;

	AstTypeBool(const TokenLocation &nloc) : Super(AST_TYPE_BOOL, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
