#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeSByte : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeSByte)

	typedef AstBaseType Super;

	AstTypeSByte(const TokenLocation &nloc) : Super(AST_TYPE_SBYTE, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
