#include "AstBinaryAssignAllowConst.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstBinaryAssignAllowConst

bool AstBinaryAssignAllowConst::CodeGen(CompiledProgram &p)
{
	if (nodes[0]->qualifiers & AST_Q_BITFIELD)
		return Super::CodeGen(p);

	return CodeGenMaybeConst(p, true);
}


}
