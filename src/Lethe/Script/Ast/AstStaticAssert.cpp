#include "AstStaticAssert.h"
#include "Constants/AstTextConstant.h"
#include <Lethe/Script/Program/CompiledProgram.h>

namespace lethe
{

// AstStaticAssert

bool AstStaticAssert::CodeGen(CompiledProgram &p)
{
	auto res = nodes[IDX_COND]->ToBoolConstant(p);

	if (res < 0)
		return p.Error(nodes[IDX_COND], "doesn't evaluate to a constant value");

	if (!res)
	{
		if (nodes.IsValidIndex(IDX_MESSAGE_OPT))
		{
			auto *tc = AstStaticCast<AstTextConstant *>(nodes[IDX_MESSAGE_OPT]);
			return p.Error(nodes[IDX_COND], String::Printf("static assertion failed: %s", tc->text.Ansi()));
		}

		return p.Error(nodes[IDX_COND], "static assertion failed");
	}

	return true;
}


}
