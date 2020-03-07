#include "AstConstULong.h"
#include <Lethe/Script/Ast/Types/AstTypeULong.h>
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstConstULong

QDataType AstConstULong::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_ULONG];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstULong::CodeGen(CompiledProgram &p)
{
	p.EmitULongConst(num.ul);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstULong::GetTypeNode() const
{
	static AstTypeULong tnode{TokenLocation()};
	return &tnode;
}

}
