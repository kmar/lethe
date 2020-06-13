#include "ScriptEngine.h"
#include "Vm/JitX86/VmJitX86.h"
#include "TypeInfo/BaseObject.h"
#include "Utils/NativeHelpers.h"
#include "Vm/Builtin.h"

#include <Lethe/Core/String/StringRef.h>
#include <Lethe/Core/Memory/Heap.h>
#include <Lethe/Core/Io/VfsFile.h>
#include <Lethe/Core/Io/MemoryStream.h>
#include <Lethe/Core/Time/Timer.h>

namespace lethe
{

// NativeClassProxy

NativeClassProxy::NativeClassProxy(ScriptEngine &eng)
	: engine(eng)
{
}

NativeClassProxy &NativeClassProxy::Member(const String &name, size_t offset)
{
	if (success)
	{
		auto &pool = engine.program->cpool;
		auto &cls = pool.nClass[handle];
		cls.members[name] = (Int)offset;
	}

	return *this;
}

// ScriptEngine

bool ScriptEngine::floatLitIsDouble = false;

void ScriptEngine::SetFloatLiteralIsDouble(bool nfloatLitIsDouble)
{
	floatLitIsDouble = nfloatLitIsDouble;
}

ScriptEngine::ScriptEngine(EngineMode emode)
	: mode(emode)
{
	// check if JIT available
	// FIXME: better!
#if LETHE_CPU_X86
	if (emode == ENGINE_JIT)
		vmJit = new VmJitX86;
#else
	// no JIT
	mode = ENGINE_RELEASE;
#endif

	MemSet(&compileStats, 0, sizeof(compileStats));

	compiler = new Compiler;
	compiler->SetFloatLiteralIsDouble(floatLitIsDouble);
	compiler->onError.Set(this, &Self::OnError);
	compiler->onWarning.Set(this, &Self::OnWarning);
	compiler->onCompile.Set(this, &Self::OnCompile);
	compiler->onResolve.Set(this, &Self::OnResolve);

	program = new CompiledProgram(mode == ENGINE_JIT);
	program->SetUnsafe(mode == ENGINE_JIT || mode == ENGINE_RELEASE);
	program->onError.Set(this, &Self::OnError);
	program->onWarning.Set(this, &Self::OnWarning);
	program->engineRef = this;

	NativeHelpers::Init(*program);

	const char * const boolStr[2] = {"false", "true"};

	internalProg.Format(
		"constexpr bool DEBUG = %s;\nconstexpr bool OS_WINDOWS=%s;\n",
		boolStr[!program->GetUnsafe()],
#if LETHE_OS_WINDOWS
		"true"
#else
		"false"
#endif
	);

	// define some useful type aliases

	internalProg +=
		R"#(
typedef const bool const_bool;
typedef const byte const_byte;
typedef const sbyte const_sbyte;
typedef const short const_short;
typedef const ushort const_ushort;
typedef const char const_char;
typedef const int const_int;
typedef const uint const_uint;
typedef const long const_long;
typedef const ulong const_ulong;
typedef const float const_float;
typedef const double const_double;
typedef const name const_name;
typedef const string const_string;

// string_view elements always immutable
typedef const_byte[] string_view;

// returns -1 on error
native char decode_utf8(string_view &sw);

// serialization helpers
native static float float_from_binary(uint binary_value);
native static uint float_to_binary(float value);

native static double double_from_binary(ulong binary_value);
native static ulong double_to_binary(double value);

// base Object
__intrinsic class object
{
	// is derived from n?
	native __intrinsic final bool is(name n) const;
	// get class name
	native final name class_name() const;
	// get non-state class name
	native final name nonstate_class_name() const;
	// this handles class inheritance, so base state class gets mapped to actual state class in derived class
	native final name fix_state_name(name state_class_name) const;
	// state helper
	native static name class_name_from_delegate(void delegate() dg);

	// set vtable
	native bool vtable(name className);
}

namespace __int
{
	inline noinit int abs(int x) {return x >= 0 ? x : -x;}
	inline noinit int min(int x, int y) {return x <= y ? x : y;}
	inline noinit int max(int x, int y) {return x >= y ? x : y;}
	inline noinit int clamp(int value, int minv, int maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
	inline noinit int sign(int value) {return (value > 0)-(value < 0);}
}

namespace __uint
{
	// bit intrinsics
	native __intrinsic noinit int bsf(uint v);
	native __intrinsic noinit int bsr(uint v);
	native __intrinsic noinit int popcnt(uint v);
	native __intrinsic noinit uint bswap(uint v);

	inline noinit uint min(uint x, uint y) {return x <= y ? x : y;}
	inline noinit uint max(uint x, uint y) {return x >= y ? x : y;}
	inline noinit uint clamp(uint value, uint minv, uint maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
}

namespace __long
{
	inline noinit long abs(long x) {return x >= 0 ? x : -x;}
	inline noinit long min(long x, long y) {return x <= y ? x : y;}
	inline noinit long max(long x, long y) {return x >= y ? x : y;}
	inline noinit long clamp(long value, long minv, long maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
	inline noinit int sign(long value) {return (value > 0)-(value < 0);}
}

namespace __ulong
{
	// bit intrinsics
	native __intrinsic noinit int bsf(ulong v);
	native __intrinsic noinit int bsr(ulong v);
	native __intrinsic noinit int popcnt(ulong v);
	native __intrinsic noinit ulong bswap(ulong v);

	inline noinit ulong min(ulong x, ulong y) {return x <= y ? x : y;}
	inline noinit ulong max(ulong x, ulong y) {return x >= y ? x : y;}
	inline noinit ulong clamp(ulong value, ulong minv, ulong maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
}

namespace __float
{
	// math
	native __intrinsic noinit float sqrt(float v);

	inline noinit float abs(float x) {return x >= 0 ? x : -x;}
	inline noinit float min(float x, float y) {return x <= y ? x : y;}
	inline noinit float max(float x, float y) {return x >= y ? x : y;}
	inline noinit float clamp(float value, float minv, float maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
	inline noinit float saturate(float value) {return value < cast float 0.0 ? cast float 0.0 : value > cast float 1.0 ? cast float 1.0 : value;}
	inline noinit float lerp(float a, float b, float t) {return a*(1.0-t) + b*t;}
	inline noinit int sign(float value) {return (value > cast float 0.0)-(value < cast float 0.0);}

	native noinit float floor(float value);
	native noinit float ceil(float value);

	native static uint hash(float value);
}

namespace __double
{
	// math
	native __intrinsic noinit double sqrt(double v);

	inline noinit double abs(double x) {return x >= 0 ? x : -x;}
	inline noinit double min(double x, double y) {return x <= y ? x : y;}
	inline noinit double max(double x, double y) {return x >= y ? x : y;}
	inline noinit double clamp(double value, double minv, double maxv) {return value < minv ? minv : value > maxv ? maxv : value;}
	inline noinit double saturate(double value) {return value < cast double 0.0 ? cast double 0.0 : value > cast double 1.0 ? cast double 1.0 : value;}
	inline noinit double lerp(double a, double b, double t) {return a*(cast double 1.0-t) + b*t;}
	inline noinit int sign(double value) {return (value > cast double 0.0)-(value < cast double 0.0);}

	native noinit double floor(double value);
	native noinit double ceil(double value);

	native static uint hash(double value);
}

namespace __name
{
	native static uint hash(name value);
}

namespace __string
{
	native static uint hash(string value);
}

// set entry base: (emulating red-black tree using array<>)
__intrinsic __set_entry struct set_entry
{
	enum __node_color
	{
		BLACK = 0,
		RED = 1
	}
	private int parent;
	private int left;
	private int right;
	private __node_color color;
}

// intrinsic support for string and dynamic arrays
native int __strlen();
native int __str_trim();
// returns new length
native int __str_insert(int pos, string what);
native int __str_find(string what, int pos=0);
native bool __str_starts_with(string what);
native bool __str_ends_with(string what);
// return true if replaced
native bool __str_replace(string what, string rwith);
// erase (returns new length)
native int __str_erase(int pos, int count=-1);
native string_view __str_slice(int from, int to=-1);
// split based on charset; keepEmpty: true to keep empty parts
native array<string> __str_split(string chset, bool keepEmpty=false);
// inplace-conversion to uppercase/lowercase
native void __str_toupper();
native void __str_tolower();

// dynamic arrays (some are shared with array refs):
native void __da_resize(int nsize);
native void __da_reserve(int nsize);
native void __da_clear();
native void __da_reset();
native void __da_shrink();
native void __da_pop();
native void __da_erase(int index);
native void __da_erase_unordered(int index);
native void __da_reverse();
native void __da_sort();

// special (assignment)
native void __da_assign(int aref);

// push/insert/...
native void __da_push(int elem);
native void __da_push_unique(int elem);
native void __da_insert(int elem, int before);
native int __da_find(int elem);
native int __da_lower_bound(int elem);
native int __da_upper_bound(int elem);
native int __da_find_sorted(int elem);
native int __da_insert_sorted(int elem);
native int __da_insert_sorted_unique(int elem);
// priority_queue:
native void __da_push_heap(int elem);
native int __da_pop_heap();
native int __da_slice(int from, int to = -1);
// predecessor/successor index in map
native int __da_pred(int idx);
native int __da_succ(int idx);
native int __da_begin();
)#"

#if LETHE_32BIT
		R"#(
typedef uint pointer;
typedef const pointer const_pointer;
typedef int intptr;
typedef const intptr cont_intptr;
typedef uint uintptr;
typedef const uintptr cont_uintptr;
)#";
#else
		R"#(
typedef ulong pointer;
typedef const pointer const_pointer;
typedef long intptr;
typedef const intptr cont_intptr;
typedef ulong uintptr;
typedef const uintptr cont_uintptr;
)#";
#endif

	CompileBuffer(internalProg.Ansi(), "*internal");

	stockCtx = CreateContext(2048);
}

ScriptEngine::~ScriptEngine()
{
	stockCtx = nullptr;
	LETHE_ASSERT(contexts.IsEmpty());
}

bool ScriptEngine::EnableRuntimeChecks(bool enable)
{
	if (mode == ENGINE_DEBUG || mode == ENGINE_DEBUG_NOBREAK)
		return false;

	program->SetUnsafe(!enable);
	return true;
}

void ScriptEngine::EnableInlineExpansion(bool enable)
{
	if (program)
		program->inlineExpansionAllowed = enable;
}

String ScriptEngine::GetInternalProgram() const
{
	return internalProg;
}

const AstNode *ScriptEngine::GetFindDefinitionRoot() const
{
	if (!findDefRoot.IsEmpty())
		return findDefRoot.Get();

	return compiler ? compiler->GetRootNode() : nullptr;
}

Array<SharedPtr<ScriptContext>> ScriptEngine::GetContexts() const
{
	Array<SharedPtr<ScriptContext>> res;
	MutexLock lock(contextMutex);

	res.Resize(contexts.GetSize());

	for (Int i=0; i<contexts.GetSize(); i++)
		res[i] = contexts[i];

	return res;
}

Int ScriptEngine::GetLiveScriptObjectCount() const
{
	return program ? program->cpool.liveScriptObjects : 0;
}

Int ScriptEngine::GetBreakpointPC(const String &filename, Int nline) const
{
	if (mode < ENGINE_DEBUG)
		return -1;

	for (auto &&it : program->codeToLine)
	{
		if (it.line == nline && it.file == filename)
			return it.pc;
	}

	return -1;
}

Array<Int> ScriptEngine::GetBreakpointPCList(const String &filename, Int nline) const
{
	Array<Int> res;

	if (mode < ENGINE_DEBUG)
		return res;

	for (auto &&it : program->codeToLine)
	{
		if (it.line == nline && it.file == filename)
			res.AddUnique(it.pc);
	}

	return res;
}

Int ScriptEngine::SetBreakpoint(const String &nfilename, Int npc, bool enabled)
{
	if (mode < ENGINE_DEBUG)
		return -1;

	MutexLock lock(breakpointMutex);

	return SetBreakpointInternal(nfilename, npc, enabled);
}

Int ScriptEngine::SetBreakpointInternal(const String &nfilename, Int npc, bool enabled)
{
	auto brkHandle = freeBreakpoints.Alloc();
	breakpoints.Resize(brkHandle+1);

	ScriptBreakpoint &sbrk = breakpoints[brkHandle];
	sbrk.filename = nfilename;
	sbrk.pc = npc;
	sbrk.enabled = enabled;

	program->SetBreakpoint(npc, enabled);

	return brkHandle;
}

bool ScriptEngine::ChangeBreakpoint(Int handle, const String &filename, Int npc)
{
	if (mode < ENGINE_DEBUG)
		return false;

	MutexLock lock(breakpointMutex);

	auto &brk = breakpoints[handle];

	if (!brk.IsValid())
		return false;

	program->SetBreakpoint(brk.pc, false);

	brk.filename = filename;
	brk.pc = npc;

	program->SetBreakpoint(brk.pc, brk.enabled);

	return true;
}

Int ScriptEngine::ToggleBreakpoint(const String &nfilename, Int npc, bool enabled)
{
	if (mode < ENGINE_DEBUG || npc < 0)
		return -1;

	MutexLock lock(breakpointMutex);

	// okay, first search for breakpoint at specific pos

	for (Int i=0; i<breakpoints.GetSize(); i++)
	{
		auto &it = breakpoints[i];

		if (it.pc == npc)
		{
			// found so delete and disable!
			program->SetBreakpoint(npc, false);
			it.enabled = false;
			it.pc = -1;
			freeBreakpoints.Free(i);
			return -1;
		}
	}

	// set new if not found

	return SetBreakpointInternal(nfilename, npc, enabled);
}

bool ScriptEngine::EnableBreakpoint(Int handle, bool nenable)
{
	MutexLock lock(breakpointMutex);

	auto &brk = breakpoints[handle];

	if (!brk.IsValid())
		return false;

	if ((bool)brk.enabled != nenable)
	{
		brk.enabled = nenable;
		program->SetBreakpoint(brk.pc, nenable);
	}

	return true;
}

void ScriptEngine::DeleteBreakpoint(Int handle)
{
	MutexLock lock(breakpointMutex);

	auto &brk = breakpoints[handle];

	if (brk.IsValid())
		program->SetBreakpoint(brk.pc, false);

	// invalidate
	brk.pc = -1;
	brk.enabled = false;

	freeBreakpoints.Free(handle);
}

void ScriptEngine::DeleteAllBreakpoints()
{
	MutexLock lock(breakpointMutex);

	for (auto &&it : breakpoints)
	{
		if (!it.IsValid())
			continue;

		program->SetBreakpoint(it.pc, false);
	}

	breakpoints.Clear();
	freeBreakpoints.Clear();
}

Array<ScriptBreakpoint> ScriptEngine::GetBreakpoints() const
{
	MutexLock lock(breakpointMutex);

	Array<ScriptBreakpoint> res;

	for (auto &&it : breakpoints)
	{
		if (it.IsValid())
			res.Add(it);
	}

	return res;
}

void *ScriptEngine::FindGlobalVariable(const String &n) const
{
	if (program)
	{
		auto it = program->cpool.globalVars.Find(n);

		if (it != program->cpool.globalVars.End())
		{
			auto offset = it->value;

			return program->cpool.data.GetData() + offset;
		}
	}
	return nullptr;
}

Int ScriptEngine::GetByteCodeSize() const
{
	return program->instructions.GetSize();
}

String ScriptEngine::DisassembleByteCode(Int pc)
{
	// FIXME: better, should move disassemble out of the Vm!
	return GetStockContext().GetVm().Disassemble(pc, program->instructions[pc]);
}

void *ScriptEngine::GetClassVtable(Name n) const
{
	auto *dt = FindClass(n);
	LETHE_RET_FALSE(dt);

	return program->cpool.data.GetData() + dt->vtblOffset;
}

ScriptContext &ScriptEngine::GetStockContext()
{
	return *stockCtx;
}

void ScriptEngine::SetupVtbl(void *clsPtr)
{
	auto pcls = static_cast<void **>(clsPtr);
	pcls[-2] = this;

	auto lambda = [](const void *inst)
	{
		auto *obj = static_cast<const BaseObject *>(inst);

		if (obj)
		{
			auto eng = static_cast<ScriptEngine *>(obj->scriptVtbl[-3]);
			auto &ctx = eng->GetStockContext();

			ctx.Lock();
			auto &stk = ctx.GetStack();
			stk.PushPtr(inst);
			ctx.CallPointer(obj->scriptVtbl[0]);
			stk.Pop(1);
			ctx.Unlock();
		}
	};

	void (*plambda)(const void *) = lambda;
	pcls[-1] = reinterpret_cast<void *>(plambda);
}

void ScriptEngine::EnableProfiling(bool enable)
{
	program->SetProfiling(enable);
}

void ScriptEngine::OnError(const String &msg, const TokenLocation &loc)
{
	onError(msg, loc);
}

void ScriptEngine::OnWarning(const String &msg, const TokenLocation &loc, Int warnid)
{
	onWarning(msg, loc, warnid);
}

void ScriptEngine::OnCompile(const String &filename)
{
	onCompile(filename);
}

void ScriptEngine::OnResolve(Int steps)
{
	onResolve(steps);
}

bool ScriptEngine::CompileBuffer(const char *buf, const String &filename)
{
	MemoryStream ms(buf);
	return CompileStream(ms, filename);
}

bool ScriptEngine::CompileFile(const String &filename)
{
	VfsFile f(filename);
	return CompileStream(f, filename);
}

bool ScriptEngine::CompileStream(Stream &stream, const String &filename)
{
	LETHE_RET_FALSE(compiler);
	PerfWatch pw;
	pw.Start();
	auto oldIo = compileStats.ioTime;
	auto res = compiler->AddCompiledProgram(compiler->CompileBuffered(stream, filename, &compileStats.ioTime));
	compileStats.compileTime += Double(pw.Stop()) / 1000000.0;
	compileStats.compileTime = Max<Double>(0, compileStats.compileTime - (compileStats.ioTime - oldIo));
	return res;
}

bool ScriptEngine::Link(int linkFlags)
{
	LETHE_RET_FALSE(compiler);
	PerfWatch pw;
	pw.Start();

	findDefRoot.Clear();

	LETHE_RET_FALSE(compiler->Resolve());

	if (linkFlags & LINK_CLONE_AST_FIND_DEFINITION)
	{
		findDefRoot = compiler->GetRootNode()->Clone();

		AstConstIterator it0(compiler->GetRootNode());
		AstIterator it1(findDefRoot);

		HashMap<const AstNode *, AstNode *> nodeRemap;

		for (;;)
		{
			const auto *src = it0.Next();

			if (!src)
				break;

			auto *dst = it1.Next();
			nodeRemap[src] = dst;
		}

		it1 = AstIterator(findDefRoot);

		for (;;)
		{
			auto *dst = it1.Next();

			if (!dst)
				break;

			if (dst->target)
				dst->target = nodeRemap[dst->target];
		}
	}

	compileStats.resolveTime += Double(pw.Stop()) / 1000000.0;

	if (!(linkFlags & LINK_SKIP_CODEGEN))
	{
		pw.Start();
		LETHE_RET_FALSE(compiler->CodeGen(*program));
		compileStats.codeGenTime += Double(pw.Stop()) / 1000000.0;

		if (vmJit)
		{
			pw.Start();
			LETHE_RET_FALSE(vmJit->CodeGen(*program));
			compileStats.jitTime += Double(pw.Stop()) / 1000000.0;
		}

		if (mode >= ENGINE_DEBUG)
		{
			program->savedOpcodes.Resize(program->instructions.GetSize());

			for (Int i=0; i<program->savedOpcodes.GetSize(); i++)
				program->savedOpcodes[i] = (Byte)program->instructions[i];
		}
	}

	if (!(linkFlags & LINK_KEEP_COMPILER))
	{
		pw.Start();
		compiler.Clear();
		compileStats.cleanupTime += pw.Stop() / 1000000.0;
	}

	return true;
}

String ScriptEngine::GetFunctionSignature(const StringRef &funcName) const
{
	Int fidx = program->functions.FindIndex(funcName);

	if (fidx < 0)
		return String();

	return program->functions.GetValue(fidx).typeSignature;
}

bool ScriptEngine::BindNativeFunction(const String &fname, const ConstPool::NativeCallback &callback)
{
	program->cpool.BindNativeFunc(fname, callback);
	return 1;
}

NativeClassProxy ScriptEngine::BindNativeClass(const String &cname, size_t size, size_t align, void(*ctor)(void *instptr), void(*dtor)(void *instptr))
{
	NativeClassProxy res(*this);
	res.success = true;
	res.handle = program->cpool.BindNativeClass(cname, (Int)size, (Int)align, ctor, dtor);

	return res;
}

NativeClassProxy ScriptEngine::BindNativeStruct(const String &cname, size_t size, size_t align)
{
	NativeClassProxy res(*this);
	res.success = true;
	res.handle = program->cpool.BindNativeStruct(cname, (Int)size, (Int)align);

	return res;
}

// create new script execution context
SharedPtr<ScriptContext> ScriptEngine::CreateContext(Int stkSize)
{
	ScriptContext *res = new ScriptContext(stkSize);
	res->engine = this;
	res->vmJit = vmJit;
	res->mode = mode;
	res->vm->SetProgram(program);

	auto execFlags = mode == ENGINE_DEBUG ? Vm::EXEC_DEBUG : 0;

	if (mode == ENGINE_DEBUG_NOBREAK)
		execFlags |= Vm::EXEC_NO_BREAK;

	res->vm->SetExecFlags(execFlags);

	MutexLock lock(contextMutex);
	contexts.Add(res);

	return res;
}

void ScriptEngine::ClearCompiler()
{
	compiler.Clear();
}

bool ScriptEngine::GetJitCode(const Byte *&ptr, Int &size)
{
	return vmJit ? vmJit->GetJitCode(ptr, size) : false;
}

Int ScriptEngine::FindFunctionOffset(const StringRef &fname) const
{
	if (!program)
		return 0;

	auto ci = program->functions.Find(fname);

	return ci == program->functions.End() ? 0 : ci->value.adr;
}

Int ScriptEngine::FindMethodIndex(const StringRef &fname, const Name &clsname, bool allowVirtual) const
{
	if (!program)
		return 0;

	auto *clstype = program->FindClass(clsname);

	if (!clstype)
		return 0;

	auto res = clstype->FindMethodOffset(fname);

	if (!allowVirtual && res < 0)
	{
		String tmp = clstype->name + "::" + fname;
		res = FindFunctionOffset(tmp);
	}

	return res;
}

const void *ScriptEngine::MethodIndexToPointer(Int idx) const
{
	const void *res = nullptr;

	if (!program)
		return res;

	if (idx < 0)
	{
		// virtual; simply store special vtbl index
		res = (const void *)(UIntPtr)(-idx*2 + 1);
	}
	else
	{
		// PC offset
		if (vmJit)
			res = vmJit->GetCodePtr(idx);
		else
			res = program->instructions.GetData() + idx;
	}

	return res;
}

String ScriptEngine::FindFunctionNameNear(const void *ptr) const
{
	String res;

	if (!ptr || !program || vmJit)
		return res;

	auto pc = (Int)(IntPtr)(static_cast<const Int *>(ptr) - program->instructions.GetData());

	while (pc >= 0)
	{
		auto ci = program->funcMap.Find(pc);

		if (ci != program->funcMap.End())
		{
			res = program->functions.GetKey(ci->value).key;
			break;
		}

		pc--;
	}

	return res;
}

String ScriptEngine::FindFunctionName(const void *ptr) const
{
	String res;

	if (!ptr || !program)
		return res;

	Int ofs = -1;

	if (vmJit)
		ofs = vmJit->FindFunctionPC(ptr);
	else
		ofs = (Int)(IntPtr)(static_cast<const Int *>(ptr) - program->instructions.GetData());

	if (ofs < 0)
		return res;

	auto ci = program->funcMap.Find(ofs);

	if (ci == program->funcMap.End())
		return res;

	res = program->functions.GetKey(ci->value).key;

	return res;
}

String ScriptEngine::FindMethodName(ScriptDelegate dg) const
{
	auto *obj = static_cast<ScriptBaseObject *>(dg.instancePtr);

	if (!obj || !program)
		return String();

	const auto *dt = obj->GetScriptClassType();

	if (!dt)
		return String();

	if (vmJit && !((UIntPtr)dg.funcPtr & 1))
	{
		Int pc = vmJit->FindFunctionPC(dg.funcPtr);
		LETHE_ASSERT(pc >= 0);

		if (pc >= 0)
			dg.funcPtr = program->instructions.GetData() + pc;
	}

	return dt->FindMethodName(dg, *program);
}

size_t ScriptEngine::GetMemUsageTypes() const
{
	LETHE_RET_FALSE(program);

	auto res = program->types.GetMemUsage();

	for (auto &&dt : program->types)
	{
		res += dt->GetMemUsage();
	}

	res += program->typeHash.GetMemUsage();
	res += program->classTypeHash.GetMemUsage();

	return res;
}

bool ScriptEngine::StartDebugServer()
{
	LETHE_RET_FALSE(mode == ENGINE_DEBUG && debugServer);

	auto contexts = GetContexts();

	for (auto &it : contexts)
		it->onDebugBreak.Set(debugServer.Get(), &DebugServer::OnDebugBreak);

	return debugServer->Start();
}

bool ScriptEngine::WaitForDebugger(Int msec) const
{
	LETHE_RET_FALSE(mode == ENGINE_DEBUG && debugServer);

	return debugServer->WaitForDebugger(msec);
}

bool ScriptEngine::CreateDebugServer(String ip, Int port)
{
	LETHE_RET_FALSE(mode == ENGINE_DEBUG);
	debugServer = new DebugServer(*this, ip, port);

	// sane default
	debugServer->onReadScriptFile = [](const String &fnm)->String
	{
		String res;

		VfsFile vf(fnm);

		if (vf.IsOpen())
		{
			Array<Byte> tmp;

			if (vf.ReadAll(tmp, 1))
			{
				tmp.Add(0);
				res = reinterpret_cast<const char *>(tmp.GetData());
			}
		}

		return res;
	};

	return true;
}

bool ScriptEngine::StopDebugServer()
{
	LETHE_RET_FALSE(mode == ENGINE_DEBUG);
	debugServer.Clear();
	return true;
}

}
