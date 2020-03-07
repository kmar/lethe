#include "AstTypeArray.h"
#include "AstTypeArrayRef.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstTypeArray

bool AstTypeArray::CodeGenComposite(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::CodeGenComposite(p));
	QDataType qdt = GetTypeDesc(p);

	if (qdt.HasDtor())
	{
		const DataType &dt = *qdt.ref;

		if (!dt.GenDtor(p))
			return p.Error(this, "failed to generate destructors");
	}

	if (qdt.HasCtor())
	{
		const DataType &dt = *qdt.ref;

		if (!dt.GenCtor(p))
			return p.Error(this, "failed to generate constructors");
	}

	return true;
}

bool AstTypeArray::TypeGen(CompiledProgram &p)
{
	LETHE_RET_FALSE(Super::TypeGen(p));

	bool isStatic = nodes.GetSize() > 1;

	auto arrType = isStatic ? DT_STATIC_ARRAY : DT_DYNAMIC_ARRAY;

	// check if already generated
	if (typeRef.ref && typeRef.ref->type == arrType)
		return true;

	// try to create virtual type
	UniquePtr<DataType> dt = new DataType;

	typeRef.qualifiers = qualifiers;

	if (!isStatic)
		typeRef.qualifiers |= AST_Q_DTOR;

	dt->type = arrType;

	if (isStatic)
	{
		AstNode *dconst = nodes[1]->DerefConstant(p);

		if (!dconst)
			dconst = nodes[1];

		if (dconst->type != AST_CONST_INT)
			return p.Error(this, "static array dims must be constant integer");

		Int dims = dconst->num.i;
		dt->arrayDims = (UInt)dims;

		if (dims <= 0)
			return p.Error(this, "invalid array dims (must be > 0)");
	}

	{
		// generate complementary type (array ref)
		AstTypeArrayRef aref(location);
		aref.nodes.Add(nodes[0]);
		bool failed = aref.TypeGen(p);
		aref.nodes.Clear();
		LETHE_RET_FALSE(failed);
		dt->complementaryType = aref.GetTypeDesc(p).ref;
	}

	dt->elemType = nodes[0]->GetTypeDesc(p);

	// we need to check for zero size as well in the case a struct didn't generate a type
	if (dt->elemType.GetTypeEnum() == DT_NONE || !dt->elemType.GetSize())
	{
		LETHE_RET_FALSE(nodes[0]->target ? nodes[0]->target->TypeGen(p) : nodes[0]->TypeGen(p));
		dt->elemType = nodes[0]->GetTypeDesc(p);

		if (dt->elemType.GetTypeEnum() == DT_NONE)
			return p.Error(this, "couldn't generate array elem type!");
	}

	dt->elemType.qualifiers &= ~(AST_Q_SKIP_DTOR | AST_Q_LOCAL_INT);

	if (!isStatic && dt->elemType.GetTypeEnum() == DT_STATIC_ARRAY)
		return p.Error(this, "dynamic arrays of static arrays not allowed");

	// now compute size and alignment
	if (isStatic)
	{
		dt->align = dt->elemType.GetAlign();
		dt->size = dt->arrayDims*dt->elemType.GetSize();
	}
	else
	{
		Int align = sizeof(void *);
		Int size = align + 2*sizeof(int);

		size += align-1;
		size = size/align * align;

		dt->align = align;
		dt->size = size;
	}

	dt->name = dt->GetName();
	typeRef.ref = p.AddType(dt.Detach());

	if (!isStatic)
		LETHE_RET_FALSE(typeRef.ref->GenDynArr(p));

	if (typeRef.HasCtor())
		typeRef.qualifiers |= AST_Q_CTOR;

	if (typeRef.HasDtor())
		typeRef.qualifiers |= AST_Q_DTOR;

	return true;
}

bool AstTypeArray::GetTemplateTypeText(StringBuilder &sb) const
{
	bool isStatic = nodes.GetSize() > 1;

	// cannot put static arrays as template arguments
	if (isStatic)
		return false;

	sb.AppendFormat("array<");

	nodes[0]->AppendTypeQualifiers(sb);
	LETHE_RET_FALSE(nodes[0]->GetTemplateTypeText(sb));

	sb.AppendFormat(">");

	return true;
}


}
