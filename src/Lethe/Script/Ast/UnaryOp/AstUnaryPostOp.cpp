#include "AstUnaryPostOp.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/CodeGenTables.h>

namespace lethe
{

// AstUnaryPostOp

bool AstUnaryPostOp::CodeGen(CompiledProgram &p)
{
	bool shouldpop = false;

	if (!nodes[0]->IsConstExpr())
	{
		LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		shouldpop = true;
	}

	LETHE_RET_FALSE(nodes[0]->CodeGenRef(p));
	const QDataType &dt = nodes[0]->GetTypeDesc(p);

	bool pop = ShouldPop();

	if (!shouldpop && type == AST_UOP_POSTINC && dt.GetTypeEnum() == DT_ARRAY_REF)
	{
		p.PopStackType(true);

		if (!pop)
		{
			// reserve space for ptr
			p.EmitI24(OPC_PUSH_RAW, 1);
			p.EmitI24(OPC_LPUSHPTR, 1);

			// copy out aref
			p.Emit(OPC_LPUSHPTR);
			p.Emit(OPC_PLOADPTR_IMM);
			p.EmitI24(OPC_LSTOREPTR, 2);
			p.Emit(OPC_LPUSHPTR);
			p.EmitI24(OPC_PLOAD32_IMM, (Int)sizeof(void *));
			p.EmitI24(OPC_LSTORE32, 3);
			auto pdt = dt;
			pdt.RemoveReference();
			p.PushStackType(pdt);
		}

		// allow preinc for array refs
		p.EmitIntConst(1);
		p.EmitIntConst(dt.GetType().elemType.GetSize());
		p.EmitI24(OPC_BCALL, BUILTIN_SLICEFWD_INPLACE);

		p.EmitI24(OPC_POP, 1);

		return true;
	}

	if (dt.GetTypeEnum() <= DT_BOOL || dt.GetTypeEnum() >= DT_FLOAT)
		return p.Error(this, "invalid type for post-op");

	const Int amt = type == AST_UOP_POSTINC ? 1 : -1;

	Int &lins = p.instructions.Back();
	UInt linsOpc = lins & 255u;

	if ((dt.GetTypeEnum() == DT_INT || dt.GetTypeEnum() == DT_UINT) && linsOpc == OPC_LPUSHADR)
	{
		Int loffset = lins >> 8;

		if (loffset >= 0 && loffset < 255)
		{
			// shortcut...
			if (!pop)
			{
				lins = OPC_LPUSH32 + (loffset << 8);
				++loffset;
				LETHE_ASSERT(loffset <= 255);
				p.Emit(OPC_LIADD_ICONST + Int((UInt)amt << 24) + (loffset << 8) + (loffset << 16));
				return true;
			}

			lins = OPC_LIADD_ICONST + Int((UInt)amt << 24) + (loffset << 8) + (loffset << 16);
			p.PopStackType(1);
			return true;
		}
	}

	p.EmitI24(opcodeRefIncPost[dt.GetTypeEnum()], amt);
	p.PopStackType(1);

	if (shouldpop)
		p.Emit(OPC_POP + (1 << 8));
	else
		p.PushStackType(dt);

	return true;
}


}
