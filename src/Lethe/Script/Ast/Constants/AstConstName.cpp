#include "AstConstName.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeName.h>

namespace lethe
{

// AstConstName

AstConstName::AstConstName(const String &ntext, const TokenLocation &nloc)
	: Super(ntext, AST_CONST_NAME, nloc)
{
	num.ul = Name(ntext).GetValue();
}

QDataType AstConstName::GetTypeDesc(const CompiledProgram &p) const
{
	QDataType res;
	res.ref = &p.elemTypes[DT_NAME];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstName::CodeGen(CompiledProgram &p)
{
	Name n(text.Ansi());
	p.EmitNameConst(n);

	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstName::GetTypeNode() const
{
	static AstTypeName tnode{TokenLocation()};
	return &tnode;
}


}
