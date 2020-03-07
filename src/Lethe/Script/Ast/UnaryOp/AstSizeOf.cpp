#include "AstSizeOf.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Constants/AstConstInt.h>

namespace lethe
{

// AstSizeOf

bool AstSizeOf::FoldConst(const CompiledProgram &p)
{
	UniquePtr<AstConstInt> n = new AstConstInt(location);

	auto *tn = const_cast<AstNode *>(nodes[0]->GetTypeNode());

	LETHE_RET_FALSE(tn);

	// FIXME: this asks for trouble!
	LETHE_RET_FALSE(tn->TypeGenDef(const_cast<CompiledProgram &>(p)));
	LETHE_RET_FALSE(tn->TypeGen(const_cast<CompiledProgram &>(p)));
	LETHE_RET_FALSE(tn->CodeGenComposite(const_cast<CompiledProgram &>(p)));
	auto qdt = nodes[0]->GetTypeDesc(p);
	n->num.i = qdt.GetSize();

	parent->ReplaceChild(this, n.Detach());
	delete this;
	return true;
}

QDataType AstSizeOf::GetTypeDesc(const CompiledProgram &p) const
{
	return QDataType::MakeConstType(p.elemTypes[DT_INT]);
}

}
