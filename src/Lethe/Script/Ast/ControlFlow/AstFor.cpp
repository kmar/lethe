#include "AstFor.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include "../AstSymbol.h"
#include "../AstVarDecl.h"
#include <Lethe/Script/Ast/Types/AstVarDeclList.h>
#include <Lethe/Script/Ast/Types/AstTypeInt.h>
#include <Lethe/Script/Ast/Constants/AstConstInt.h>
#include <Lethe/Script/Ast/BinaryOp/AstBinaryOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstDotOp.h>
#include <Lethe/Script/Ast/BinaryOp/AstSubscriptOp.h>
#include "../AstExpr.h"
#include "../AstBlock.h"
#include "../NamedScope.h"

namespace lethe
{

// AstFor

bool AstFor::ConvertRangeBasedFor(const ErrorHandler &p)
{
	LETHE_ASSERT(nodes.GetSize() == 4);

	// validate range based for first
	if (nodes[2]->type != AST_EXPR)
		return true;

	auto *preinc = nodes[2]->nodes[0];

	if (preinc->type != AST_UOP_PREINC)
		return true;

	auto *lsym = preinc->nodes[0];

	if (lsym->type != AST_IDENT)
		return true;

	if (nodes[1]->type != AST_OP_LT)
		return true;

	if (nodes[1]->nodes[0]->type != AST_IDENT)
		return true;

	auto *sym = AstStaticCast<AstSymbol *>(lsym);

	auto iterName = sym->text;
	auto idxName = p.AddString(String::Printf("%s_index", iterName.Ansi()));

	// rewrite AST...

	auto *initNode = nodes[0];
	initNode->parent = nullptr;

	auto *loopScope = initNode->scopeRef;

	sym->text = idxName;

	// initializer (int idx=0)
	auto *vdlist = new AstVarDeclList(nodes[0]->location);
	vdlist->parent = this;
	vdlist->scopeRef = initNode->scopeRef;
	nodes[0] = vdlist;

	auto *itype = vdlist->Add(new AstTypeInt(vdlist->location));
	itype->flags |= AST_F_SKIP_CGEN;

	auto *vd = new AstVarDecl(vdlist->location);
	vd->scopeRef = initNode->scopeRef;
	vd->flags |= AST_F_REFERENCED;

	vdlist->Add(vd);

	auto *vdsym = new AstSymbol(idxName, vdlist->location);
	vdsym->scopeRef = initNode->scopeRef;

	auto &members = vdsym->scopeRef->members;

	// remove iter from members if present
	auto it = members.Find(iterName);

	if (it != members.End())
		members.Erase(it);

	members.Add(idxName, vd);
	vdsym->flags |= AST_F_RESOLVED;
	vdsym->target = vd;
	vd->Add(vdsym);

	auto *czero = new AstConstInt(vdlist->location);
	czero->num.i = 0;

	vd->Add(czero);

	// "resolve" increment expr
	sym->target = nullptr;
	sym->flags &= ~AST_F_RESOLVED;

	// rewrite condition
	auto *ocond = nodes[1];
	ocond->parent = nullptr;

	// we want condition sym < array.size

	auto *opsym = AstStaticCast<AstSymbol *>(ocond->nodes[0]);
	opsym->text = idxName;
	opsym->target = nullptr;
	opsym->flags &= ~AST_F_RESOLVED;

	auto *arr = ocond->nodes[1];
	arr->parent = nullptr;

	auto *oprhs = new AstDotOp(ocond->location);
	oprhs->Add(arr);

	oprhs->scopeRef = ocond->scopeRef;

	ocond->nodes[1] = oprhs;
	oprhs->parent = ocond;

	const String size = "size";
	auto *sizeSym = new AstSymbol(p.AddString(size), ocond->location);
	oprhs->Add(sizeSym);
	sizeSym->scopeRef = arr->scopeRef;

	// last thing we need to do: replace body with new block and define a reference
	// this will be tricky, but...

	auto *arrClone = arr->Clone();

	auto *stmt = nodes[3];

	auto *block = new AstBlock(stmt->location);

	// add new block scope
	auto *bscope = new NamedScope(NSCOPE_LOCAL);

	block->scopeRef = bscope;
	arrClone->scopeRef = bscope;

	// we still need to turn cloned array into subscript operator with index
	auto *sub  = new AstSubscriptOp(stmt->location);
	sub->Add(arrClone);

	auto subidx = opsym->Clone();
	subidx->scopeRef = bscope;
	sub->Add(subidx);

	arrClone = sub;

	if (stmt->scopeRef == loopScope)
	{
		// special handling for empty statements
		bscope->parent = loopScope;
		stmt->scopeRef = bscope;
	}
	else
	{
		bscope->parent = stmt->scopeRef->parent;
		stmt->scopeRef->parent = bscope;
	}

	// just add this to scope list so that we don't leak memory
	bscope->parent->scopes.Add(bscope);

	block->parent = stmt->parent;
	nodes[3] = block;

	stmt->parent = nullptr;

	// first we add iter expr
	AstIterator iter(initNode);

	Int found = 0;

	StackArray<AstNode *, 64> fixupNodes;

	while (auto *n = iter.Next())
	{
		if (n->scopeRef == loopScope)
			n->scopeRef = bscope;

		if (n->type == AST_VAR_DECL)
			bscope->members[iterName] = n;

		if (!found && (n->type == AST_VAR_DECL || n->type == AST_OP_ASSIGN))
			fixupNodes.Add(n);
	}

	for (Int i=fixupNodes.GetSize()-1; i>=0; i--)
	{
		auto *n = fixupNodes[i];
		delete n->nodes[1];
		n->nodes[1] = arrClone;
		arrClone->parent = n;
		found = 1;
	}

	initNode->scopeRef = bscope;

	if (initNode->type != AST_VAR_DECL_LIST)
	{
		// must create expression
		auto *expr = new AstExpr(initNode->location);
		expr->Add(initNode);
		initNode = expr;
	}

	block->Add(initNode);

	// and then statement
	block->Add(stmt);

	if (!found)
	{
		delete arrClone;
		return p.Error(this, "failed to rewrite range based for");
	}

	// remap symScopeRefs
	AstIterator ai(stmt);

	while (auto *n = ai.Next())
	{
		if (n->symScopeRef == loopScope)
			n->symScopeRef = bscope;
	}

	while (Resolve(p) == RES_MORE);

	return true;
}

bool AstFor::ResolveNode(const ErrorHandler &e)
{
	if (type == AST_FOR_RANGE && nodes[1]->type == AST_OP_LT)
	{
		auto *tnode = nodes[1]->nodes[1]->GetTypeNode();

		if (tnode)
		{
			switch (tnode->type)
			{
			case AST_TYPE_DYNAMIC_ARRAY:
			case AST_TYPE_ARRAY_REF:
			case AST_TYPE_ARRAY:
				// turn this into iteration...
				LETHE_RET_FALSE(ConvertRangeBasedFor(e));
				break;

			default:;
			}
		}
	}

	return Super::ResolveNode(e);
}

bool AstFor::CodeGen(CompiledProgram &p)
{
	// [0] = init, [1] = cond, [2] = inc, [3] = body, [4] = (opt) nobreak
	/* similar to while, transform into
		into:
		init expr
		br expr
		body
		incexpr
		expr
		br_backwards

	for JIT, we want:
		init expr
		expr
		br_inv_bailout
		body
		incexpr
		expr
		br_backwards
	this help a bit in most cases but does horrible things to bubbletest3 loop!
	*/
	Int mark = p.ExprStackMark();
	LETHE_RET_FALSE(nodes[0]->CodeGen(p));

	if (nodes[0]->type != AST_EMPTY)
		p.ExprStackCleanupTo(mark);

	Int bconst = nodes[1]->ToBoolConstant(p);

	if (bconst == 0)
		return true;

	// unfortunately this hurts a certain simple synthetic test a lot!
	if (p.GetJitFriendly())
	{
		// TODO: of course I should do some analysis on real bounds of the for loop so that we don't unroll more iters than the loop will actualy run

		Int fwdExit[UNROLL_MAX_COUNT];

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			fwdExit[i] = -1;

		if (nodes[1]->type != AST_EMPTY) {
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
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
		LETHE_RET_FALSE(nodes[3]->CodeGen(p));

		Int bodyWeight = Max<Int>(p.instructions.GetSize() - body, 1);

		// trying world's dumbest unrolling, but it might help a lot in tiny loops due to persistent caching in regs
		Int unrollCount = Min<Int>(UNROLL_MAX_COUNT*UNROLL_MIN_WEIGHT / bodyWeight, UNROLL_MAX_COUNT)-1;

		// don't unroll loops that increase latent counter
		if (p.GetLatentCounter() != olc)
			unrollCount = 0;

		for (Int i=0; i<unrollCount; i++)
		{
			// incr
			LETHE_RET_FALSE(nodes[2]->CodeGen(p));

			// expr
			if (nodes[1]->type != AST_EMPTY)
			{
				LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
				// flip condition
				DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
				fwdExit[1+i] = p.EmitForwardJump(p.ConvJump(dt, OPC_IBZ_P));
				p.PopStackType();
			}

			// body
			LETHE_RET_FALSE(nodes[3]->CodeGen(p));
		}

		LETHE_ASSERT(scopeRef->type == NSCOPE_LOOP);
		scopeRef->FixupContinueHandles(p);
		// inc expr
		LETHE_RET_FALSE(nodes[2]->CodeGen(p));

		// expr
		if (nodes[1]->type == AST_EMPTY) {
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_BR, body));
		} else {
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();
		}

		for (Int i=0; i<UNROLL_MAX_COUNT; i++)
			if (fwdExit[i] >= 0)
				p.FixupForwardTarget(fwdExit[i]);

		// handle nobreak if necessary
		if (nodes[1]->type != AST_EMPTY && nodes.GetSize() > 4)
			LETHE_RET_FALSE(nodes[4]->CodeGenNoBreak(p));

		scopeRef->FixupBreakHandles(p);
	}
	else
	{
		Int fwd = p.EmitForwardJump(OPC_BR);
		p.FlushOpt();
		Int body = p.instructions.GetSize();
		// body
		LETHE_RET_FALSE(nodes[3]->CodeGen(p));
		LETHE_ASSERT(scopeRef->type == NSCOPE_LOOP);
		scopeRef->FixupContinueHandles(p);
		// inc expr
		LETHE_RET_FALSE(nodes[2]->CodeGen(p));
		LETHE_RET_FALSE(p.FixupForwardTarget(fwd));

		// expr
		if (nodes[1]->type == AST_EMPTY)
			LETHE_RET_FALSE(p.EmitBackwardJump(OPC_BR, body));
		else
		{
			LETHE_RET_FALSE(CodeGenBoolExpr(nodes[1], p));
			DataTypeEnum dt = p.exprStack.Back().GetTypeEnum();
			LETHE_RET_FALSE(p.EmitBackwardJump(p.ConvJump(dt, OPC_IBNZ_P), body));
			p.PopStackType();

			// handle nobreak
			if (nodes.GetSize() > 4)
				LETHE_RET_FALSE(nodes[4]->CodeGenNoBreak(p));
		}

		scopeRef->FixupBreakHandles(p);
	}

	return true;
}


}
