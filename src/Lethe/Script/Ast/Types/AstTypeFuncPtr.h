#pragma once

#include "AstFuncBase.h"

namespace lethe
{

class LETHE_API AstTypeFuncPtr : public AstFuncBase
{
	LETHE_BUCKET_ALLOC(AstTypeFuncPtr)

public:
	SCRIPT_AST_NODE(AstTypeFuncPtr)

	typedef AstFuncBase Super;

	AstTypeFuncPtr(const TokenLocation &nloc) : Super(AST_TYPE_FUNC_PTR, nloc) {}

	QDataType GetTypeDesc(const CompiledProgram &p) const override;
	const AstNode *GetTypeNode() const override;
	bool TypeGen(CompiledProgram &p) override;

	inline AstNode *GetArgs() const override
	{
		return nodes[1];
	}

	bool GetTemplateTypeText(StringBuilder &sb) const override;

	void CopyTo(AstNode *n) const override;

protected:
	QDataType typeRef;
};


}
