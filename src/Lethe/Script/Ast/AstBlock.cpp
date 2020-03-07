#include "AstBlock.h"
#include "NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstBlock

LETHE_BUCKET_ALLOC_DEF(AstBlock)

bool AstBlock::ResolveNode(const ErrorHandler &)
{
	if (nodes.IsEmpty())
		flags |= AST_F_RESOLVED;

	return true;
}

bool AstBlock::CodeGen(CompiledProgram &p)
{
	if (flags & AST_F_SKIP_CGEN)
		return true;

	p.EnterScope(scopeRef);

	Int lastStatement = -1;

	for (Int i=0; i<nodes.GetSize(); i++)
	{
		const auto ntype = nodes[i]->type;

		switch(ntype)
		{
		case AST_LABEL:
			lastStatement = -1;
			break;

		case AST_GOTO:
		case AST_BREAK:
		case AST_CONTINUE:
		case AST_RETURN:
		case AST_RETURN_VALUE:
			if (lastStatement < 0)
				lastStatement = i;

			break;

		default:
			;
		}

		if (lastStatement >= 0 && i > lastStatement)
		{
			// poor man's unreachable code detection; doesn't work in all cases
			p.Warning(nodes[i], "unreachable code");
			break;
		}

		p.SetLocation(nodes[i]->location);
		LETHE_RET_FALSE(nodes[i]->CodeGen(p));
	}

	if (!endOfBlockLocation.file.IsEmpty())
		p.SetLocation(endOfBlockLocation);

	// leaving scope now...
	if (scopeRef->type == NSCOPE_FUNCTION)
	{
		p.FixupReturnHandles();
		p.ProfExit();
	}

	return p.LeaveScope();
}


}
