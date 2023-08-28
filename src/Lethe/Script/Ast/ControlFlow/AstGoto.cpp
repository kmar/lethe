#include "AstGoto.h"
#include "AstLabel.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstGoto

bool AstGoto::ResolveNode(const ErrorHandler &eh)
{
	if (flags & AST_F_RESOLVED)
		return true;

	target = scopeRef->FindLabel(text);

	if (!target)
	{
		eh.Error(this, String::Printf("target label not found: `%s' (maybe trying to jump over var decl)", text.Ansi()));
		return false;
	}

	target->flags |= AST_F_REFERENCED;
	flags |= AST_F_RESOLVED;
	return true;
}

bool AstGoto::CodeGen(CompiledProgram &p)
{
	p.SetLocation(location);

	auto *label = AstStaticCast<AstLabel *>(target);

	LETHE_RET_FALSE(p.GotoScope(label));

	if (label->pc >= 0)
		p.EmitBackwardJump(OPC_BR, label->pc);
	else
		label->forwardTargets.Add(p.EmitForwardJump(OPC_BR));

	return true;
}


}
