#pragma once

#include "Opcodes.h"
#include "Stack.h"

#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/String/StringRef.h>
#include <Lethe/Core/Delegate/Delegate.h>

namespace lethe
{

class CompiledProgram;
class ConstPool;
class Vm;
class VmJitX86;
class ScriptContext;

// instructions are always encoded as 32-bit integers
/*
	encoding:
	lsbyte = opcode
	imm24 mode:
		signed top 24 bits
	uimm24 mode:
		unsigned top 24 bits
	imm16 mode:
		signed most 16 bits
		+uimm8#2 unsigned next 8 bits
	need constant pool to push large values OR I could actually fetch next dword, this would be
		more cache friendly but would break RISC architecture (and also alignment for 64-bit values) => use constant pool!

	stack-only modes:
		- add two topmost stack values, store result in 1 below top, pop
		- add imm24 to stack top
		- add stk-rel uimm8 [dst], stk-rel uimm8[src], imm8 [src2]
	global modes:
		- add top of stack to global at uimm24, pop
		I think one will do, we don't really need to optimize globals (hmm, really?!); problem is there may be a lot of globals
		so really, no special modes for globals => globals will be slow by design; in fact I could just support loads/stores
	indirect modes:
		- (static)array
		- object
		- struct

	so... primarily we need to add relative quantities
	SO there will be no direct cpool mode but instead we'll be able to load const on stack
	3-way immed mode
	add stkrel, stkrel, imm8 => this will replace add stack-relative imm16

	because of structures on stack, we need to be able to load byte, sbyte, short, ushort as well; but they may require byte-alignment!
	ALSO we need to store such quantities! *sigh*

	think: use reverse subtract? probably yes
	hmm... best would be to use generated code for VM ops

	for best performance, this must be handled as a register => the caller is responsible for saving/restoring this ptr;
	otherwise if performance is not a concern we might simply always push it on stack
	using a register will also allow for consistent arg parsing (no matter if func/method)
	BUT this might cause problems for coroutine stack serialization (latent/thread) => it would anyway so probably ok...

	OK, time to redesign (maybe?)

	well, as I add more opcodes (especially string-related), performance goes to hell
	I'm tempted to emulate strings via native calls => yes, builtins rule


	smart pointer stuff:
	addref_strong (just ptr)
	release_strong (+zero?)
	addref_weak
	release_weak (+zero?)
	weak_to_strong (+zero target?)
*/

// right now it's 2.5x slower than Lua! SHAME!!!
// ok with optimizations it's now a bit faster but still...
// 43x slower than assembly!
// PROBLEM: I've already consumed half of opcodes!; a LOT is still missing:
// all unary ops, extra conversions, long/double/string ops, this calls, ...
// running out of opcodes! only 48 left! => will probably have to abandon certain types; no way I'll fit in 256 this way
// => I'll probably get rid of (u)long and double... but at least long would be damn useful
// I'm very tempted to drop long/double for several reasons
// #1: this will simplify a lot of things (goodbye alignment; well not for structs)
// #2: (obviously): this will save lots of opcodes
// #3: it (might?) enable portable bytecode (i.e. 32-bit vs 64-bit) [no]
// bad things: - cannot use long seeking for files
//             - cannot use high-precision doubles
// alternatively I might consider using a reduced set of ops for long/doubles
// hmm, slow long may be emulated with int/uint combo => decided, no support for long/double!
// special float commands (LPUSH32F and others) are not generated when JIT is disabled, they must be +1 relative to non-jit commands!

struct Disasm
{
	const char *name;
	VmOpCode type;
	Operands operands;
};

enum ExecResult
{
	EXEC_OK,
	// attempt to call JIT exec on dumb proxy
	EXEC_NO_JIT,
	// null ptr passed to exec by ptr
	EXEC_NULL_PTR,
	// invalid program counter passed
	EXEC_INVALID_PC,
	// function to call not found
	EXEC_FUNC_NOT_FOUND,
	// null instance ptr passed to method call
	EXEC_NULL_INSTANCE,
	// method not founc
	EXEC_METHOD_NOT_FOUND,
	// no program/stack
	EXEC_NO_PROG,
	// runtime exception (debug only)
	EXEC_EXCEPTION,
	// breakpoint hit (debug only)
	EXEC_BREAKPOINT,
	// program break (debug only)
	EXEC_BREAK
};

class LETHE_API VmJitBase
{
public:
	virtual ~VmJitBase() {}

	virtual Int GetPCFromCodePtr(const void * /*codePtr*/) const {return -1;}

	virtual const void *GetCodePtr(Int pc) const = 0;

	virtual bool GetJitCode(const Byte *&ptr, Int &size) = 0;

	virtual bool CodeGen(CompiledProgram &prog) = 0;

	virtual ExecResult ExecScriptFunc(Vm &vm, Int scriptPC) = 0;

	virtual ExecResult ExecScriptFuncPtr(Vm &vm, const void *address) = 0;

	// returns -1 if not found
	virtual Int FindFunctionPC(const void *address) const = 0;

};

class LETHE_API Vm
{
public:
	// Execute bit flags:
	// (note: only valid for interpreter)
	enum ExecuteFlags
	{
		// additional checks (null pointers) + allow to break program execution
		EXEC_DEBUG = 1,
		EXEC_NO_BREAK = 2,

		EXEC_MASK = 3
	};

	typedef Delegate< void(Stack &) > CallbackFunc;
	Vm();

	ExecResult Execute(Int pc = 0);
	ExecResult ExecutePtr(const void *adr);

	template<Int flags>
	ExecResult ExecuteTemplate(const Instruction *iptr);

	Array<String> GetCallStack(const Instruction *iptr) const;
	Int GetCallStackDepth(const Instruction *iptr) const;
	// get locals for stack frame
	Array<String> GetLocals(const Instruction *iptr, const Stack::StackWord *sptr, bool withType = false) const;
	// get this for stack frame (program counter), startpc = function start pc
	Array<String> GetThis(Int pc,Int startpc) const;

	// find script func, returns pc index (or -1 if not found)
	Int FindFunc(const StringRef &fname) const;

	// call script func
	ExecResult CallFunc(const StringRef &fname);

	// call global constructors (if any)
	ExecResult CallGlobalConstructors();
	// call global destructors (if any)
	ExecResult CallGlobalDestructors();

	String Disassemble(Int pc, Int ins) const;

	// see EXEC_xxxx flags
	inline void SetExecFlags(Int nflags)
	{
		execFlags = nflags;
	}

	inline Int GetExecFlags() const
	{
		return execFlags;
	}

	void SetStack(Stack *stk);
	void SetProgram(CompiledProgram *prg);

	// stack
	Stack *stack;

	// compiled program
	CompiledProgram *prog;

	// native functions
	HashMap< String, CallbackFunc > nativeFuncs;

	Delegate< void(const char *) > onRuntimeError;

	// debug break handler; returns true to continue execution; otherwise returns (potentially modified) result
	Delegate<bool(ScriptContext &ctx, ExecResult &)> onDebugBreak;

	LETHE_NOINLINE ExecResult RuntimeException(const Instruction *iptr, const char *msg);

	Int FindFuncStart(Int pc) const;

private:
	Int execFlags;

	template<bool dg>
	LETHE_NOINLINE ExecResult DoFCall(const Instruction *&iptr, Stack &stk);

	String DisassembleInternal(Int pc, Int ins) const;
	String GetFuncName(Int pc) const;

	// pc = func base ptr, opc = inside
	String GetFullCallStack(Int pc, Int opc) const;
	Int GetFullCallStackDepth(Int pc, Int opc) const;
	const Stack::StackWord *FindStackFrame(const Stack::StackWord *ptr) const;
};

}
