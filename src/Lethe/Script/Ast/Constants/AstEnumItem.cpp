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
	dt->type = DT_INT;
	dt->size = dt->align = (Int)sizeof(Int);

	LETHE_ASSERT(parent && parent->type == AST_ENUM);
	dt->baseType = parent->GetTypeDesc(p);
	dt->name = dt->GetName();

	typeRef = QDataType::MakeConstType(*p.AddType(dt));

	return Super::BeginCodegen(p);
}

QDataType AstEnumItem::GetTypeDesc(const CompiledProgram &) const
{
	LETHE_ASSERT(typeRef.GetTypeEnum() == DT_INT);

	return typeRef;
}

bool AstEnumItem::TypeGen(CompiledProgram &p)
{
	LETHE_ASSERT(!nodes.IsEmpty());

	if (!nodes[0]->ConvertConstTo(DT_INT, p))
		return p.Error(this, "enum item must be convertible to const int");

	if (!(nodes[0]->type == AST_CONST_INT))
		return p.Error(this, "enum item must be a constant int expression");

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
