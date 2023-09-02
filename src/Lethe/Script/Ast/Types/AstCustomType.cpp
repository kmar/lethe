#include "AstCustomType.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstCustomType)

// AstCustomType

const AstNode *AstCustomType::GetTypeNode() const
{
	return this;
}

AstNode *AstCustomType::GetResolveTarget() const
{
	return const_cast<AstCustomType *>(this);
}

QDataType AstCustomType::GetTypeDesc(const CompiledProgram &) const
{
	return typeRef;
}

void AstCustomType::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstCustomType *>(n);
	tmp->typeRef = typeRef;
}


}
