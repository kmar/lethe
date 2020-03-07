#include "AstConstNull.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstConstNull

QDataType AstConstNull::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_NULL];
	res.qualifiers = AST_Q_CONST | AST_Q_SKIP_DTOR;
	return res;
}

bool AstConstNull::CodeGen(CompiledProgram &p)
{
	p.EmitI24(OPC_PUSHZ_RAW, 1);
	p.PushStackType(GetTypeDesc(p));
	return true;
}


}
