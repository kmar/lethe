#pragma once

#include "AstCustomType.h"
#include <Lethe/Script/TypeInfo/Attributes.h>

namespace lethe
{

LETHE_API_BEGIN

class LETHE_API AstTypeEnum : public AstCustomType
{
	LETHE_BUCKET_ALLOC(AstTypeEnum)

public:
	LETHE_AST_NODE(AstTypeEnum)

	enum
	{
		IDX_NAME = 0,
		IDX_UNDERLYING = 1,
		IDX_FIRST_ITEM = 2
	};

	typedef AstCustomType Super;

	AstTypeEnum(const TokenLocation &nloc) : Super(AST_ENUM, nloc) {}

	bool BeginCodegen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool TypeGen(CompiledProgram &p) override;

	bool GetTemplateTypeText(StringBuilder &sb) const override;

	SharedPtr<Attributes> attributes;
};

LETHE_API_END

}
