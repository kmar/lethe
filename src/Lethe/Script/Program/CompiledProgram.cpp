#include <Lethe/Core/String/String.h>
#include <Lethe/Core/Sys/Path.h>
#include "CompiledProgram.h"
#include <Lethe/Script/Ast/AstNode.h>
#include <Lethe/Script/Ast/CodeGenTables.h>
#include <Lethe/Script/Ast/NamedScope.h>
#include <Lethe/Script/Ast/ControlFlow/AstLabel.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Builtin.h>
#include <Lethe/Script/Compiler/Warnings.h>
#include "../ScriptEngine.h"

namespace lethe
{

// ErrorHandler

void ErrorHandler::CheckShadowing(const NamedScope *cscope, const String &nname, AstNode *nnode, const Delegate<void(const String &msg, const TokenLocation &loc, Int warnid)> &onWarn)
{
	if (cscope->IsLocal() && (nnode->type == AST_VAR_DECL || nnode->type == AST_ARG))
	{
		const auto *tmp = cscope->parent;

		while (tmp)
		{
			auto *sym = tmp->FindSymbol(nname);

			if (sym && (sym->type == AST_VAR_DECL || sym->type == AST_ARG))
			{
				Path pth = sym->location.file;
				onWarn(
					String::Printf("declaration of %s shadows a previous variable at line %d in %s", nname.Ansi(), sym->location.line, pth.GetFilename().Ansi()),
					nnode->location, WARN_SHADOW);
				break;
			}

			tmp = tmp->parent;
		}
	}
}

bool ErrorHandler::Error(const AstNode *n, const String &msg) const
{
	onError(msg, n ? n->location : TokenLocation());
	return 0;
}

void ErrorHandler::Warning(const AstNode *n, const String &msg, Int warnid) const
{
	onWarning(msg, n->location, warnid);
}

const String &ErrorHandler::AddString(const String &str) const
{
	MutexLock _(stringTableMutex);

	auto ci = stringTable.Find(str);

	if (ci != stringTable.End())
		return *ci;

	stringTable.Add(str);
	return str;
}

const String &ErrorHandler::AddString(const StringRef &sr) const
{
	MutexLock _(stringTableMutex);

	auto ci = stringTable.Find(sr);

	if (ci != stringTable.End())
		return *ci;

	stringTable.Add(sr);
	return stringTable.GetKey(stringTable.GetSize()-1);
}

void ErrorHandler::AddLateDeleteNode(AstNode *node) const
{
	SpinMutexLock lock(lateDeleteMutex);
	lateDeleteNodes.Add(node);
}

void ErrorHandler::FlushLateDeleteNodes()
{
	SpinMutexLock lock(lateDeleteMutex);

	for (auto *it : lateDeleteNodes)
		delete it;

	lateDeleteNodes.Clear();
}

// CompiledProgram

CompiledProgram::CompiledProgram(bool njitFriendly)
	: exprStackOfs(0)
	, initializerDelta(0)
	, lastForwardJump(-1)
	, curScope(nullptr)
	, curScopeIndex(-1)
	, scopeIndex(0)
	, globalConstIndex(-1)
	, globalDestIndex(-1)
	, jitRef(nullptr)
	, strongDtor(-1)
	, strongVDtor(-1)
	, weakDtor(-1)
	, weakVDtor(-1)
	, engineRef(nullptr)
	, stackFrameBase(0)
	, emitOptBase(0)
	, jumpOptBase(0)
	, unsafe(0)
	, jitFriendly(njitFriendly)
	, profiling(false)
	, inlineCall(0)
{
	// first two instructions are reserved for halt and null function
	instructions.Add(OPC_HALT);
	FlushOpt();
	instructions.Add(OPC_RET);
	FlushOpt();
	globalConstIndex = instructions.GetSize();

	// initialize elemtypes
	DataType *dt;

	dt = elemTypes + DT_BOOL;
	dt->type = DT_BOOL;
	dt->align = dt->size = sizeof(bool);
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	for (Int i=DT_BYTE; i<=DT_SBYTE; i++)
	{
		dt = elemTypes + i;
		dt->type = (DataTypeEnum)i;
		dt->align = dt->size = 1;
		dt->typeIndex = AddType(new DataType(*dt))->typeIndex;
	}

	for (Int i=DT_SHORT; i<=DT_USHORT; i++)
	{
		dt = elemTypes + i;
		dt->type = (DataTypeEnum)i;
		dt->align = dt->size = 2;
		dt->typeIndex = AddType(new DataType(*dt))->typeIndex;
	}

	dt = elemTypes + DT_CHAR;
	auto charType = dt;
	dt->type = DT_CHAR;
	dt->align = dt->size = sizeof(Int);
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	for (Int i=DT_INT; i<=DT_UINT; i++)
	{
		dt = elemTypes + i;
		dt->type = (DataTypeEnum)i;
		dt->align = dt->size = 4;
		dt->typeIndex = AddType(new DataType(*dt))->typeIndex;
	}

	for (Int i=DT_LONG; i<=DT_ULONG; i++)
	{
		dt = elemTypes + i;
		dt->type = (DataTypeEnum)i;
		dt->align = dt->size = 8;
		dt->typeIndex = AddType(new DataType(*dt))->typeIndex;
	}

	dt = elemTypes + DT_FLOAT;
	dt->align = dt->size = 4;
	dt->type = DT_FLOAT;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_DOUBLE;
	dt->align = AlignOf<Double>::align;
	dt->size = 8;
	dt->type = DT_DOUBLE;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_ENUM;
	dt->type = DT_ENUM;
	dt->align = dt->size = 4;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_NAME;
	dt->align = dt->size = 4;
	dt->type = DT_NAME;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_STRING;
	dt->align = (Int)AlignOf<String>::align;
	dt->size = sizeof(String);
	dt->type = DT_STRING;
	dt->elemType = QDataType::MakeConstType(*charType);
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_NULL;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->type = DT_NULL;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_FUNC_PTR;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->type = DT_FUNC_PTR;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_DELEGATE;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->size *= 2;
	dt->type = DT_DELEGATE;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_RAW_PTR;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->type = DT_RAW_PTR;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_STRONG_PTR;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->type = DT_STRONG_PTR;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	dt = elemTypes + DT_WEAK_PTR;
	dt->align = dt->size = sizeof(UIntPtr);
	dt->type = DT_WEAK_PTR;
	dt->typeIndex = AddType(new DataType(*dt))->typeIndex;

	// init internal func names (to avoid allocations)
	ifuncNames[IFUNC_INIT] = "__init";
	ifuncNames[IFUNC_COPY] = "__copy";
}

const String &CompiledProgram::GetInternalFuncName(InternalFunc ifunc) const
{
	LETHE_ASSERT(ifunc >= 0 && ifunc < IFUNC_MAX);
	return ifuncNames[ifunc];
}

void CompiledProgram::EnterScope(NamedScope *scopeRef)
{
	LETHE_ASSERT(scopeRef);

	for (Int i=0; i<scopeRef->deferred.GetSize(); i++)
		scopeRef->deferred[i]->flags &= ~AST_F_DEFER;

	scopeRef->ResetDeferredTop();

	// FIXME: I wonder WHY this condition was even here => this broke for loop unrolling!
	//if (GetInline())
	{
		scopeRef->varOfs = 0;
		scopeRef->varSize = 0;
		scopeRef->maxVarAlign = 0;
		scopeRef->maxVarSize = 0;
		scopeRef->localVars.Clear();
	}

	NamedScope *ps = scopeRef->parent;
	Int vofsBase = scopeRef->varOfs;

	if (ps && scopeRef->IsLocal() && ps->IsLocal())
	{
		vofsBase = scopeRef->varOfs = ps->varOfs;
		scopeRef->varSize = ps->varOfs;
		scopeRef->maxVarAlign = ps->maxVarAlign;
		scopeRef->maxVarSize = ps->maxVarSize;
	}

	scopeStack.Add(ScopeDesc());
	ScopeDesc &sd = scopeStack.Back();
	sd.oldScope = curScope;
	sd.varOfsBase = vofsBase;
	sd.scopeIndex = curScopeIndex;
	curScopeIndex = scopeIndex++;
	curScope = scopeRef;
}

// TODO/FIXME: refactor to merge functionality of breakscope, gotoscope and possibly returnscope
bool CompiledProgram::GotoScope(const AstLabel *label)
{
	const auto *target = label->scopeRef;
	NamedScope *cscope = curScope;

	for (Int i=scopeStack.GetSize()-1; i>=0; i--)
	{
		if (cscope == target)
		{
			if (label->pc >= 0)
			{
				// jumping backwards....
				LETHE_RET_FALSE(EmitDefer(cscope, label->deferredSize));

				if (cscope->varOfs != label->varOfsBase)
				{
					// emit_cleanup!
					cscope->GenDestructors(*this, label->localVarSize);
					Emit(OPC_POP + (UInt((cscope->varOfs - label->varOfsBase) / Stack::WORD_SIZE) << 8));
				}
			}
			return true;
		}

		ScopeDesc sd = scopeStack[i];

		LETHE_RET_FALSE(EmitDefer(cscope));

		Int vofsBase = sd.varOfsBase;

		if (cscope->varOfs != vofsBase)
		{
			// emit_cleanup!
			cscope->GenDestructors(*this);
			Emit(OPC_POP + (UInt((cscope->varOfs - vofsBase) / Stack::WORD_SIZE) << 8));
		}

		cscope = sd.oldScope;
	}

	LETHE_ASSERT(false && "broken goto");
	return false;
}

NamedScope *CompiledProgram::BreakScope(bool isContinue)
{
	NamedScope *cscope = curScope;

	for (Int i=scopeStack.GetSize()-1; i>=0; i--)
	{
		if (cscope->type == NSCOPE_LOOP || (!isContinue && cscope->type == NSCOPE_SWITCH))
			return cscope;

		ScopeDesc sd = scopeStack[i];

		LETHE_RET_FALSE(EmitDefer(cscope));

		Int vofsBase = sd.varOfsBase;

		if (cscope->varOfs != vofsBase)
		{
			// emit_cleanup!
			cscope->GenDestructors(*this);
			Emit(OPC_POP + (UInt((cscope->varOfs - vofsBase)/Stack::WORD_SIZE) << 8));
		}

		cscope = sd.oldScope;
	}

	LETHE_ASSERT(false && "broken break/continue");
	return nullptr;
}

bool CompiledProgram::StateBreakScope()
{
	if (stateBreakLock++)
		return false;

	NamedScope *cscope = curScope;

	for (Int i=scopeStack.GetSize()-1; i>=0; i--)
	{
		ScopeDesc sd = scopeStack[i];

		Int vofsBase = sd.varOfsBase;

		const auto olddtop = cscope->deferredTop;
		const auto dstart = Min<Int>(olddtop, cscope->deferred.GetSize());

		for (Int j = dstart - 1; j >= 0; j--)
		{
			cscope->deferredTop = j;
			auto dn = cscope->deferred[j];

			if (!(dn->flags & AST_F_DEFER) || (dn->qualifiers & AST_Q_NOSTATEBREAK))
				continue;

			if (!dn->CodeGen(*this))
			{
				cscope->deferredTop = olddtop;
				--stateBreakLock;
				return false;
			}
		}

		cscope->deferredTop = olddtop;

		if (cscope->varOfs != vofsBase)
		{
			// emit_cleanup!
			cscope->GenDestructors(*this);
			Emit(OPC_POP + (UInt((cscope->varOfs - vofsBase)/Stack::WORD_SIZE) << 8));
		}

		if (cscope->type == NSCOPE_FUNCTION)
		{
			LETHE_ASSERT(!inlineCall);
			Emit(OPC_RET);
			--stateBreakLock;
			return true;
		}

		cscope = sd.oldScope;
	}

	LETHE_ASSERT(false && "broken statebreak return");
	return false;
}

bool CompiledProgram::EmitDefer(NamedScope *cscope, Int nstart)
{
	bool res = true;
	const auto olddtop = cscope->deferredTop;
	const auto dstart = Min<Int>(olddtop, cscope->deferred.GetSize());

	for (Int j = dstart-1; j >= nstart; j--)
	{
		cscope->deferredTop = j;
		auto dn = cscope->deferred[j];

		if (!(dn->flags & AST_F_DEFER))
			continue;

		if (!dn->CodeGen(*this))
		{
			res = false;
			break;
		}
	}

	cscope->deferredTop = olddtop;

	return res;
}

bool CompiledProgram::ReturnScope(bool retOpt)
{
	NamedScope *cscope = curScope;

	for (Int i=scopeStack.GetSize()-1; i>=0; i--)
	{
		ScopeDesc sd = scopeStack[i];

		Int vofsBase = sd.varOfsBase;

		if (cscope->type == NSCOPE_FUNCTION && cscope->deferred.IsEmpty())
		{
			// check if we can do fast return now without branching
			bool dest = !retOpt || cscope->HasDestructors();

			if (!dest && cscope->varOfs != vofsBase)
			{
				// emit_cleanup!
				Emit(OPC_POP + (UInt((cscope->varOfs - vofsBase) / Stack::WORD_SIZE) << 8));
			}

			return !dest;
		}

		if (cscope->type == NSCOPE_FUNCTION && !cscope->deferred.IsEmpty() && (cscope->deferred.Back()->flags & AST_F_DEFER))
		{
			// avoid generating extra code!
			return 0;
		}

		LETHE_RET_FALSE(EmitDefer(cscope));

		if (cscope->varOfs != vofsBase)
		{
			// emit_cleanup!
			cscope->GenDestructors(*this);
			Emit(OPC_POP + (UInt((cscope->varOfs - vofsBase)/Stack::WORD_SIZE) << 8));
		}

		if (cscope->type == NSCOPE_FUNCTION)
		{
			LETHE_ASSERT(!inlineCall && !cscope->deferred.IsEmpty());
			return 1;
		}

		cscope = sd.oldScope;
	}

	LETHE_ASSERT(false && "broken return");
	return false;
}

bool CompiledProgram::LeaveScope(bool virt)
{
	ScopeDesc sd = scopeStack.Back();

	if (!virt)
	{
		LETHE_RET_FALSE(EmitDefer(curScope));
	}

	if (curScope->IsLocal())
	{
		for (const auto &lv : curScope->localVars)
			EndLocalVarLifeTime(lv.offset);
	}

	if (!virt)
	{
		Int vofsBase = sd.varOfsBase;

		if (curScope->varOfs != vofsBase)
		{
			// emit_cleanup!
			curScope->GenDestructors(*this);
			Emit(OPC_POP + (UInt((curScope->varOfs - vofsBase)/Stack::WORD_SIZE) << 8));
		}

		if (curScope->type == NSCOPE_FUNCTION)
		{
			if (!inlineCall)
			{
				// don't forget to return
				Emit(OPC_RET);
			}
		}
	}

	if (sd.oldScope && sd.oldScope->IsLocal())
	{
		LETHE_ASSERT(curScope && curScope->IsLocal());
		sd.oldScope->maxVarSize = Max(curScope->maxVarSize, sd.oldScope->maxVarSize);
	}

	curScope = sd.oldScope;
	curScopeIndex = sd.scopeIndex;
	scopeStack.Pop();
	return 1;
}

void CompiledProgram::DynArrayVarFix(QDataType qdt)
{
	if (qdt.GetTypeEnum() != DT_DYNAMIC_ARRAY)
		return;

	auto etop = exprStack.Back().GetTypeEnum();

	if (etop != DT_ARRAY_REF)
		return;

	EmitIntConst(qdt.GetType().elemType.GetType().typeIndex);

	Int fidx = cpool.FindNativeFunc("__da_fromaref");
	LETHE_ASSERT(fidx >= 0);
	EmitI24(OPC_BCALL, fidx);
}

Int CompiledProgram::GetPc() const
{
	return instructions.GetSize();
}

bool CompiledProgram::FixupForwardTarget(Int fwHandle)
{
	Int target = fixupTargets[fwHandle];
	Int cur = instructions.GetSize();
	Int delta = cur - (target+1);

	auto opc = instructions[target] & 255u;

	if (!delta && target > jumpOptBase && target == instructions.GetSize()-1 && opc == OPC_BR)
	{
		// we can't do anything fancy here, i.e. while loop remembers body by index so we can't pop here
		instructions.Back() = OPC_NOP;
		// new: don't flush opt for br 0
		flist.Free(fwHandle);
		return 1;
	}
	else
	{
		LETHE_RET_FALSE(CanEncodeI24(delta));
		Int &ins = instructions[target];
		ins &= 255;
		ins += (UInt)delta << 8;
	}

	// optimize conditional jump + br => flipped cond
	// this is very dangerous and won't work here until forward jump target is resolved...

	Int num = instructions.GetSize();

	bool flippedJump = false;

	if (num > 1 && emitOptBase <= num - 2 && ((instructions.Back() & 255) == OPC_BR))
	{
		auto &pins = instructions[instructions.GetSize()-2];

		if (IsCondJumpNoFloat(pins))
		{
			// assuming 2's complement
			auto brdelta = Int(instructions.Back()) >> 8;
			auto pinsdelta = Int(pins) >> 8;
			// must be jump over next, but skip if it's "infinite loop" or overflow
			if (pinsdelta == 1 && brdelta != -1 && CanEncodeI24(brdelta + pinsdelta))
			{
				pins = FlipJump(pins);
				pins += instructions.Back() & ~255;

				// look for pending forward jumps at num-1
				for (auto &it : fixupTargets)
					if (it == num-1)
						--it;

				// we could pop here, but better safe than sorry
				instructions.Back() = OPC_NOP;

				flippedJump = true;
			}
		}
	}

	flist.Free(fwHandle);

	// this helps JIT keep some regs cached when falling through
	// (preparing for loop unrolling)
	FlushOpt(flippedJump);

	jumpOptBase = emitOptBase;
	return 1;
}

bool CompiledProgram::IsCall(Int ins)
{
	switch(ins)
	{
	case OPC_CALL:
	case OPC_BCALL:
	case OPC_BCALL_TRAP:
	case OPC_BMCALL:
	case OPC_NCALL:
	case OPC_NMCALL:
	case OPC_NVCALL:
	case OPC_VCALL:
	case OPC_FCALL:
	case OPC_FCALL_DG:
		return true;
	}

	return false;
}

bool CompiledProgram::IsConvToBool(Int ins)
{
	auto opc = ins & 255;

	if (opc >= OPC_ICMPEQ && opc <= OPC_DCMPGE)
		return true;

	if (opc >= OPC_PCMPEQ && opc <= OPC_PCMPNZ)
		return true;

	return opc == OPC_CONV_PTOB || ins == (OPC_BCALL + 256*BUILTIN_CONV_STOBOOL);
}

void CompiledProgram::SetupVtbl(Int ofs)
{
	engineRef->SetupVtbl(cpool.data.GetData() + ofs);
}

bool CompiledProgram::IsCondJump(Int ins)
{
	auto opc = ins & 255;
	return opc > OPC_BR && opc <= OPC_PCMPNZ;
}

bool CompiledProgram::IsCondJumpNoFloat(Int ins)
{
	LETHE_RET_FALSE(IsCondJump(ins));

	if (nanAware)
	{
		auto opc = ins & 255;

		switch(opc)
		{
		case OPC_FBZ_P:
		case OPC_FBNZ_P:
		case OPC_DBZ_P:
		case OPC_DBNZ_P:
		case OPC_FBEQ:
		case OPC_FBNE:
		case OPC_FBLT:
		case OPC_FBLE:
		case OPC_FBGT:
		case OPC_FBGE:
		case OPC_DBEQ:
		case OPC_DBNE:
		case OPC_DBLT:
		case OPC_DBLE:
		case OPC_DBGT:
		case OPC_DBGE:
			return false;
		}
	}

	return true;
}

Int CompiledProgram::FlipJump(Int ins)
{
	LETHE_ASSERT(IsCondJump(ins));
	auto opc = ins & 255;

	static const Int flipJumpTable[] =
	{
		OPC_BR,
		OPC_IBNZ_P,
		OPC_IBZ_P,
		OPC_FBNZ_P,
		OPC_FBZ_P,
		OPC_DBNZ_P,
		OPC_DBZ_P,
		OPC_IBNZ,
		OPC_IBZ,

		OPC_IBNE,
		OPC_IBEQ,
		OPC_IBGE,
		OPC_IBGT,
		OPC_IBLE,
		OPC_IBLT,
		OPC_UIBGE,
		OPC_UIBGT,
		OPC_UIBLE,
		OPC_UIBLT,

		OPC_FBNE,
		OPC_FBEQ,
		OPC_FBGE,
		OPC_FBGT,
		OPC_FBLE,
		OPC_FBLT,

		OPC_DBNE,
		OPC_DBEQ,
		OPC_DBGE,
		OPC_DBGT,
		OPC_DBLE,
		OPC_DBLT,

		OPC_PCMPNE,
		OPC_PCMPEQ,
		OPC_PCMPNZ,
		OPC_PCMPZ
	};

	ins &= ~255;
	ins |= flipJumpTable[opc - OPC_BR];
	return ins;
}

const DataType &CompiledProgram::Coerce(const DataType &dt0, const DataType &dt1) const
{
	if (&dt0 == &dt1)
		return dt0;

	return elemTypes[DataType::ComposeTypeEnum(dt0.type, dt1.type)];
}

void CompiledProgram::PushStackType(const QDataType &qdt)
{
	Int align = qdt.GetAlign();
	Int size = qdt.GetSize();

	// double size for method ptrs (used for delegates)
	if (qdt.IsMethodPtr())
		size *= 2;

	LETHE_ASSERT(size || qdt.GetTypeEnum() == DT_STRUCT);

	if (size)
	{
		align = Max<Int>(align, Stack::WORD_SIZE);
		size += Stack::WORD_SIZE-1;
		size &= ~(Stack::WORD_SIZE-1);
	}

	// in 32-bit mode we only align stack up to 4 bytes
	align = Min<Int>(align, Stack::WORD_SIZE);

	exprStack.Add(qdt);
	exprStackOffset.Add(exprStackOfs);

	if (align)
		exprStackOfs += (align - (exprStackOfs % align)) % align;

	exprStackOfs += size;

	LETHE_ASSERT(!(exprStackOfs % Stack::WORD_SIZE));

	// can be null for global initializers; we don't check global init for stack overflow (FIXME: not a great idea?)
	if (curScope)
		curScope->maxVarSize = Max<Int>(curScope->maxVarSize, curScope->varSize + exprStackOfs);
}

void CompiledProgram::PopStackType(bool nocleanup)
{
	if (!nocleanup)
	{
		const QDataType &qdt = exprStack.Back();

		// TODO: more/better
		if (!qdt.IsReference())
		{
			if (qdt.GetTypeEnum() == DT_STRING)
			{
				EmitDelStr(0);
			}
			else if (qdt.HasDtor())
			{
				Emit(OPC_LPUSHADR);
				EmitBackwardJump(OPC_CALL, qdt.GetType().funDtor);
				Emit(OPC_POP + (1 << 8));
			}
		}
	}

	exprStack.Pop();
	exprStackOfs = exprStackOffset.Back();
	exprStackOffset.Pop();
}

Int CompiledProgram::ExprStackMark() const
{
	return exprStack.GetSize();
}

void CompiledProgram::ExprStackCleanupTo(Int mark)
{
	while (exprStack.GetSize() > mark)
	{
		Int opc = OPC_POP | ((UInt)((exprStackOfs - exprStackOffset.Back()) / Stack::WORD_SIZE) << 8);
		PopStackType();
		Emit(opc);
	}
}

void CompiledProgram::Finalize()
{
	validReturnTargets.Reset();

	// add global dtors if any
	globalDestIndex = -1;

	if (!globalDestInstr.IsEmpty())
	{
		FlushOpt();
		EmitFunc("$$global_dtor", 0);
		globalDestIndex = instructions.GetSize();

		Int start = instructions.GetSize();
		Int size = globalDestInstr.GetSize();

		for (Int i=0; i<size; i++)
			Emit(globalDestInstr[i]);

		// global dest fixups
		for (Int i=0; i<globalDestFixups.GetSize(); i++)
		{
			const GlobalDestFixup &fix = globalDestFixups[i];
			Int pc = size - fix.revPc + start - 1;
			instructions[pc] += UInt(fix.target - pc - 1) << 8;
		}

		Emit(OPC_RET);
	}

	// this helps with program logic:
	barriers.Add(instructions.GetSize());
	loops.Add(instructions.GetSize());
}

void CompiledProgram::FlushOpt(bool nobarrier)
{
	if (!nobarrier)
	{
		Int lastBarrier = barriers.IsEmpty() ? -1 : barriers.Back();

		LETHE_ASSERT(instructions.GetSize() >= lastBarrier);

		if (instructions.GetSize() != lastBarrier)
			barriers.Add(instructions.GetSize());
	}

	LETHE_ASSERT(emitOptBase <= instructions.GetSize());
	emitOptBase = instructions.GetSize();
}

void CompiledProgram::AddReturnHandle(Int handle)
{
	returnHandles.Add(handle);
}

bool CompiledProgram::FixupHandles(Array<Int> &handles)
{
	auto cmp = [this](int x, int y) -> bool
	{
		return fixupTargets[x] < fixupTargets[y];
	};
	handles.Sort(cmp);

	for (Int i=handles.GetSize()-1; i>=0; i--)
		LETHE_RET_FALSE(FixupForwardTarget(handles[i]));

	handles.Clear();
	return 1;
}

bool CompiledProgram::FixupReturnHandles()
{
	return FixupHandles(returnHandles);
}

const DataType *CompiledProgram::FindClass(Name n) const
{
	const DataType *res = nullptr;
	Int idx = classTypeHash.FindIndex(n);

	if (idx >= 0)
		res = classTypeHash.GetValue(idx);

	return res;
}

void CompiledProgram::AddClassType(Name n, const DataType *dt)
{
	classTypeHash[n] = dt;
}

const DataType *CompiledProgram::AddType(DataType *newType)
{
	bool isEnumItem = newType->type == DT_INT && newType->baseType.GetTypeEnum() == DT_ENUM;

	if (newType->IsArray() || newType->IsPointer() || newType->type == DT_FUNC_PTR || newType->type == DT_DELEGATE || isEnumItem)
	{
		// special handling here...
		auto ci = typeHash.Find(newType->name);

		if (ci != typeHash.End())
		{
			delete newType;
			return ci->value;
		}

		typeHash[newType->name] = newType;
	}

	auto res = newType;
	res->typeIndex = types.Add(newType);
	return res;
}

bool CompiledProgram::EmitGlobalCopy(AstNode *n, const DataType &src, Int offset)
{
	// TODO: reference
	if (src.IsPointer() && src.type != DT_RAW_PTR)
	{
		EmitI24(OPC_GLOADADR, offset);
		EmitBackwardJump(OPC_CALL, src.funAssign);
		EmitI24(OPC_POP, 1);
		Emit(OPC_LPUSHADR);
		EmitBackwardJump(OPC_CALL, src.funDtor);
		EmitI24(OPC_POP, 2);
		return true;
	}

	if (src.type == DT_ARRAY_REF)
	{
		Emit(OPC_LPUSHADR);
		EmitI24(OPC_GLOADADR, offset);
		Emit(OPC_PCOPY + ((UInt)src.size << 8));
		Emit(OPC_POP + (2 << 8));
		return true;
	}

	if (src.IsStruct())
	{
		Int ssize = (src.size + Stack::WORD_SIZE-1) / Stack::WORD_SIZE;
		Emit(OPC_LPUSHADR);
		EmitI24(OPC_GLOADADR, offset);

		if (src.funAssign >= 0)
		{
			EmitBackwardJump(OPC_CALL, src.funAssign);
			EmitI24(OPC_POP, 2+ssize);
		}
		else
		{
			EmitU24(OPC_PCOPY, src.size);
			EmitU24(OPC_POP, ssize);
		}

		return true;
	}

	if (src.type == DT_NONE || src.type > DT_STRING)
		return Error(n, "unsupported type for global copy");

	if (src.type == DT_STRING)
	{
		EmitIntConst(offset);
		Emit(OPC_BCALL + (BUILTIN_GSTRSTORE << 8));
		return true;
	}


	bool large = Abs(offset) >= (1 << 23);

	if (large)
	{
		EmitI24(OPC_GLOADADR, offset);
		Emit(opcodeRefStore[0][src.type]);
		return true;
	}

	EmitU24(opcodeGlobalStore[0][src.type], offset);
	return true;
}

void CompiledProgram::EmitLocalDtor(const DataType &src, Int offset)
{
	Int delta = offset;
	delta /= Stack::WORD_SIZE;

	// TODO: more/better!
	if (src.type == DT_STRING)
	{
		EmitDelStr(delta);
		return;
	}

	if (src.funDtor < 0)
		return;

	Emit(OPC_LPUSHADR + ((UInt)delta << 8));
	EmitBackwardJump(OPC_CALL, src.funDtor);
	Emit(OPC_POP + (1 << 8));
}

void CompiledProgram::EmitGlobalDestCall(Int pc)
{
	AddGlobalDestFixup(pc);
	globalDestInstr.AddFront(OPC_CALL);
}

void CompiledProgram::AddGlobalDestFixup(Int pc)
{
	GlobalDestFixup fix;
	fix.target = pc;
	fix.revPc = globalDestInstr.GetSize();

	globalDestFixups.Add(fix);
}

bool CompiledProgram::EmitGlobalDtor(AstNode *n, const DataType &src, Int offset)
{
	(void)n;

	bool large = Abs(offset) >= (1 << 23);

	Int smallOfs = (Int)((UInt)offset & ((1u << 16) - 1));
	Int hiOfs = (offset - smallOfs) >> 16;

	// note: in reverse order
	if (src.type == DT_STRING)
	{
		globalDestInstr.AddFront(OPC_BCALL + (BUILTIN_GDELSTR << 8));

		if (large)
		{
			globalDestInstr.AddFront(OPC_IADD);
			globalDestInstr.AddFront(OPC_ISHL_ICONST + (16 << 8));
			globalDestInstr.AddFront(OPC_PUSH_ICONST + ((UInt)hiOfs << 8));
			globalDestInstr.AddFront(OPC_PUSH_ICONST + ((UInt)smallOfs << 8));
		}
		else
			globalDestInstr.AddFront(OPC_PUSH_ICONST + ((UInt)offset << 8));

		return true;
	}

	LETHE_RET_FALSE(src.funDtor >= 0);
	globalDestInstr.AddFront(OPC_POP + (1 << 8));
	EmitGlobalDestCall(src.funDtor);

	if (large)
	{
		globalDestInstr.AddFront(OPC_AADDH_ICONST + ((UInt)hiOfs << 8));
		offset = smallOfs;
	}

	globalDestInstr.AddFront(OPC_GLOADADR + ((UInt)offset << 8));
	return true;
}

bool CompiledProgram::IsValidCodePtr(const Int *iptr) const
{
	return iptr >= instructions.GetData() && iptr < instructions.GetData() + instructions.GetSize();
}

void CompiledProgram::ProfEnter(const String &fname)
{
	if (!profiling || inlineCall)
		return;

	profFuncName.Add(fname);

	EmitIntConst(cpool.Add(fname));
	EmitI24(OPC_BCALL, BUILTIN_PROF_ENTER);
}

void CompiledProgram::ProfExit()
{
	if (!profiling || inlineCall)
		return;

	EmitIntConst(cpool.Add(profFuncName.Back()));
	EmitI24(OPC_BCALL, BUILTIN_PROF_EXIT);

	profFuncName.Pop();
}

void CompiledProgram::SetLocation(const TokenLocation &loc)
{
	CodeToLine cl;
	cl.pc = instructions.GetSize();
	cl.line = loc.line;
	cl.file = loc.file;

	if (!codeToLine.IsEmpty())
	{
		auto &last = codeToLine.Back();

		if (cl.pc == last.pc)
		{
			last = cl;
			return;
		}

		if (last.line == cl.line && last.file == cl.file)
			return;
	}

	codeToLine.Add(cl);
}

void CompiledProgram::AddVtbl(Int offset, Int count)
{
	vtbls.Add(offset);
	vtbls.Add(count);
}

bool CompiledProgram::VtblOk() const
{
	for (Int i = 0; i < vtbls.GetSize(); i += 2)
	{
		Int ofs = vtbls[i];
		Int count = vtbls[i + 1];
		const IntPtr *ptr = reinterpret_cast<const IntPtr *>(cpool.data.GetData() + ofs);

		for (Int j = 0; j < count; j++)
		{
			if (ptr[j] < 0)
				return 0;
		}
	}

	return 1;
}

// convert vtbls to pointers
void CompiledProgram::FixupVtbl()
{
	for (Int i=0; i<vtbls.GetSize(); i += 2)
	{
		Int ofs = vtbls[i];
		Int count = vtbls[i+1];
		IntPtr *ptr = reinterpret_cast<IntPtr *>(cpool.data.GetData() + ofs);

		for (Int j=0; j<count; j++)
		{
			LETHE_ASSERT(ptr[j] >= 0);
			ptr[j] = ptr[j] * sizeof(Instruction) + reinterpret_cast<IntPtr>(instructions.GetData());
		}
	}
}

void CompiledProgram::FixupVtblJit(const Array<Int> &pcToCode, const Byte *code)
{
	for (Int i = 0; i < vtbls.GetSize(); i += 2)
	{
		Int ofs = vtbls[i];
		Int count = vtbls[i + 1];
		IntPtr *ptr = reinterpret_cast<IntPtr *>(cpool.data.GetData() + ofs);

		for (Int j = 0; j < count; j++)
		{
			Int pc = Int((ptr[j] - reinterpret_cast<IntPtr>(instructions.GetData())) / sizeof(Instruction));
			ptr[j] = reinterpret_cast<IntPtr>(code + pcToCode[pc]);
		}
	}
}

void CompiledProgram::StartStackFrame()
{
	stackFrameBase = curScope->varOfs;
}

void CompiledProgram::EndStackFrame()
{
	stackFrameBase = 0;
}

void CompiledProgram::StartLocalVarLifeTime(const QDataType &qdt, Int offset, const String &name)
{
	if (jitFriendly)
		return;

	// FIXME? not ideal; but we can disable inlining in full debug mode
	if (GetInline())
		return;

	offset = stackFrameBase - offset;

	LocalVarDebugKey lk;
	lk.index = curScopeIndex;
	lk.offset = offset;

	DebugInfoVar v;
	v.name = name;
	v.type = qdt;
	v.offset = offset;
	v.startPC = instructions.GetSize();
	v.endPC = -1;
	v.isLocal = 1;

	localVars[lk] = v;
}

void CompiledProgram::EndLocalVarLifeTime(Int offset)
{
	if (jitFriendly)
		return;

	offset = stackFrameBase - offset;

	LocalVarDebugKey lk;
	lk.index = curScopeIndex;
	lk.offset = offset;
	auto it = localVars.Find(lk);

	if (it == localVars.End())
		return;

	it->value.endPC = instructions.GetSize();
}

void CompiledProgram::MarkReturnValue(Int delta)
{
	returnValues.Add(instructions.GetSize() + delta);
}

void CompiledProgram::InitValidReturnPath(Int from, Int to)
{
	validReturnTargetStart = from;
	validReturnTargets.Resize(to - from);
	validReturnTargets.Clear();
}

void CompiledProgram::MarkValidReturnPathTarget(Int pc)
{
	pc -= validReturnTargetStart;

	if (pc >= 0 && pc < validReturnTargets.GetSize())
		validReturnTargets.SetBit(pc);
}

bool CompiledProgram::IsValidReturnPathTarget(Int pc) const
{
	pc -= validReturnTargetStart;

	return pc >= 0 && pc < validReturnTargets.GetSize() && validReturnTargets.TestBit(pc);
}

UInt CompiledProgram::ConvJump(DataTypeEnum dte, UInt ins)
{
	switch(ins)
	{
	case OPC_IBZ_P:
		if (dte == DT_FLOAT)
			return OPC_FBZ_P;

		if (dte == DT_DOUBLE)
			return OPC_DBZ_P;

		return ins;

	case OPC_IBNZ_P:
		if (dte == DT_FLOAT)
			return OPC_FBNZ_P;

		if (dte == DT_DOUBLE)
			return OPC_DBNZ_P;

		return ins;

	default:;
	}

	return OPC_HALT;
}

void CompiledProgram::ClearLatentCounter()
{
	compiledProgramLatentCounter = 0;
	latentStackLevel = curScope->varOfs;
}

Int CompiledProgram::IncLatentCounter()
{
	if (!CheckLatentStack())
		return -1;

	return compiledProgramLatentCounter++;
}

bool CompiledProgram::CheckLatentStack() const
{
	return curScope->varOfs == latentStackLevel;
}

bool CompiledProgram::SetBreakpoint(Int pc, bool enable)
{
	if (savedOpcodes.IsEmpty() || pc < 0 || pc >= instructions.GetSize())
		return false;

	if (!enable)
		instructions[pc] = (instructions[pc] & ~255) | savedOpcodes[pc];
	else
		instructions[pc] = (instructions[pc] & ~255) | OPC_BREAK;

	return true;
}

bool CompiledProgram::IsSwitchTable(Int pc) const
{
	auto sw = LowerBound(switchRange.Begin(), switchRange.End(), pc);
	Int swidx = sw == switchRange.End() ? -1 : Int(IntPtr(sw - switchRange.Begin()));

	bool isSwitchTable = false;

	if (swidx >= 0)
	{
		if (swidx & 1)
			isSwitchTable = pc >= switchRange[swidx-1] && pc < switchRange[swidx];
		else
			isSwitchTable = pc >= switchRange[swidx] && pc < switchRange[swidx+1];
	}

	return isSwitchTable;
}

}
