#include "AstConstLong.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeLong.h>

namespace lethe
{

// AstConstLong

QDataType AstConstLong::GetTypeDesc(const CompiledProgram &p) const
{
	// special handling for enum items
	if (typeRef)
		return QDataType::MakeConstType(*typeRef);

	QDataType res;
	res.ref = &p.elemTypes[DT_LONG];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstLong::CodeGen(CompiledProgram &p)
{
	p.EmitLongConst(num.l);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstLong::GetTypeNode() const
{
	static AstTypeLong tnode{TokenLocation()};
	return &tnode;
}

}
