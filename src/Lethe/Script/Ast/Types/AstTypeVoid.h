#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeVoid : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeVoid)

	typedef AstBaseType Super;

	AstTypeVoid(const TokenLocation &nloc) : Super(AST_TYPE_VOID, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
