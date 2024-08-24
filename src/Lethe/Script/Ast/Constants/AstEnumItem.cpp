#include "AstEnumItem.h"
#include "AstConstInt.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstEnumItem)

// AstEnumItem

bool AstEnumItem::BeginCodegen(CompiledProgram &p)
{
	auto dt = new DataType;

	LETHE_ASSERT(parent && parent->type == AST_ENUM);
	dt->baseType = parent->GetTypeDesc(p);

	dt->type = dt->baseType.GetTypeEnumUnderlying();
	dt->size = dt->baseType.GetSize();
	dt->align = dt->baseType.GetAlign();

	dt->name = dt->GetName();

	typeRef = QDataType::MakeConstType(*p.AddType(dt));

	return Super::BeginCodegen(p);
}

QDataType AstEnumItem::GetTypeDesc(const CompiledProgram &) const
{
	LETHE_ASSERT(typeRef.IsNumber());

	return typeRef;
}

bool AstEnumItem::TypeGen(CompiledProgram &p)
{
	LETHE_ASSERT(!nodes.IsEmpty());

	auto qdt = GetTypeDesc(p);

	if (!nodes[0]->ConvertConstTo(qdt.GetTypeEnumUnderlying(), p))
		return p.Error(this, "enum item must be convertible to const integer");

	if (!(nodes[0]->type >= AST_CONST_BOOL && nodes[0]->type <= AST_CONST_ULONG))
		return p.Error(this, "enum item must be a constant integer expression");

	AstStaticCast<AstConstInt *>(nodes[0])->typeRef = typeRef.ref;

	return true;
}

void AstEnumItem::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstEnumItem *>(n);
	tmp->typeRef = typeRef;
}

}
