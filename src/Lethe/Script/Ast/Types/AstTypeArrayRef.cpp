#include "AstTypeArrayRef.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>

namespace lethe
{

// AstTypeArrayRef

bool AstTypeArrayRef::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));

	// check if already generated
	if (typeRef.ref && typeRef.ref->type == DT_ARRAY_REF)
		return true;

	// try to create virtual type
	UniquePtr<DataType> dt = new DataType;

	typeRef.qualifiers = qualifiers;
	typeRef.qualifiers &= ~AST_Q_DTOR;

	dt->type = DT_ARRAY_REF;
	dt->elemType = nodes[IDX_TYPE]->GetTypeDesc(p);

	if (dt->elemType.GetTypeEnum() == DT_NONE)
	{
		LETHE_RET_FALSE(nodes[IDX_TYPE]->target ? nodes[IDX_TYPE]->target->TypeGen(p) : nodes[IDX_TYPE]->TypeGen(p));
		dt->elemType = nodes[IDX_TYPE]->GetTypeDesc(p);

		if (dt->elemType.GetTypeEnum() == DT_NONE)
			return p.Error(this, "couldn't generate array ref elem type!");
	}

	// now compute size and alignment
	dt->align = Stack::WORD_SIZE;
	dt->size = 2*dt->align;

	dt->name = dt->GetName();

	typeRef.ref = p.AddType(dt.Detach());
	return true;
}

bool AstTypeArrayRef::GetTemplateTypeText(StringBuilder &sb) const
{
	sb += "[](";
	nodes[0]->AppendTypeQualifiers(sb);
	LETHE_RET_FALSE(nodes[0]->GetTemplateTypeText(sb));
	sb += ')';
	return true;
}


}
