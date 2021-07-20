#include "AstTypeDynamicArray.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstTypeDynamicArray)

// AstTypeDynamicArray

AstTypeDynamicArray::~AstTypeDynamicArray()
{
	// don't allow double free of shared type node
	if (aref)
		aref->nodes.Clear();
}

bool AstTypeDynamicArray::TypeGen(CompiledProgram &p)
{
	return Super::TypeGen(p) && (aref ? aref->TypeGen(p) : true);
}

AstNode::ResolveResult AstTypeDynamicArray::Resolve(const ErrorHandler &e)
{
	auto res = Super::Resolve(e);

	if ((flags & AST_F_RESOLVED) && aref)
		aref->flags |= AST_F_RESOLVED;

	return res;
}

AstNode *AstTypeDynamicArray::GetArrayRefNode()
{
	if (!aref)
	{
		aref = new AstTypeArrayRef(location);
		aref->nodes.Add(nodes[0]);
	}

	return aref;
}

void AstTypeDynamicArray::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstTypeDynamicArray *>(n);

	if (aref)
		tmp->GetArrayRefNode();
}


}
