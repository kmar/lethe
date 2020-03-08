#pragma once

#include "AstBaseType.h"

namespace lethe
{

class LETHE_API AstCustomType : public AstBaseType
{
	LETHE_BUCKET_ALLOC(AstCustomType)

public:
	LETHE_AST_NODE(AstCustomType)

	typedef AstBaseType Super;

	AstCustomType(AstNodeType ntype, const TokenLocation &nloc) : Super(ntype, nloc) {}

	const AstNode *GetTypeNode() const override;
	AstNode *GetResolveTarget() const override;
	QDataType GetTypeDesc(const CompiledProgram &p) const override;

	void CopyTo(AstNode *n) const override;

protected:
	QDataType typeRef;
};

}
