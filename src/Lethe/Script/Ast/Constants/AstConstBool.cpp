#include "AstConstBool.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeBool.h>

namespace lethe
{

// AstConstBool

QDataType AstConstBool::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_BOOL];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstBool::CodeGen(CompiledProgram &p)
{
	p.EmitIntConst(num.i);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstBool::GetTypeNode() const
{
	static AstTypeBool tnode{TokenLocation()};
	return &tnode;
}

}
