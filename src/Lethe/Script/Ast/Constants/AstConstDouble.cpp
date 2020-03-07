#include "AstConstDouble.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeDouble.h>

namespace lethe
{

// AstConstDouble

QDataType AstConstDouble::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_DOUBLE];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstDouble::CodeGen(CompiledProgram &p)
{
	p.EmitDoubleConst(num.d);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstDouble::GetTypeNode() const
{
	static AstTypeDouble tnode{TokenLocation()};
	return &tnode;
}

}
