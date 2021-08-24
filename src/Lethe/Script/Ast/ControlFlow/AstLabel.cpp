#include "AstLabel.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

#include "../NamedScope.h"

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(AstLabel)

// AstLabel

AstLabel::AstLabel(const String &ntext, const TokenLocation &nloc)
	: Super(ntext, AST_LABEL, nloc)
	, pc(-1)
	, varOfsBase(-1)
	, localVarSize(0)
	, deferredSize(0)
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

	varOfsBase = -1;
	deferredSize = 0;

	if (p.curScope)
	{
		varOfsBase = p.curScope->varOfs;
		localVarSize = p.curScope->localVars.GetSize();
		deferredSize = 0;

		// FIXME: is this ok?
		for (Int i=0; i<p.curScope->deferred.GetSize(); i++)
			if (p.curScope->deferred[i]->flags & AST_F_DEFER)
				deferredSize = i+1;
	}

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
	tmp->varOfsBase = varOfsBase;
	tmp->localVarSize = localVarSize;
	tmp->deferredSize = deferredSize;
	tmp->forwardTargets = forwardTargets;
}


}
