#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeString : public AstBaseType
{
public:
	SCRIPT_AST_NODE(AstTypeString)

	typedef AstBaseType Super;

	AstTypeString(const TokenLocation &nloc) : Super(AST_TYPE_STRING, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool TypeGen(CompiledProgram &p) override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
