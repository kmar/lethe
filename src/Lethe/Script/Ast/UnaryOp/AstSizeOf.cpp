#include "AstSizeOf.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Ast/Constants/AstConstName.h>
#include <Lethe/Script/Ast/NamedScope.h>

namespace lethe
{

// AstSizeOf

bool AstSizeOf::TryTypeGen(AstNode *tn, const CompiledProgram &p)
{
	LETHE_RET_FALSE(tn);
	// FIXME: this asks for trouble!
	LETHE_RET_FALSE(tn->TypeGenDef(const_cast<CompiledProgram &>(p)));
	LETHE_RET_FALSE(tn->TypeGen(const_cast<CompiledProgram &>(p)));
	return tn->CodeGenComposite(const_cast<CompiledProgram &>(p));
}

bool AstSizeOf::FoldConst(const CompiledProgram &p)
{
	UniquePtr<AstConstInt> n;
	UniquePtr<AstConstName> nn;

	if (type != AST_TYPEID)
		n = new AstConstInt(location);

	auto *tn = const_cast<AstNode *>(nodes[0]->GetTypeNode());

	LETHE_RET_FALSE(tn);

	LETHE_RET_FALSE(TryTypeGen(tn, p));
	auto qdt = nodes[0]->GetTypeDesc(p);

	switch(type)
	{
	case AST_SIZEOF:
		n->num.i = qdt.GetSize();
		break;

	case AST_ALIGNOF:
		n->num.i = qdt.GetAlign();
		break;

	case AST_OFFSETOF:
	{
		n->num.i = -1;

		// this will be tricky!
		if (!nodes[0]->symScopeRef || nodes[0]->type != AST_IDENT)
			break;

		auto *sr = nodes[0]->symScopeRef;

		if (!sr->IsComposite() || !sr->node)
			break;

		auto ctype = sr->node->GetTypeDesc(p);

		auto &txt = AstStaticCast<AstText *>(nodes[0])->text;

		for (auto &&it : ctype.GetType().members)
		{
			if (it.name == txt)
			{
				n->num.i = it.offset;
				break;
			}
		}
		break;
	}

	case AST_TYPEID:
		nn = new AstConstName(qdt.GetName(), location);
		break;

	default:;
	}

	AstNode *newNode = type == AST_TYPEID ? static_cast<AstNode *>(nn.Detach()) : static_cast<AstNode *>(n.Detach());
	parent->ReplaceChild(this, newNode);
	delete this;
	return true;
}

QDataType AstSizeOf::GetTypeDesc(const CompiledProgram &p) const
{
	return QDataType::MakeConstType(type == AST_TYPEID ? p.elemTypes[DT_NAME] : p.elemTypes[DT_INT]);
}

}
