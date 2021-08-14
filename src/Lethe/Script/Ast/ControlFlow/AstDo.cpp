#include "AstDo.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstDo

bool AstDo::CodeGen(CompiledProgram &p)
{
	// [0] = body, [1] = cond, [2] = (opt)nobreak
	p.FlushOpt();

	Int bconst = nodes[1]->ToBoolConstant(p);

	Int body = p.instructions.GetSize();

	// if condition is false, there's no loop here
	if (p.GetJitFriendly() && bconst != 0)
		p.loops.Add(body);

	// body
	auto olc = p.GetLatentCounter();
	LETHE_RET_FALSE(nodes[0]->CodeGen(p));

	Int bodyWeight = Max<Int>(p.instructions.GetSize() - body, 1);

	LETHE_ASSERT(scopeRef->type == NSCOPE_LOOP);
	scopeRef->FixupContinueHandles(p);

	// also: no unrolling if latent call used inside body
	if (p.GetJitFriendly() && bconst != 0 && olc == p.GetLatentCounter())
	{
		// TODO: of course I should do some analysis on real bounds of the for loop so that we don't unroll more iters than the loop will actualy run

		Int fwdExit[UNROLL_MAX_COUNT];

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			fwdExit[i] = -1;

		// trying world's dumbest unrolling, but it might help a lot in tiny loops due to persistent caching in regs
		Int unrollCount = Min<Int>(UNROLL_MAX_COUNT*UNROLL_MIN_WEIGHT / bodyWeight, UNROLL_MAX_COUNT)-1;

		// this is broken for do-while!
		for (Int i=0; i<unrollCount; i++)
		{
			if (bconst != 1)
			{
				LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
				// flip condition
				DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
				fwdExit[1+i] = p.EmitForwardJump(p.ConvJump(dt, OPC_IBZ_P));
				p.PopStackType();
			}

			LETHE_RET_FALSE(nodes[0]->CodeGen(p));
		}

		if (bconst < 0)
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();
		}

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			if (fwdExit[i] >= 0)
				p.FixupForwardTarget(fwdExit[i]);

		// handle nobreak (unless infinite loop)
		if (bconst < 0 && nodes.GetSize() > 2)
			LETHE_RET_FALSE(nodes[2]->CodeGenNoBreak(p));
	}
	else
	{
		if (bconst < 0)
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();

			// handle nobreak
			if (nodes.GetSize() > 2)
				LETHE_RET_FALSE(nodes[2]->CodeGenNoBreak(p));
		}
		else if (bconst == 1)
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_BR, body));
	}

	scopeRef->FixupBreakHandles(p);
	return true;
}


}
