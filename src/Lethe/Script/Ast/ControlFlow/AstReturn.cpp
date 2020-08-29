#include "AstReturn.h"
#include "../NamedScope.h"
#include "../AstSymbol.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Types/AstFuncBase.h>

namespace lethe
{

// AstReturn

bool AstReturn::CodeGen(CompiledProgram &p)
{
	if (!nodes.IsEmpty())
	{
		const AstNode *n = this;

		while (n && n->type != AST_FUNC)
			n = n->parent;

		LETHE_RET_FALSE(n);
		const AstFuncBase *fn = AstStaticCast<const AstFuncBase *>(n);

		auto resType = fn->GetResult()->GetTypeDesc(p);

		if (resType.IsReference())
		{
			// we have this: AST_EXPR / AST_BIN_ASSIGN...
			AstNode *lv = nodes[0]->nodes[0]->nodes.GetSize() > 1 ? nodes[0]->nodes[0]->nodes[1] : nullptr;

			QDataType lvdt;

			if (lv)
				lvdt = lv->GetTypeDesc(p);

			if (!lv || resType.GetType() != lvdt.GetType())
				return p.Error(this, "incompatible types");

			if (resType.IsReference() && !resType.IsConst() && lvdt.IsConst())
				return p.Error(lv, "cannot return non-const reference (input is const)");

			LETHE_RET_FALSE(lv->CodeGenRef(p, 1));

			auto *lvNode = lv->FindVarSymbolNode();
			const auto *lvScope = lvNode ? lvNode->scopeRef : scopeRef;

			if (lvNode && lvNode->symScopeRef)
				lvScope = lvNode->symScopeRef;

			if (lvNode && lvNode != lv)
				lvdt = lvNode->GetTypeDesc(p);

			if (!(lvdt.qualifiers & AST_Q_STATIC))
			{
				if (lvScope && lvScope->IsLocal() && !lvdt.IsReference())
					return p.Error(lv, "returning address of a local variable");

				// make sure we don't return non-const member ref from a const method
				if (lvScope && resType.IsReference() && !resType.IsConst() && lvScope->IsComposite() && scopeRef->IsConstMethod())
					return p.Error(lv, "cannot return non-const member reference from a const method");
			}

			// find result offset
			Int ofs = nodes[0]->nodes[0]->nodes[0]->target->offset;
			ofs = scopeRef->varSize - ofs;
			ofs /= Stack::WORD_SIZE;
			// we have 1 ptr on stack
			ofs++;
			p.EmitU24(OPC_LSTOREPTR, ofs);
			p.PopStackType(1);
		}
		else
		{
			auto ntype = nodes[0]->GetTypeDesc(p);

			if (resType.IsStruct() && !resType.CanAlias(ntype))
				return p.Error(nodes[0], String::Printf("cannot return %s derived struct by value", resType.GetName().Ansi()));

			LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		}
	}

	if (p.ReturnScope(!p.GetInline() && !p.GetProfiling()))
	{
		// bypass br if last return statement and can cleanup without calling dtors!
		p.Emit(OPC_RET);
	}
	else
		p.AddReturnHandle(p.EmitForwardJump(OPC_BR));

	if (type == AST_RETURN_VALUE || !nodes.IsEmpty())
		p.MarkReturnValue(-1);

	return true;
}


}
