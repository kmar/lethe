#include "ScriptContext.h"
#include "ScriptEngine.h"
#include "Utils/NativeHelpers.h"
#include <Lethe/Core/Thread/Thread.h>

#include <Lethe/Core/String/StringBuilder.h>

#include "Program/CompiledProgram.h"
#include "Program/ConstPool.h"

namespace lethe
{

// ScriptContext

ScriptContext::ScriptContext(Int stkSize)
	: vmJit(nullptr)
	, mode(ENGINE_JIT)
	, mutex(Mutex::Recursive())
	, stateDelegateRef(nullptr)
{
	if (stkSize <= 0)
		stkSize = 65536;

	vmStack = new Stack(stkSize);
	vmStack->context = this;
	vm = new Vm;
	vm->SetStack(vmStack);
	vm->onRuntimeError.Set(this, &Self::OnRuntimeError);
	vm->onDebugBreak.Set(this, &Self::OnDebugBreak);

	// default debug break, assuming context is bound to a thread,
	// waits for break mode flag to be reset from the outside, ready to process next instruction
	onDebugBreak = [](ScriptContext &ctx, ExecResult &)->bool
	{
		auto tmp = ctx.GetCallStack();

		auto &eng = ctx.GetEngine();

		eng.onInfo(String::Printf("debug break in context 0x" LETHE_FORMAT_UINTPTR_HEX, (UIntPtr)(void *)&ctx));

		for (auto &&it : tmp)
			eng.onInfo(it.Ansi());

		while (ctx.InBreakMode())
			Thread::Sleep(10);

		// continue execution
		return false;
	};
}

ScriptContext::~ScriptContext()
{
	auto frq = Timer::GetHiCounterFreq();

	Array<ProfileInfo> sorted;

	Double totalSec = 0;

	for (auto &&i : profiling)
	{
		totalSec += (Double)i.value.rootTicks / frq;
		sorted.Add(i.value);
		sorted.Back().nameIdx = i.key;
	}

	sorted.Sort();

	if (!profiling.IsEmpty())
	{
		engine->onInfo("");
		engine->onInfo(String::Printf("context 0x" LETHE_FORMAT_UINTPTR_HEX " (percentage of context totals, 100%% = %0.6lf msec)", (UIntPtr)(void *)this, totalSec*1000.0));
		engine->onInfo("-------------------------------------------------------------------------------");
	}

	for (auto &&i : sorted)
	{
		auto excSec = (Double)i.exclusiveTicks / frq;
		auto incSec = (Double)i.totalTicks / frq;
		engine->onInfo(String::Printf("%s #" LETHE_FORMAT_ULONG " (%0.6lf / %0.6lf msec %0.2lf%% / %0.2lf%%)",
			vmStack->GetConstantPool().sPool[i.nameIdx].Ansi(), i.calls, excSec * 1000.0, incSec * 1000.0, excSec*100.0/totalSec, incSec*100.0/totalSec));

		// extract called from sequence
		for (auto &&cf : i.calledFrom)
			engine->onInfo(String::Printf("    called from: %s", vmStack->GetConstantPool().sPool[profiling.GetKey(cf).key].Ansi()));
	}

	MutexLock lock(engine->contextMutex);
	engine->contexts.Erase(engine->contexts.FindIndex(this));
}

SharedPtr<ScriptContext> ScriptContext::Clone() const
{
	return engine->CreateContext();
}

String ScriptContext::GetName() const
{
	return String::Printf("%s (0x" LETHE_FORMAT_UINTPTR_HEX ")", debugName.Ansi(), (UIntPtr)(const void *)this);
}

void ScriptContext::SetName(const String &ctxname)
{
	debugName = ctxname;
}

Int ScriptContext::GetCallStackDepth() const
{
	auto iptr = vmStack->insPtr;
	return vm->GetCallStackDepth(iptr);
}

Array<String> ScriptContext::GetCallStack(Int maxVarTextLen) const
{
	auto iptr = vmStack->insPtr;
	return vm->GetCallStack(iptr, maxVarTextLen);
}

Array<String> ScriptContext::GetGlobals(Int maxVarTextLen) const
{
	Array<String> res;
	auto *prog = GetStack().prog;

	if (!prog)
		return res;

	for (auto &&it : prog->cpool.globalVars)
	{
		StringBuilder sb;
		sb += it.key;
		sb += " = ";

		auto *gdata = prog->cpool.data.GetData();

		const auto &ginfo = it.value;

		const void *ptr = gdata + ginfo.offset;

		if (ginfo.qualifiers & AST_Q_REFERENCE)
		{
			sb += " = ref ";
			ptr = *(const void **)ptr;
		}

		ginfo.type->GetVariableText(sb, ptr, maxVarTextLen);

		res.Add(sb.Get());
	}

	return res;
}

ExecResult ScriptContext::RunConstructors()
{
	vm->prog->cpool.ClearGlobalBakedStrings();

	if (vmJit)
	{
		Int idx = vm->prog->globalConstIndex;

		if (idx < 0)
			return EXEC_OK;

		return vmJit->ExecScriptFunc(*vm, idx);
	}

	return vm->CallGlobalConstructors();
}

ExecResult ScriptContext::RunDestructors()
{
	if (vmJit)
	{
		Int idx = vm->prog->globalDestIndex;

		if (idx < 0)
			return EXEC_OK;

		return vmJit->ExecScriptFunc(*vm, idx);
	}

	return vm->CallGlobalDestructors();
}

ExecResult ScriptContext::Call(const StringRef &fname)
{
	if (vmJit)
	{
		Int fpc = vm->FindFunc(fname);

		if (fpc < 0)
			return EXEC_FUNC_NOT_FOUND;

		return vmJit->ExecScriptFunc(*vm, fpc);
	}

	return vm->CallFunc(fname);
}

ExecResult ScriptContext::ResumeCall()
{
	if (vmJit)
		return EXEC_NO_JIT;

	if (!vm->stack->GetInsPtr())
		return EXEC_NULL_PTR;

	return vm->ExecutePtr(vm->stack->GetInsPtr());
}

ExecResult ScriptContext::CallPointer(const void *funptr)
{
	if (!funptr)
		return EXEC_NULL_PTR;

	if (vmJit)
		return vmJit->ExecScriptFuncPtr(*vm, funptr);

	// make sure we push retadr to instruction #0 (halt)
	vmStack->PushPtr(vm->prog->instructions.GetData());

	if (vmStack->GetNesting())
	{
		auto saved = vm->GetExecFlags();
		vm->SetExecFlags(saved | Vm::EXEC_NO_BREAK);
		auto res = vm->ExecutePtr(funptr);
		vm->SetExecFlags(saved);
		return res;
	}

	return vm->ExecutePtr(funptr);
}

ExecResult ScriptContext::CallOffset(Int pcOffset)
{
	if (pcOffset < 0)
		return EXEC_INVALID_PC;

	if (vmJit)
		return vmJit->ExecScriptFunc(*vm, pcOffset);

	// make sure we push retadr to instruction #0 (halt)
	vmStack->PushPtr(vm->prog->instructions.GetData());

	if (vmStack->GetNesting())
	{
		auto saved = vm->GetExecFlags();
		vm->SetExecFlags(saved | Vm::EXEC_NO_BREAK);
		auto res = vm->Execute(pcOffset);
		vm->SetExecFlags(saved);
		return res;
	}

	return vm->Execute(pcOffset);
}

BaseObject *ScriptContext::NewObject(Name name)
{
	auto &stk = *vmStack.Get();
	stk.PushInt(name.GetIndex());
	Builtin::Opcode_New_Dynamic(stk);
	auto fptr = stk.GetPtr(0);

	if (fptr)
	{
		// do fcall
		stk.Pop(1);
		CallPointer(fptr);
	}

	stk.Pop(1);

	// note: strong ref is 0
	auto *res = static_cast<BaseObject *>(stk.GetPtr(0));
	stk.Pop(1);

	return res;
}

void ScriptContext::ConstructObject(Name name, void *inst)
{
	auto &stk = *vmStack.Get();
	Builtin::Opcode_New_Dynamic(stk, name, inst);
	auto fptr = stk.GetPtr(0);

	if (fptr)
	{
		// do fcall
		stk.Pop(1);
		CallPointer(fptr);
	}

	stk.Pop(2);
}

void ScriptContext::DestructObject(Name name, void *inst)
{
	auto &stk = *vmStack.Get();

	auto *dt = stk.GetProgram().FindClass(name);

	if (dt && dt->funDtor >= 0)
	{
		stk.PushPtr(inst);
		CallOffset(dt->funDtor);
		stk.Pop(1);
	}
}

ExecResult ScriptContext::CallMethod(const StringRef &fname, const void *instancePtr)
{
	if (!instancePtr)
		return EXEC_NULL_INSTANCE;

	auto *obj = static_cast<const BaseObject *>(instancePtr);

	auto *clstype = obj->GetScriptClassType();

	// FIXME: better error here...
	if (!clstype)
		return EXEC_METHOD_NOT_FOUND;

	Int idx = clstype->FindMethodOffset(fname);

	if (!idx)
		return EXEC_METHOD_NOT_FOUND;

	auto *oldThis = vmStack->GetThis();
	vmStack->SetThis(instancePtr);

	ExecResult res = EXEC_OK;

	auto &stk = *vmStack;

	// emulate pushed this
	stk.PushPtr(nullptr);

	if (idx >= 0)
		res = CallOffset(idx);
	else
	{
		// extract vtbl ptr AND execute
		const void *fptr = obj->scriptVtbl[-idx];
		res = CallPointer(fptr);
	}

	stk.Pop(1);

	vmStack->SetThis(oldThis);

	return res;
}

ExecResult ScriptContext::CallDelegate(const ScriptDelegate &dg)
{
	if (dg.IsEmpty())
		return EXEC_NULL_INSTANCE;

	const auto *ptr = dg.funcPtr;
	auto vidx = (UIntPtr)ptr;

	// if LSBit is 1, it's not an actual pointer but rather a vtbl index
	if (vidx & 1)
	{
		vidx &= 0xffffffffu;
		vidx >>= 2;
		const void * const *vtbl = *static_cast<const void * const * const *>(&static_cast<const BaseObject *>(dg.instancePtr)->scriptVtbl);
		ptr = reinterpret_cast<const void * const *>(vtbl)[vidx];
	}
	else
	{
		vidx &= ~(UIntPtr)3;
		ptr = (void *)vidx;
	}

	return CallMethodByPointer(ptr, dg.instancePtr);
}

ExecResult ScriptContext::CallMethodByIndex(Int idx, const void *instancePtr)
{
	if (!instancePtr)
		return EXEC_NULL_INSTANCE;

	if (!idx)
		return EXEC_METHOD_NOT_FOUND;

	auto *obj = static_cast<const BaseObject *>(instancePtr);

	auto *oldThis = vmStack->GetThis();
	vmStack->SetThis(instancePtr);

	ExecResult res = EXEC_OK;

	auto &stk = *vmStack;

	// emulate pushed this
	stk.PushPtr(nullptr);

	if (idx >= 0)
		res = CallOffset(idx);
	else
	{
		// extract vtbl ptr AND execute
		const void *fptr = obj->scriptVtbl[-idx];
		res = CallPointer(fptr);
	}

	stk.Pop(1);

	vmStack->SetThis(oldThis);

	return res;
}

ExecResult ScriptContext::CallMethodByPointer(const void *ptr, const void *instancePtr)
{
	if (!instancePtr || !ptr)
		return EXEC_NULL_INSTANCE;

	auto *oldThis = vmStack->GetThis();
	vmStack->SetThis(instancePtr);

	ExecResult res = EXEC_OK;

	auto &stk = *vmStack;

	// emulate pushed this
	stk.PushPtr(nullptr);

	res = CallPointer(ptr);

	stk.Pop(1);

	vmStack->SetThis(oldThis);

	return res;
}

void ScriptContext::OnRuntimeError(const char *msg)
{
	onRuntimeError(msg);
}

bool ScriptContext::OnDebugBreak(ScriptContext &ctx, ExecResult &res)
{
	return onDebugBreak(ctx, res);
}

void ScriptContext::ProfEnter(Int fnameIdx)
{
	auto idx = profiling.FindIndex(fnameIdx);

	if (idx < 0)
	{
		profiling[fnameIdx] = ProfileInfo();
		idx = profiling.GetSize()-1;
	}

	auto &pi = profiling.GetValue(idx);
	++pi.calls;
	++pi.depth;

	profStack.Add(ProfileStack{Timer::GetHiCounter(), profParent});
	profParent = idx;
}

void ScriptContext::ProfExit(Int fnameIdx)
{
	auto &pi = profiling[fnameIdx];
	const auto &ps = profStack.Back();

	auto sticks = ps.startTicks;
	sticks = Timer::GetHiCounter() - sticks;

	if (!--pi.depth)
		pi.totalTicks += sticks;

	pi.exclusiveTicks += sticks - pi.childTicks;
	pi.childTicks = 0;

	profParent = ps.parentIndex;

	if (profParent >= 0)
	{
		pi.calledFrom.Add(profParent);
		profiling.GetValue(profParent).childTicks += sticks;
	}
	else
		pi.rootTicks += sticks;

	profStack.Pop();
}

Int ScriptContext::ArrayInterface(const DataType &elemType, ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam)
{
	Lock();
	auto res = NativeHelpers::ArrayInterface(*this, elemType, cmd, aptr, pparam, iparam);
	Unlock();
	return res;
}

bool ScriptContext::GetCurrentLocation(TokenLocation &nloc) const
{
	nloc.line = nloc.column = 0;
	nloc.file.Clear();

	auto *iptr = GetStack().GetInsPtr();
	auto *base = GetStack().GetProgram().instructions.GetData();
	auto pc = (Int)IntPtr(iptr - base);

	const auto &ctol = GetStack().GetProgram().codeToLine;
	CompiledProgram::CodeToLine lookup;
	lookup.pc = pc;

	auto it = LowerBound(ctol.Begin(), ctol.End(), lookup);

	LETHE_RET_FALSE(it != ctol.End());

	nloc.file = it->file;
	nloc.line = it->line;

	if (it != ctol.Begin())
	{
		auto tmp = it;
		--tmp;

		if (it->pc > pc)
		{
			nloc.file = tmp->file;
			nloc.line = tmp->line;
		}
	}

	return true;
}

String ScriptContext::GetFunctionSignature(const StringRef &funcName) const
{
	return GetEngine().GetFunctionSignature(funcName);
}

void ScriptContext::SetStateDelegateRef(ScriptDelegate *nref)
{
	stateDelegateRef = nref;
}

}
