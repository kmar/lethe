#include "AstConstUInt.h"
#include <Lethe/Script/Ast/Types/AstTypeUInt.h>
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstConstUInt

QDataType AstConstUInt::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_UINT];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstUInt::CodeGen(CompiledProgram &p)
{
	p.EmitUIntConst(num.ui);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstUInt::GetTypeNode() const
{
	static AstTypeUInt tnode{TokenLocation()};
	return &tnode;
}

}
