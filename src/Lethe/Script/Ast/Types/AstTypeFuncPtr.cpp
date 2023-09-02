#include "AstTypeFuncPtr.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstTypeFuncPtr)

// AstTypeFuncPtr

const AstNode *AstTypeFuncPtr::GetTypeNode() const
{
	return this;
}

QDataType AstTypeFuncPtr::GetTypeDesc(const CompiledProgram &) const
{
	return typeRef;
}

bool AstTypeFuncPtr::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));
	// [0] = return type
	// [1] = arglist
	// try to create virtual type
	typeRef.qualifiers = qualifiers;
	typeRef.ref = p.AddType(GenFuncType(this, p, nodes[0], nodes[1]->nodes));
	return true;
}

bool AstTypeFuncPtr::GetTemplateTypeText(StringBuilder &sb) const
{
	nodes[0]->AppendTypeQualifiers(sb);
	LETHE_RET_FALSE(nodes[0]->GetTemplateTypeText(sb));

	auto *args = GetArgs()->nodes[0];
	sb.AppendFormat(type == AST_TYPE_DELEGATE ? " delegate(" : " function(");

	for (Int i=0; i<args->nodes.GetSize(); i+=2)
	{
		args->nodes[i]->AppendTypeQualifiers(sb);
		LETHE_RET_FALSE(args->nodes[i]->GetTemplateTypeText(sb));

		if (i+2 < args->nodes.GetSize())
			sb.AppendFormat(",");
	}

	sb.AppendFormat(")");
	return true;
}

void AstTypeFuncPtr::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstTypeFuncPtr *>(n);
	tmp->typeRef = typeRef;
}


}
