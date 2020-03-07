#include "AstConstInt.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Ast/Types/AstTypeInt.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstConstInt)

// AstConstInt

QDataType AstConstInt::GetTypeDesc(const CompiledProgram &p) const
{
	// special handling for enum items
	if (typeRef)
		return QDataType::MakeConstType(*typeRef);

	QDataType res;
	res.ref = &p.elemTypes[DT_INT];
	res.qualifiers = AST_Q_CONST;
	return res;
}

bool AstConstInt::CodeGen(CompiledProgram &p)
{
	p.EmitIntConst(num.i);
	p.PushStackType(GetTypeDesc(p));
	return true;
}

const AstNode *AstConstInt::GetTypeNode() const
{
	static AstTypeInt tnode{TokenLocation()};
	return &tnode;
}

void AstConstInt::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstConstInt *>(n);
	tmp->typeRef = typeRef;
}

}
