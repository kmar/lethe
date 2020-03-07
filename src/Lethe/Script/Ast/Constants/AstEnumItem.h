#pragma once

#include "../AstText.h"

namespace lethe
{

class LETHE_API AstEnumItem : public AstText
{
	LETHE_BUCKET_ALLOC(AstEnumItem)
public:
	SCRIPT_AST_NODE(AstEnumItem)

	typedef AstText Super;

	AstEnumItem(const String &ntext, const TokenLocation &nloc) : Super(ntext, AST_ENUM_ITEM, nloc) {}

	bool BeginCodegen(CompiledProgram &p) override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	bool TypeGen(CompiledProgram &p) override;

	void CopyTo(AstNode *n) const override;

private:
	friend class AstTypeEnum;
	QDataType typeRef;
};


}
