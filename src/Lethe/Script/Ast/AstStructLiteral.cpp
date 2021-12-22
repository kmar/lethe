#include "AstStructLiteral.h"
#include "AstVarDecl.h"
#include "NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Stack.h>

namespace lethe
{

// AstStructLiteral

const AstNode *AstStructLiteral::GetTypeNode() const
{
	return nodes[0]->GetTypeNode();
}

QDataType AstStructLiteral::GetTypeDesc(const CompiledProgram &p) const
{
	return nodes[0]->GetTypeDesc(p);
}

// AstStructLiteral

bool AstStructLiteral::CodeGen(CompiledProgram &p)
{
	auto qdt = nodes[0]->GetTypeDesc(p);

	if (qdt.qualifiers & AST_Q_NOCOPY)
		return p.Error(nodes[0], "cannot initialize nocopy variable");

	if (qdt.IsReference())
		return p.Error(nodes[0], "struct literal cannot be a reference");

	bool noinit = nodes[1]->IsCompleteInitializerList(p, qdt) && !qdt.HasCtor() && !qdt.HasDtor();

	if ((qdt.qualifiers & (AST_Q_NOINIT | AST_Q_HAS_GAPS)) == AST_Q_HAS_GAPS)
		noinit = false;

	Int stkSize = (qdt.GetSize() + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;

	bool rvo = (flags & AST_F_NRVO) != 0;

	Int odelta = p.initializerDelta;

	if (!rvo)
	{
		p.EmitI24Zero(noinit ? OPC_PUSH_RAW : OPC_PUSHZ_RAW, stkSize);
		p.EmitCtor(qdt);

		p.PushStackType(qdt);
		p.initializerDelta -= stkSize * Stack::WORD_SIZE;
	}
	else
	{
		p.initializerDelta += scopeRef->varOfs - target->offset;
	}

	LETHE_RET_FALSE(nodes[1]->GenInitializerList(p, qdt, 0, false));
	p.initializerDelta = odelta;

	if (parent->type != AST_VAR_DECL)
	{
		if (!rvo)
			AstVarDecl::CallInit(p, nodes[0], -1);
		else
		{
			AstVarDecl::CallInit(p, nodes[0], -1, (scopeRef->varOfs - target->offset)/Stack::WORD_SIZE);
		}
	}

	return true;
}


}
