#pragma once

#include "ScriptContext.h"
#include "DebugServer/DebugServer.h"

namespace lethe
{

enum LinkFlags
{
	// skip code generation
	LINK_SKIP_CODEGEN = 1,
	// keep compiler and AST in memory
	LINK_KEEP_COMPILER = 2,
	// clone AST for find definition
	LINK_CLONE_AST_FIND_DEFINITION = 4
};

enum SingleStepMode
{
	STEP_INTO,
	STEP_OVER,
	STEP_OUT
};

struct CompileStats
{
	// cummulative I/O time (no easy way to measure!)
	Double ioTime;
	// cummulative compile time
	Double compileTime;
	// resolve time
	Double resolveTime;
	// codegen time
	Double codeGenTime;
	// JIT time
	Double jitTime;
	// cleanup time (freeing memory)
	Double cleanupTime;
};

class ScriptEngine;

struct LETHE_API NativeClassProxy
{
private:
	ScriptEngine &engine;
public:
	String error;
	// internal handle
	Int handle;
	bool success;

	explicit NativeClassProxy(ScriptEngine &eng);

	NativeClassProxy &Member(const String &name, size_t offset);
};

struct ScriptBreakpoint
{
	String filename;
	// bytecode program counter, <0 = invalid
	Int pc = -1;
	// enabled flag
	Int enabled = 0;

	bool IsValid() const {return pc >= 0;}
	bool IsEnabled() const {return enabled != 0;}
};

class LETHE_API ScriptEngine : public NoCopy, public RefCounted
{
	typedef ScriptEngine Self;

	friend class ScriptContext;
public:
	// wraps script program (and shared JIT)
	ScriptEngine(EngineMode emode);
	virtual ~ScriptEngine();

	// get actual script engine mode, if JIT is not available
	EngineMode GetMode() const {return mode;}

	// by default JIT or RELEASE don't do runtime checks
	// they can be enabled here (must be called before linking)
	// note: RT checks cannot be disabled in debug mode
	// returns true on success (fails if EngineMode is DEBUG)
	bool EnableRuntimeChecks(bool enable);

	// floatLitIsDouble: if true, floating point literals are treated as double instead of float
	static void SetFloatLiteralIsDouble(bool nfloatLitIsDouble);

	// enable profiling (instrumentation) - must be called before compiling!
	void EnableProfiling(bool enable);

	// enable inline function expansion? on by default for all modes
	void EnableInlineExpansion(bool enable);

	// compile file/stream
	bool CompileBuffer(const char *buf, const String &filename);
	bool CompileFile(const String &filename);
	bool CompileStream(Stream &stream, const String &filename);

	const CompileStats &GetStats() const
	{
		return compileStats;
	}

	inline const Compiler &GetCompiler() const
	{
		return *compiler;
	}

	inline Compiler &GetCompiler()
	{
		return *compiler;
	}

	inline const Array<UniquePtr<DataType>> &GetTypes() const
	{
		return program->types;
	}

	inline const DataType *FindClass(Name n) const
	{
		return program->FindClass(n);
	}

	inline const Array<String> &GetConstStrings() const
	{
		return program->cpool.sPool;
	}

	void *GetClassVtable(Name n) const;

	// bind native function (fully qualified name)
	bool BindNativeFunction(const String &fname, const ConstPool::NativeCallback &callback);

	// get function signature for a function
	// empty => not found
	String GetFunctionSignature(const StringRef &funcName) const;

	// bind native class (fully qualified name)
	// this should return a binding proxy
	NativeClassProxy BindNativeClass(const String &cname, size_t size, size_t align, void (*ctor)(void *instptr) = nullptr, void (*dtor)(void *instptr) = nullptr);

	// bind native struct (fully qualified name)
	NativeClassProxy BindNativeStruct(const String &cname, size_t size, size_t align);

	// link compiled programs (all native functions must be bound at this point)
	// see LinkFlags
	bool Link(int linkFlags = 0);

	// create new script execution context
	// stkSize = desired stack size in stack words, 0 = default
	SharedPtr<ScriptContext> CreateContext(Int stkSize = 0);

	// get created script contexts
	Array<SharedPtr<ScriptContext>> GetContexts() const;

	// free memory used by compiler (AST and so on)
	void ClearCompiler();

	// special JIT functions:
	bool GetJitCode(const Byte *&ptr, Int &size);

	ScriptContext &GetStockContext();

	// convert (delegate) method index to pointer
	// 0 = invalid
	// >0 = bytecode PC
	// <0 = -vtbl index
	const void *MethodIndexToPointer(Int idx) const;

	// find method name for delegate
	String FindMethodName(ScriptDelegate dg) const;

	// find function name from func ptr
	String FindFunctionName(const void *ptr) const;
	// same as above but looks up for function start
	// note: only works in intepreter mode
	String FindFunctionNameNear(const void *ptr) const;
	// find function name near program counter
	String FindFunctionNameNearPC(Int pc) const;

	// convert from JIT code address to bytecode program counter
	// return -1 if not found (or if in interpreter mode)
	Int JitCodeToPC(const void *adr) const;

	// get location description at program counter
	String GetLocationDescriptionAtPC(Int pc) const;

	// find method index for class
	// 0 = not found, <0 = -vtbl_index, >0 = PC offset
	Int FindMethodIndex(const StringRef &fname, const Name &clsname, bool allowVirtual = true) const;

	// returns nullptr if not found, otherwise pointer to data; must be called after RunConstructors!
	void *FindGlobalVariable(const String &n) const;

	// find function index
	// 0 = not found, >0 = PC offset
	Int FindFunctionOffset(const StringRef &fname) const;

	// compile error callbacks
	Delegate< void(const String &msg, const TokenLocation &loc) > onError;
	Delegate< void(const String &msg, const TokenLocation &loc, Int warnid) > onWarning;

	// compile start callback
	Delegate< void(const String &filename) > onCompile;
	// resolve steps callback
	Delegate< void(Int steps) > onResolve;

	// info callback (profiler etc.)
	Delegate< void(const String &msg) > onInfo;

	// this may be necessary to sync native object with vtable change
	Delegate<void(BaseObject *obj, Name n)> onVtableChange;

	// this may be necessary to sync native object
	// this is called each time an object is created via script
	Delegate<void(BaseObject *obj, const DataType &dt)> onNewObject;

	// debugging:

	// get program counter for breakpoint at file/line
	Int GetBreakpointPC(const String &filename, Int nline) const;
	// return multiple program counters; necessary for templates
	Array<Int> GetBreakpointPCList(const String &filename, Int nline) const;

	// set breakpoint at specific location
	// returns breakpoint handle, -1 if none
	Int SetBreakpoint(const String &nfilename, Int npc, bool enabled = true);
	// change breakpoint position
	bool ChangeBreakpoint(Int handle, const String &filename, Int npc);
	// toggle breakpoint at specific location
	// returns breakpoint handle, -1 if none/error
	Int ToggleBreakpoint(const String &nfilename, Int npc, bool enabled = true);
	// enable/disable breakpoint
	bool EnableBreakpoint(Int handle, bool nenable);
	// delete breakpoint
	void DeleteBreakpoint(Int handle);
	// delete all breakpoints
	void DeleteAllBreakpoints();
	// get current breakpoints
	Array<ScriptBreakpoint> GetBreakpoints() const;

	// note: may return null
	DebugServer *GetDebugServer() const {return debugServer.Get();}

	bool CreateDebugServer(String ip = "127.0.0.1", Int port = 27100);
	bool StartDebugServer();
	bool StopDebugServer();

	// wait for debugger to connect, up to msec milliseconds
	// returns true if connected
	bool WaitForDebugger(Int msec) const;

	// get number of live script objects
	Int GetLiveScriptObjectCount() const;

	// get approx memory usage (lower bound) for program type descriptors
	size_t GetMemUsageTypes() const;

	Int GetByteCodeSize() const;
	String DisassembleByteCode(Int pc);

	String GetInternalProgram() const;

	const AstNode *GetFindDefinitionRoot() const;

private:
	friend struct NativeClassProxy;
	friend class CompiledProgram;

	static bool floatLitIsDouble;

	UniquePtr<CompiledProgram> program;
	UniquePtr<Compiler> compiler;
	// JIT virtual machine (shared among contexts)
	UniquePtr<VmJitBase> vmJit;
	EngineMode mode;

	CompileStats compileStats;

	SharedPtr<ScriptContext> stockCtx;

	UniquePtr<AstNode> findDefRoot;

	// list of all contexts for this engine
	mutable Mutex contextMutex;
	Array<ScriptContext *> contexts;

	mutable Mutex breakpointMutex;
	Array<ScriptBreakpoint> breakpoints;
	FreeIdList freeBreakpoints;

	String internalProg;

	SharedPtr<DebugServer> debugServer;

	void SetupVtbl(void *clsPtr);

	void OnError(const String &msg, const TokenLocation &loc);
	void OnWarning(const String &msg, const TokenLocation &loc, Int warnid);
	void OnCompile(const String &filename);
	void OnResolve(Int steps);

	Int SetBreakpointInternal(const String &nfilename, Int npc, bool enabled);
};

}
