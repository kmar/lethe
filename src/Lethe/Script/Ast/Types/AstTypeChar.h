#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeChar : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeChar)

	typedef AstBaseType Super;

	AstTypeChar(const TokenLocation &nloc) : Super(AST_TYPE_CHAR, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
