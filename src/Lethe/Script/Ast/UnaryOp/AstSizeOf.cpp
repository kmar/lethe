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
	// we must not call CodeGenComposite here
	return true;
}

bool AstSizeOf::FoldConst(const CompiledProgram &p)
{
	// we need to delay this due to problems
	if (!p.foldSizeof)
		return false;

	UniquePtr<AstConstInt> n;
	UniquePtr<AstConstName> nn;

	if (type != AST_TYPEID)
		n = new AstConstInt(location);

	auto *tn = const_cast<AstNode *>(nodes[0]->GetTypeNode());

	LETHE_RET_FALSE(tn);

	LETHE_RET_FALSE(TryTypeGen(tn, p));
	auto qdt = tn->GetTypeDesc(p);

	auto size = qdt.GetSize();
	auto align = qdt.GetAlign();

	// empty struct handling: we force virtual size and alignment to 1 byte here
	if (!size && qdt.GetTypeEnum() == DT_STRUCT)
		size = align = 1;

	// the problem here is that qdt can be DT_CLASS but in fact a pointer
	if (qdt.GetTypeEnum() == DT_CLASS)
	{
		if ((qdt.qualifiers & (AST_Q_RAW | AST_Q_WEAK)) || (nodes[0]->target && nodes[0]->target->type == AST_VAR_DECL))
		{
			// it must be a pointer
			size = align = (Int)sizeof(IntPtr);
		}
	}

	switch(type)
	{
	case AST_SIZEOF:
		n->num.i = size;
		break;

	case AST_ALIGNOF:
		n->num.i = align;
		break;

	case AST_OFFSETOF:
	case AST_BITSIZEOF:
	case AST_BITOFFSETOF:
	{
		n->num.i = -1;

		// this will be tricky!
		if (!nodes[0]->symScopeRef || nodes[0]->type != AST_IDENT)
			break;

		auto *sr = nodes[0]->symScopeRef;

		if (!sr->IsComposite() || !sr->node)
			break;

		if (!TryTypeGen(sr->node, p))
			break;

		auto ctype = sr->node->GetTypeDesc(p);

		auto &txt = AstStaticCast<AstText *>(nodes[0])->text;

		for (auto &&it : ctype.GetType().members)
		{
			if (it.name == txt)
			{
				if (type == AST_OFFSETOF)
					n->num.i = (Int)it.offset;
				else if (type == AST_BITOFFSETOF)
					n->num.i = it.bitOffset;
				else
					n->num.i = it.bitSize;
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
