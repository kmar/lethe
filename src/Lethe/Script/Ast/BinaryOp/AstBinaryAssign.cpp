#include "AstBinaryAssign.h"
#include "../AstSymbol.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstBinaryAssign

AstNode *AstBinaryAssign::GetResolveTarget() const
{
		return nodes[0]->GetResolveTarget();
}

bool AstBinaryAssign::CodeGenRef(CompiledProgram &p, bool allowConst, bool)
{
	// not supported
	(void)allowConst;
	return p.Error(this, "refs for assignment not supported");
}

bool AstBinaryAssign::CodeGenMaybeConst(CompiledProgram &p, bool allowConst)
{
	QDataType dt = nodes[1]->GetTypeDesc(p);

	// prefer refs if possible for structs
	if (dt.IsStruct() && !dt.IsReference() && !dt.IsPointer() && nodes[1]->CanPassByReference(p))
		LETHE_RET_FALSE(nodes[1]->CodeGenRef(p, true));
	else
		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

	if (p.exprStack.IsEmpty())
		return p.Error(nodes[1], "rhs for assigment must return a value");

	return CodeGenCommon(p, true, false, allowConst);
}

bool AstBinaryAssign::CodeGen(CompiledProgram &p)
{
	auto *rnode = nodes[0];

	if (rnode->type == AST_OP_DOT)
		rnode = rnode->nodes[1];

	if (rnode->qualifiers & AST_Q_PROPERTY)
		return AstSymbol::CallPropertySetter(p, nodes[0], nodes[1]);

	if (rnode->qualifiers & AST_Q_BITFIELD)
		return AstSymbol::BitfieldStore(p, rnode, nodes[0], nodes[1]);

	return CodeGenMaybeConst(p, false);
}


}
