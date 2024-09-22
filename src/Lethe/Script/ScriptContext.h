#pragma once

#include "Common.h"

#include <Lethe/Core/Sys/NoCopy.h>
#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/Ptr/SharedPtr.h>
#include <Lethe/Core/Io/StreamDecl.h>
#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/Collect/HashSet.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/Collect/ArrayInterface.h>
#include <Lethe/Core/Thread/Lock.h>

#include <Lethe/Core/Lexer/Lexer.h>
#include <Lethe/Core/Time/Timer.h>

#include "Vm/Vm.h"

#include "Program/ConstPool.h"

#include "TypeInfo/DataTypes.h"

#include "TypeInfo/BaseObject.h"

namespace lethe
{

class ScriptContext;
class ScriptEngine;
class Builtin;
class CompiledProgram;

enum EngineMode
{
	// fastest option: JIT if possible
	ENGINE_JIT,
	// interpreted, no checking
	ENGINE_RELEASE,
	// debug with full checks (range/null pointers/break execution)
	ENGINE_DEBUG,
	// same as the above but don't allow custom break (should be a bit faster)
	ENGINE_DEBUG_NOBREAK
};

struct ScriptContextDebugData
{
	AtomicInt stepCmd = 0;
	Int activeStepCmd = 0;
	Int activeCallStackDepth = 0;
	TokenLocation origLoc;
	String origFunction;
};

LETHE_API_BEGIN

// script execution context (wraps vm and stack)
class LETHE_API ScriptContext : public NoCopy, public RefCounted
{
	typedef ScriptContext Self;
	~ScriptContext();
public:
	// stack size in stack words; 0 = default
	ScriptContext(Int stkSize = 0);

	// clone context (creates new stack and Vm)
	SharedPtr<ScriptContext> Clone() const;

	// get/set debug name
	String GetName() const;
	void SetName(const String &ctxname);

	// getters
	Stack &GetStack() const
	{
		return *vmStack;
	}
	Vm &GetVm() const
	{
		return *vm;
	}

	Array<String> GetCallStack(Int maxVarTextLen = 1024) const;
	// get callstack depth, useful for step over/step out
	Int GetCallStackDepth() const;

	// get global variables
	Array<String> GetGlobals(Int maxVarTextLen = 1024) const;

	// run global contructors/destructors
	ExecResult RunConstructors();
	ExecResult RunDestructors();

	// stack must be prepared (including thisPtr when calling a struct method)
	ExecResult Call(const StringRef &fname);

	// call class method (handles vtbls automatically)
	ExecResult CallMethod(const StringRef &fname, const void *instancePtr);

	// call class method by index (negative = vtbl index), see FindMethodIndex() in ScriptEngine
	ExecResult CallMethodByIndex(Int idx, const void *instancePtr);

	// call method via pointer; ONLY DO THIS IF YOU KNOW WHAT YOU'RE DOING!
	ExecResult CallMethodByPointer(const void *ptr, const void *instancePtr);

	// call delegate
	ExecResult CallDelegate(const ScriptDelegate &dg);

	// call function via pointer; ONLY DO THIS IF YOU KNOW WHAT YOU'RE DOING!
	ExecResult CallPointer(const void *funptr);

	// call function via PC bytecode offset
	ExecResult CallOffset(Int pcOffset);

	// for debugging: resume call, insPtr in Stack must be valid
	// only works in interpreter; ONLY DO THIS IF YOU KNOW WHAT YOU'RE DOING!
	ExecResult ResumeCall();

	// create new object (note: strong ref is 0, weak ref 1 after this call)
	BaseObject *NewObject(Name name);

	// construct/destruct script object (for native binding)
	void ConstructObject(Name name, void *inst);
	void DestructObject(Name name, void *inst);

	inline ScriptEngine &GetEngine() const {return *engine;}

	// get function signature for a function
	// empty => not found
	String GetFunctionSignature(const StringRef &funcName) const;

	inline void Lock() {mutex.Lock();}
	inline void Unlock() {mutex.Unlock();}

	// array interface (to support script dynamic array changes
	Int ArrayInterface(const DataType &elemType, ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam);

	// VM runtime error callback (not used for JIT, primarily for debugging)
	Delegate<void(const char *msg)> onRuntimeError;

	// debugging (only very basic support):

	// debug break handler; returns true to resume execution
	Delegate<bool(ScriptContext &ctx, ExecResult &res)> onDebugBreak;

	// debug break; only works in debug mode
	void Break() {Atomic::Store(vmStack->breakExecution, 1);}
	// resume from debug break; debug mode only
	void Resume() {Atomic::Store(vmStack->breakExecution, 0);}
	// in break mode?
	bool InBreakMode() const {return Atomic::Load(vmStack->breakExecution) != 0;}
	// get current location; must be in debug mode and break mode
	bool GetCurrentLocation(TokenLocation &nloc) const;

	ScriptContextDebugData &GetDebugData()
	{
		return debugData;
	}

	// custom state delegate support
	ScriptDelegate *GetStateDelegateRef() const {return stateDelegateRef;}
	void SetStateDelegateRef(ScriptDelegate *nref);

private:
	friend class Builtin;

	ScriptContextDebugData debugData;

	// profiling
	void ProfEnter(Int fnameIdx);
	void ProfExit(Int fnameIdx);

	struct ProfileInfo
	{
		Long calls = 0;
		ULong rootTicks = 0;
		ULong totalTicks = 0;
		ULong exclusiveTicks = 0;
		ULong childTicks = 0;
		Int nameIdx = -1;
		// depth: temporary to avoid counting recursion as inclusive
		Int depth = 0;
		// directly called from these (set of profile index)
		HashSet<Int> calledFrom;

		void SwapWith(ProfileInfo &o)
		{
			MemSwap(this, &o, sizeof(ProfileInfo));
		}

		bool operator <(const ProfileInfo &o) const
		{
			return exclusiveTicks > o.exclusiveTicks;
		}
	};

	struct ProfileStack
	{
		ULong startTicks;
		Int parentIndex;
	};

	Array<ProfileStack> profStack;
	HashMap<Int, ProfileInfo> profiling;
	Int profParent = -1;

	friend class ScriptEngine;

	String debugName;

	// program/constant pool is shared via engine
	ScriptEngine *engine;

	// local virtual machine
	UniquePtr<Vm> vm;
	UniquePtr<Stack> vmStack;
	// refptr to JIT (can be null)
	VmJitBase *vmJit;
	EngineMode mode;

	mutable Mutex mutex;

	ScriptDelegate *stateDelegateRef;

	void OnRuntimeError(const char *msg);
	bool OnDebugBreak(ScriptContext &ctx, ExecResult &res);
};

LETHE_API_END

}
