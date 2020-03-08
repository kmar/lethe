#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstTypeByte : public AstBaseType
{
public:
	LETHE_AST_NODE(AstTypeByte)

	typedef AstBaseType Super;

	AstTypeByte(const TokenLocation &nloc) : Super(AST_TYPE_BYTE, nloc)
	{
		flags |= AST_F_RESOLVED;
	}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;
};


}
