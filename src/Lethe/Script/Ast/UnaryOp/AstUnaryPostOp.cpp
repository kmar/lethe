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

	if (dt.GetTypeEnum() <= DT_BOOL || dt.GetTypeEnum() >= DT_FLOAT)
		return p.Error(this, "invalid type for post-op");

	const Int amt = type == AST_UOP_POSTINC ? 1 : -1;

	bool pop = ShouldPop();
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
