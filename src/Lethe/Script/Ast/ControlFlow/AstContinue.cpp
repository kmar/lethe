#include "AstContinue.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstContinue

bool AstContinue::CodeGen(CompiledProgram &p)
{
	LETHE_ASSERT(scopeRef);
	// note: must be separate because of eval order!
	NamedScope *bscope = p.BreakScope(1);
	bscope->AddContinueHandle(p.EmitForwardJump(OPC_BR));
	return true;
}


}
