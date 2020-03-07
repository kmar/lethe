#include "AstReturn.h"
#include "../NamedScope.h"
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

			if (!lv || resType.GetType() != lv->GetTypeDesc(p).GetType())
				return p.Error(this, "incompatible types");

			Int start = p.instructions.GetSize();
			LETHE_RET_FALSE(lv->CodeGenRef(p, 1));

			// FIXME: superhack to check if returning local var
			bool hasPushAdr = false;

			for (int i=start; i<p.instructions.GetSize(); i++)
			{
				auto ins = (p.instructions[i] & 255u);

				if (ins == OPC_LPUSHADR)
					hasPushAdr = true;

				// if LPUSHADR was used before a call, reset flag
				if (p.IsCall(ins))
					hasPushAdr = false;
			}

			if (hasPushAdr)
				return p.Error(nodes[0], "returning address of local variable");

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
