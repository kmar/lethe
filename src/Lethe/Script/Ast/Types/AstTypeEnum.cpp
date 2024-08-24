#include "AstTypeEnum.h"
#include "../AstText.h"
#include <Lethe/Script/Ast/Constants/AstEnumItem.h>
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

LETHE_AST_BUCKET_ALLOC_DEF(AstTypeEnum)

// AstTypeEnum

bool AstTypeEnum::BeginCodegen(CompiledProgram &p)
{
	auto dt = const_cast<DataType *>(p.AddType(new DataType));
	dt->type = DT_ENUM;

	// elemType = underlying type for an enum
	dt->elemType = nodes[IDX_UNDERLYING]->GetTypeDesc(p);
	auto *underlying = dt->elemType.ref;

	dt->size = underlying->size;
	dt->align = underlying->align;

	if (!underlying->IsInteger())
		return p.Error(nodes[IDX_UNDERLYING], "underlying type must be an integer");

	if (underlying->type == DT_ENUM)
		return p.Error(nodes[IDX_UNDERLYING], "underlying type cannot be another enum");

	if (nodes[0]->type != AST_EMPTY)
		dt->name = AstStaticCast<AstText *>(nodes[0])->GetQText(p);

	typeRef.ref = dt;

	return Super::BeginCodegen(p);
}

QDataType AstTypeEnum::GetTypeDesc(const CompiledProgram &) const
{
	return typeRef;
}

bool AstTypeEnum::TypeGen(CompiledProgram &p)
{
	if (nodes[0]->type == AST_EMPTY)
		return true;

	// check if already generated
	if (flags & AST_F_TYPE_GEN)
		return true;

	// try to create virtual type
	auto dt = const_cast<DataType *>(typeRef.ref);
	LETHE_ASSERT(dt);

	typeRef.qualifiers = qualifiers;

	// generate new type...
	for (Int i=IDX_FIRST_ITEM; i<nodes.GetSize(); i++)
	{
		auto n = AstStaticCast<AstEnumItem *>(nodes[i]);
		DataType::Member m;
		m.name = n->text;
		m.type = n->typeRef;
		m.offset = (Long)n->nodes[0]->num.ul;
		dt->members.Add(m);
	}

	flags |= AST_F_TYPE_GEN;

	dt->attributes = attributes;

	return Super::TypeGen(p);
}

bool AstTypeEnum::GetTemplateTypeText(StringBuilder &sb) const
{
	sb.AppendFormat("enum ");
	return nodes[0]->GetTemplateTypeText(sb);
}


}
