#include "AstControl.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstControl

bool AstControl::CodeGenBoolExpr(AstNode *n, CompiledProgram &p, bool varScope)
{
	LETHE_RET_FALSE(n->CodeGen(p));

	if (varScope)
		n->LoadIfVarDecl(p);

	if (p.exprStack.IsEmpty())
		return p.Error(n, "conditional expression must return a value");

	const auto &qdt = p.exprStack.Back();

	auto res = (qdt.IsNumber() && !qdt.IsLongInt()) || p.EmitConv(n, qdt, p.elemTypes[DT_BOOL]);
	LETHE_RET_FALSE(res);

	// this should help JIT + &&/|| a bit
	if (p.instructions.Back() == OPC_ICMPNZ)
		p.instructions.Back() = OPC_NOP;

	return true;
}


}
