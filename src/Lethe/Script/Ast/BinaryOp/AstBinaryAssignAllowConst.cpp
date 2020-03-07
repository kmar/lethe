#include "AstBinaryAssignAllowConst.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstBinaryAssignAllowConst

bool AstBinaryAssignAllowConst::CodeGen(CompiledProgram &p)
{
	return CodeGenMaybeConst(p, true);
}


}
