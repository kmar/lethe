#include "AstThis.h"
#include "NamedScope.h"
#include "Types/AstTypeClass.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstThis

const AstNode *AstThis::GetTypeNode() const
{
	const auto *tscope = scopeRef->FindThis();

	return tscope ? tscope->node : nullptr;
}

QDataType AstThis::GetTypeDesc(const CompiledProgram &p) const
{
	const auto *tscope = scopeRef->FindThis();

	if (!tscope)
		return QDataType();

	if (tscope->node->type == AST_CLASS)
	{
		auto qdt = AstStaticCast<AstTypeClass *>(tscope->node)->GetTypeDescPtr(DT_RAW_PTR);
		return qdt;
	}

	QDataType res = tscope->node->GetTypeDesc(p);
	res.qualifiers |= AST_Q_REFERENCE;
	return res;
}

bool AstThis::ResolveNode(const ErrorHandler &)
{
	const NamedScope *tscope = scopeRef->FindThis();

	if (!tscope)
		return true;

	target = tscope->node;
	flags |= AST_F_RESOLVED;
	return true;
}

bool AstThis::CodeGen(CompiledProgram &p)
{
	auto thisScope = scopeRef->FindThis();

	if (!thisScope || !thisScope->node)
		return p.Error(this, "this not found");

	auto ntype = thisScope->node->type;

	if (ntype != AST_CLASS && ntype != AST_STRUCT)
		return p.Error(this, "this ptr only available for classes/structs");

	if (ntype == AST_STRUCT)
	{
		auto *thisStruct = AstStaticCast<AstTypeStruct *>(thisScope->node);
		p.Emit(OPC_PUSHTHIS_TEMP);
		return EmitPtrLoad(thisStruct->GetTypeDesc(p), p);
	}

	auto *thisClass = AstStaticCast<AstTypeClass *>(thisScope->node);
	p.Emit(OPC_PUSHTHIS_TEMP);
	p.PushStackType(thisClass->GetTypeDescPtr(DT_RAW_PTR));
	return true;
}

bool AstThis::CodeGenRef(CompiledProgram &p, bool allowConst, bool)
{
	if (!allowConst && scopeRef->IsConstMethod())
		return p.Error(this, "this is const");

	if (!scopeRef->FindThis())
		return p.Error(this, "this not accessible from here");

	p.Emit(OPC_PUSHTHIS_TEMP);
	p.PushStackType(GetTypeDesc(p));
	return true;
}


}
