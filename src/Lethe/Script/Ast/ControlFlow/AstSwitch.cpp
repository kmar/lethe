#include "AstSwitch.h"
#include "../NamedScope.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/Ast/Constants/AstTextConstant.h>

namespace lethe
{

// AstSwitch

bool AstSwitch::CompareConst(AstNode *n0, AstNode *n1)
{
	LETHE_RET_FALSE(n0->type == n1->type);

	switch(n0->type)
	{
	case AST_CONST_BOOL:
	case AST_CONST_CHAR:
	case AST_CONST_INT:
	case AST_CONST_NAME:
		return n0->num.i == n1->num.i;

	case AST_CONST_UINT:
		return n0->num.ui == n1->num.ui;

	case AST_CONST_LONG:
		return n0->num.l == n1->num.l;

	case AST_CONST_ULONG:
		return n0->num.ul == n1->num.ul;

	case AST_CONST_FLOAT:
		return n0->num.f == n1->num.f;

	case AST_CONST_DOUBLE:
		return n0->num.d == n1->num.d;

	case AST_CONST_STRING:
		return AstStaticCast<AstText *>(n0)->text == AstStaticCast<AstText *>(n1)->text;

	default:;
	}

	return false;
}

bool AstSwitch::CodeGen(CompiledProgram &p)
{
	AstNode *expr = nodes[IDX_EXPR];
	AstNode *body = nodes[IDX_BODY];

	QDataType dt = expr->GetTypeDesc(p);

	if (!dt.IsSwitchable())
		return p.Error(expr, "cannot switch this type");

	const bool isConst = expr->IsConstant();

	// we only care about these cases:
	// int, uint, float, name, string
	HashSet<Int> icases;
	HashSet<UInt> uicases;
	HashSet<Long> lcases;
	HashSet<ULong> ulcases;
	HashSet<Float> fcases;
	HashSet<Double> dcases;
	HashSet<String> scases;
	Int imin = Limits<Int>::Max();
	Int imax = Limits<Int>::Min();
	UInt uimin = Limits<UInt>::Max();
	UInt uimax = Limits<UInt>::Min();

	// if jumptable is impossible for int/uint => AND all for float, name, string will be if-emulated
	bool isUInt = false;

	for (auto n : body->nodes)
	{
		if (n->type == AST_CASE_DEFAULT)
			continue;

		AstNode *caseExpr = n->nodes[0];

		if (!caseExpr->IsConstant())
			return p.Error(caseExpr, "constant expression required");

		if (!caseExpr->ConvertConstTo(dt.GetTypeEnum(), p))
			return p.Error(caseExpr, "cannot convert case expresssion to switch type");

		// reload
		caseExpr = n->nodes[0];

		switch(dt.GetTypeEnum())
		{
		case DT_UINT:
			if (uicases.Find(caseExpr->num.ui) != uicases.End())
				return p.Error(caseExpr, "case value already taken");

			uicases.Insert(caseExpr->num.ui);
			uimin = Min(caseExpr->num.ui, uimin);
			uimax = Max(caseExpr->num.ui, uimax);
			isUInt = true;
			break;

		case DT_ULONG:
			if (ulcases.Find(caseExpr->num.ul) != ulcases.End())
				return p.Error(caseExpr, "case value already taken");

			ulcases.Insert(caseExpr->num.ul);
			break;

		case DT_LONG:
			if (lcases.Find(caseExpr->num.l) != lcases.End())
				return p.Error(caseExpr, "case value already taken");

			lcases.Insert(caseExpr->num.l);
			break;

		case DT_FLOAT:
			if (fcases.Find(caseExpr->num.f) != fcases.End())
				return p.Error(caseExpr, "case value already taken");

			fcases.Insert(caseExpr->num.f);
			break;

		case DT_DOUBLE:
			if (dcases.Find(caseExpr->num.d) != dcases.End())
				return p.Error(caseExpr, "case value already taken");

			dcases.Insert(caseExpr->num.d);
			break;

		case DT_NAME:
		case DT_STRING:
		{
			const auto &text = AstStaticCast<AstTextConstant *>(caseExpr)->text;

			if (scases.Find(text) != scases.End())
				return p.Error(caseExpr, "case value already taken");

			scases.Insert(text);

			if (dt.GetTypeEnum() == DT_NAME)
			{
				// optimization: names will be handled just like integers!
				Name tmp = text;
				caseExpr->num.i = tmp.GetIndex();
				icases.Insert(caseExpr->num.i);
				imin = Min(caseExpr->num.i, imin);
				imax = Max(caseExpr->num.i, imax);
				uimin = (UInt)imin;
				uimax = (UInt)imax;
				isUInt = true;
			}
		}
		break;

		default:
			if (icases.Find(caseExpr->num.i) != icases.End())
				return p.Error(caseExpr, "case value already taken");

			icases.Insert(caseExpr->num.i);
			imin = Min(caseExpr->num.i, imin);
			imax = Max(caseExpr->num.i, imax);
			uimin = (UInt)imin;
			uimax = (UInt)imax;
			isUInt = true;
		}
	}

	Int switchCount = Max(icases.GetSize(), uicases.GetSize());
	Int switchRange = Int(uimax + 1 - uimin);

	if (isConst)
	{
		AstNode *defaultNode = nullptr;

		for (auto *n : body->nodes)
		{
			if (n->type == AST_CASE_DEFAULT)
			{
				defaultNode = n;
				continue;
			}

			auto *caseExpr = n->nodes[0];

			if (!CompareConst(expr, caseExpr))
				continue;

			for (Int j = 1; j < n->nodes.GetSize(); j++)
			{
				p.SetLocation(n->nodes[j]->location);
				LETHE_RET_FALSE(n->nodes[j]->CodeGen(p));
			}

			defaultNode = nullptr;
			break;
		}

		if (defaultNode)
		{
			for (Int j = 0; j < defaultNode->nodes.GetSize(); j++)
			{
				p.SetLocation(defaultNode->nodes[j]->location);
				LETHE_RET_FALSE(defaultNode->nodes[j]->CodeGen(p));
			}
		}

		p.FlushOpt();

		scopeRef->FixupBreakHandles(p);

		return true;
	}

	LETHE_RET_FALSE(expr->CodeGen(p));

	if (p.exprStack.GetSize() != 1)
		return p.Error(expr, "switch expression must return a value");

	// check if table switch possible:
	// note: only table-switching on 32-bit integers
	if (!isUInt || switchCount <= 2 || switchCount*3 < switchRange)
	{
		p.PopStackType(1);
		// allocating virtual variable so that switch scope cleans properly!
		p.curScope->AllocVar(dt);

		/*transcribe switch:
		using if table :
			if ($switch == casen)
				jmp fwd case x
				// loop for all cases
			jmp fwd default
		*/

		const auto dte = dt.GetTypeEnum();

		Array<Int> caseFixups;

		for (auto *n : body->nodes)
		{
			if (n->type == AST_CASE_DEFAULT)
				continue;

			p.FlushOpt();

			auto *caseExpr = n->nodes[0];

			switch(dte)
			{
			case DT_FLOAT:
				p.Emit(OPC_LPUSH32F);
				p.EmitFloatConst(caseExpr->num.f);
				p.Emit(OPC_FCMPEQ);
				caseFixups.Add(p.EmitForwardJump(OPC_IBNZ_P));
				break;

			case DT_DOUBLE:
				p.Emit(OPC_LPUSH64D);
				p.EmitDoubleConst(caseExpr->num.d);
				p.Emit(OPC_DCMPEQ);
				caseFixups.Add(p.EmitForwardJump(OPC_IBNZ_P));
				break;

			case DT_LONG:
			case DT_ULONG:
				p.EmitI24(OPC_LPUSH64, 0);
				p.EmitULongConst(caseExpr->num.ul);
				p.Emit(OPC_LCMPEQ);
				caseFixups.Add(p.EmitForwardJump(OPC_IBNZ_P));
				break;

			case DT_STRING:
				p.Emit(OPC_PUSH_ICONST);
				p.Emit(OPC_BCALL + (BUILTIN_LPUSHSTR << 8));

				p.EmitUIntConst(p.cpool.Add(AstStaticCast<AstTextConstant *>(caseExpr)->text));
				p.Emit(OPC_BCALL + (BUILTIN_LPUSHSTR_CONST << 8));

				p.Emit(OPC_BCALL + (BUILTIN_SCMPEQ << 8));
				caseFixups.Add(p.EmitForwardJump(OPC_IBNZ_P));
				break;

			default:
				p.Emit(OPC_LPUSH32);
				p.EmitIntConst(caseExpr->num.i);
				caseFixups.Add(p.EmitForwardJump(OPC_IBEQ));
			}
		}

		p.FlushOpt();
		Int defaultFixup = p.EmitForwardJump(OPC_BR);

		Int i = 0;

		for (auto n : body->nodes)
		{
			p.FlushOpt();

			Int start;

			if (n->type == AST_CASE_DEFAULT)
			{
				p.FixupForwardTarget(defaultFixup);
				defaultFixup = -1;
				start = 0;
			}
			else
			{
				p.FixupForwardTarget(caseFixups[i++]);
				start = 1;
			}

			for (Int j = start; j < n->nodes.GetSize(); j++)
			{
				p.SetLocation(n->nodes[j]->location);
				LETHE_RET_FALSE(n->nodes[j]->CodeGen(p));
			}

		}

		p.FlushOpt();

		scopeRef->FixupBreakHandles(p);

		if (defaultFixup >= 0)
			p.FixupForwardTarget(defaultFixup);

		return 1;
	}

	// okay, now: allocate switchRange * int global data (target pc), build table, generate code and fix offsets
	// code will look as follows:

	// eval_expr
	// push_iconst -imin
	// iadd
	// gloadadr tbl

	// allocate table:
	// 2 + range

	// and here comes superinstruction:
	// switch table looks like this:
	// default_pc [can jump over!]
	// ... table ...
	// (note: flushes_opt)

	// I'll make it part of code (disassembly gets garbage but who cares)

	p.PopStackType();

	if (uimin)
	{
		p.EmitIntConst(-*reinterpret_cast<Int *>(&uimin));
		p.Emit(OPC_IADD);
	}

	p.EmitU24(OPC_SWITCH, switchRange);

	UInt base = p.instructions.GetSize();

	Int tableOfs = p.instructions.GetSize();
	p.instructions.Resize(p.instructions.GetSize() + 1 + switchRange, 0);
	// assume table filled with zeroes

	p.switchRange.Add(tableOfs);
	p.switchRange.Add(tableOfs + 1 + switchRange);

	p.FlushOpt();

	for (auto n : body->nodes)
	{
		p.FlushOpt();

		UInt idx;
		Int statIdx;

		if (n->type == AST_CASE_DEFAULT)
		{
			idx = 0;
			statIdx = 0;
		}
		else
		{
			idx = n->nodes[0]->num.i - uimin + 1;
			statIdx = 1;
		}

		p.instructions[tableOfs + idx] = p.instructions.GetSize();

		for (Int j=statIdx; j<n->nodes.GetSize(); j++)
		{
			p.SetLocation(n->nodes[j]->location);
			LETHE_RET_FALSE(n->nodes[j]->CodeGen(p));
		}
	}

	p.FlushOpt();

	scopeRef->FixupBreakHandles(p);

	UInt *table = reinterpret_cast<UInt *>(p.instructions.GetData() + tableOfs);

	if (!table[0])
		table[0] = p.instructions.GetSize();

	// fill unused table values with default target
	for (int i=1; i<1+switchRange; i++)
		if (!table[i])
			table[i] = table[0];

	// turn into relative offsets
	for (int i = 0; i < 1 + switchRange; i++)
		table[i] -= base;

	return true;
}


}
