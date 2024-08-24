#include "AstConstChar.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeChar.h>

namespace lethe
{

// AstConstChar

QDataType AstConstChar::GetTypeDesc(const CompiledProgram &p) const
{
	// special handling for enum items
	if (typeRef)
		return QDataType::MakeConstType(*typeRef);

	QDataType res;
	res.ref = &p.elemTypes[DT_CHAR];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstChar::CodeGen(CompiledProgram &p)
{
	p.EmitIntConst(num.i);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstChar::GetTypeNode() const
{
	static AstTypeChar tnode{TokenLocation()};
	return &tnode;
}

}
