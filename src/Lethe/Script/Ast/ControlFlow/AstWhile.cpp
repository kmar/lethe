#include "AstWhile.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>

namespace lethe
{

// AstWhile

bool AstWhile::CodeGen(CompiledProgram &p)
{
	// [0] = cond, [1] = body, [2] = (opt)nobreak
	Int bconst = nodes[0]->ToBoolConstant(p);

	if (bconst == 0)
		return 1;

	/* transform while(expr) stmt
		into:
		br expr
		stmt
		expr
		br_backwards
	for JIT we want
		expr
		br_inv_exit
		body
		expr
		br_backwards
	but again, it hurts sometimes, just like for
	*/

	if (p.GetJitFriendly())
	{
		// TODO: of course I should do some analysis on real bounds of the for loop so that we don't unroll more iters than the loop will actualy run

		Int fwdExit[UNROLL_MAX_COUNT];

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			fwdExit[i] = -1;

		if (bconst != 1)
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[0], p));
			// flip condition
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			fwdExit[0] = p.EmitForwardJump(p.ConvJump(dt, OPC_IBZ_P));
			p.PopStackType(1);
		}

		p.FlushOpt();
		Int body = p.instructions.GetSize();
		p.loops.Add(body);

		// body
		auto olc = p.GetLatentCounter();
		LETHE_RET_FALSE(nodes[1]->CodeGen(p));

		Int bodyWeight = Max<Int>(p.instructions.GetSize() - body, 1);

		// trying world's dumbest unrolling, but it might help a lot in tiny loops due to persistent caching in regs
		Int unrollCount = Min<Int>(UNROLL_MAX_COUNT*UNROLL_MIN_WEIGHT / bodyWeight, UNROLL_MAX_COUNT)-1;

		// don't unroll loops that increase latent counter
		if (p.GetLatentCounter() != olc)
			unrollCount = 0;

		for (Int i=0; i<unrollCount; i++)
		{
			if (bconst != 1)
			{
				LETHE_RET_FALSE(CodeGenBoolExpr(nodes[0], p, 1));
				// flip condition
				DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
				fwdExit[1+i] = p.EmitForwardJump(p.ConvJump(dt, OPC_IBZ_P));
				p.PopStackType();
			}

			LETHE_RET_FALSE(nodes[1]->CodeGen(p));
		}

		LETHE_ASSERT(scopeRef->type == NSCOPE_LOOP);
		scopeRef->FixupContinueHandles(p);

		if (bconst == 1)
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_BR, body));
		else
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[0], p, 1));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();
		}

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			if (fwdExit[i] >= 0)
				p.FixupForwardTarget(fwdExit[i]);

		// handle nobreak
		if (bconst != 1 && nodes.GetSize() > 2)
			LETHE_RET_FALSE(nodes[2]->CodeGenNoBreak(p));

		scopeRef->FixupBreakHandles(p);
	}
	else
	{
		Int fwd = bconst==1 ? -1 : p.EmitForwardJump(OPC_BR);
		p.FlushOpt();
		Int body = p.instructions.GetSize();
		LETHE_RET_FALSE(nodes[1]->CodeGen(p));
		LETHE_RET_FALSE(fwd < 0 || p.FixupForwardTarget(fwd));
		LETHE_ASSERT(scopeRef->type == NSCOPE_LOOP);
		scopeRef->FixupContinueHandles(p);

		if (bconst == 1)
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_BR, body));
		else
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[0], p, 1));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();

			// handle nobreak
			if (nodes.GetSize() > 2)
				LETHE_RET_FALSE(nodes[2]->CodeGenNoBreak(p));
		}

		scopeRef->FixupBreakHandles(p);
	}
	return true;
}


}
