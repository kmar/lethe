#include "AstLabel.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstLabel)

// AstLabel

AstLabel::AstLabel(const String &ntext, const TokenLocation &nloc)
	: Super(ntext, AST_LABEL, nloc)
	, pc(-1)
{
}

bool AstLabel::BeginCodegen(CompiledProgram &p)
{
	if (!(flags & AST_F_REFERENCED))
	{
		p.Warning(this, String::Printf("unreferenced label `%s'", text.Ansi()));
		// don't report more
		flags |= AST_F_REFERENCED;
	}

	return true;
}

bool AstLabel::CodeGen(CompiledProgram &p)
{
	p.FlushOpt();
	pc = p.GetPc();

	for (auto &&it : forwardTargets)
		p.FixupForwardTarget(it);

	forwardTargets.Clear();

	return true;
}

void AstLabel::CopyTo(AstNode *n) const
{
	Super::CopyTo(n);
	auto *tmp = AstStaticCast<AstLabel *>(n);
	tmp->pc = pc;
	tmp->forwardTargets = forwardTargets;
}


}
