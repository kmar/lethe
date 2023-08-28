#include "AstBreak.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstBreak

bool AstBreak::CodeGen(CompiledProgram &p)
{
	if (flags & AST_F_SKIP_CGEN)
		return true;

	p.SetLocation(location);

	LETHE_ASSERT(scopeRef);
	// note: must be separate because of eval order!
	NamedScope *bscope = p.BreakScope();
	bscope->AddBreakHandle(p.EmitForwardJump(OPC_BR));
	return true;
}


}
