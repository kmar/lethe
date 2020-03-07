#include "AstConstFloat.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeFloat.h>

namespace lethe
{

// AstConstFloat

QDataType AstConstFloat::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_FLOAT];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstFloat::CodeGen(CompiledProgram &p)
{
	p.EmitFloatConst(num.f);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstFloat::GetTypeNode() const
{
	static AstTypeFloat tnode{TokenLocation()};
	return &tnode;
}

}
