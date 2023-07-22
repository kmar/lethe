#pragma once

#include "../Common.h"

#include <Lethe/Core/Delegate/Delegate.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Core/Collect/FreeIdList.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/Collect/Queue.h>
#include <Lethe/Core/Collect/BitSet.h>
#include <Lethe/Core/Thread/Lock.h>
#include <Lethe/Script/TypeInfo/DataTypes.h>
#include <Lethe/Script/Compiler/Warnings.h>
#include "ConstPool.h"

#include <Lethe/Script/ScriptBaseObject.h>

namespace lethe
{
struct TokenLocation;
}

namespace lethe
{

typedef ScriptBaseObject ScriptObjectBase;

class NamedScope;
class AstFunc;
class AstNode;
class AstLabel;

class VmJitBase;

class ScriptEngine;

class LETHE_API ErrorHandler
{
public:
	// error/warning delegate
	Delegate<void(const String &msg, const TokenLocation &loc)> onError;
	Delegate<void(const String &msg, const TokenLocation &loc, Int warnid)> onWarning;

	// error/warning
	// note: Error always returns false
	LETHE_NOINLINE bool Error(const AstNode *n, const String &msg) const;
	LETHE_NOINLINE bool Error(const AstNode *n, const char *msg) const;
	LETHE_NOINLINE void Warning(const AstNode *n, const String &msg, Int warnid = WARN_GENERIC) const;
	LETHE_NOINLINE void Warning(const AstNode *n, const char *msg, Int warnid = WARN_GENERIC) const;

	// temporary string table (fully qualified symbols)
	mutable Mutex stringTableMutex;
	mutable HashSet< String > stringTable;

	const String &AddString(const String &str) const;
	const String &AddString(const StringRef &sr) const;

	// special scopes for native props
	NamedScope *stringScope = nullptr;
	NamedScope *arrayRefScope = nullptr;
	NamedScope *arrayScope = nullptr;
	NamedScope *dynamicArrayScope = nullptr;

	// late deletion of AST nodes (used by ADL resolve)
	void AddLateDeleteNode(AstNode *node) const;
	void FlushLateDeleteNodes();

	// ADL pass flag
	bool tryADL = false;

	// fold sizeof flag
	bool foldSizeof = false;

	// check variable shadowing
	static void CheckShadowing(const NamedScope *cscope, const String &nname, AstNode *nnode, const Delegate<void(const String &msg, const TokenLocation &loc, Int warnid)> &onWarn);

private:
	mutable SpinMutex lateDeleteMutex;
	mutable HashSet<AstNode *> lateDeleteNodes;
};

class LETHE_API CompiledProgram : public ErrorHandler
{
public:
	CompiledProgram(bool njitFriendly = false);

	struct FuncDesc
	{
		// back-ref to AST node (temporary)
		AstFunc *node;
		// type signature (empty = none)
		String typeSignature;
		// address
		Int adr;
	};

	// functions; simply points to start_index
	HashMap< String, FuncDesc > functions;
	// function pointer map; index into functions
	HashMap< Int, Int > funcMap;
	// VM instructions
	Array< Int > instructions;
	// in debug mode only, because of breakpoints
	// note that breakpoints only modify opcode (OPC_BREAK)
	// so we can save memory this way
	Array< Byte > savedOpcodes;
	// optimization barriers (sorted)
	Array< Int > barriers;
	// switch data range (disassembly)
	Array<Int> switchRange;
	// temporary for function flow analysis
	Array<Int> returnValues;
	// loop starts for JIT code alignment (sorted)
	Array<Int> loops;

	struct CodeToLine
	{
		Int pc;
		Int line;
		String file;

		bool operator <(const CodeToLine &o) const
		{
			return pc < o.pc;
		}
	};

	// should be sorted!
	Array<CodeToLine> codeToLine;

	// codegen temporary: global destructor instructions
	Queue< Int > globalDestInstr;

	// stored special types
	Array< UniquePtr<DataType> > types;
	// to merge array types
	HashMap<String, DataType *> typeHash;
	// class types by name
	HashMap<Name, const DataType *> classTypeHash;
	// null (empty) structs by name
	HashSet<Name> nullStructTypeHash;

	// add composite type
	const DataType *AddType(DataType *newType);
	void AddClassType(Name n, const DataType *dt);

	// find class type
	const DataType *FindClass(Name n) const;

	// need constant pool as well...
	ConstPool cpool;
	// hmm, maybe a cpool per function is not a bad idea at all...
	// TODO: and debug info (*sigh*)

	void Optimize();

	void EnterScope(NamedScope *scopeRef);
	// virt = virtual; for args
	bool LeaveScope(bool virt = 0);
	// leave scope chain using break/continue
	NamedScope *BreakScope(bool isContinue = 0);
	// leave scope chain using return
	// returns true if we can return immediately
	bool ReturnScope(bool retOpt = 1);
	// emit cleanup & return from state via statebreak method call
	bool StateBreakScope();
	// leave scope chain using goto
	bool GotoScope(const AstLabel *nlabel);

	void DynArrayVarFix(QDataType qdt);

	void EmitFunc(const String &fnm, AstFunc *fn, const String &typeSignature = String());

	void EmitCtor(QDataType qdt);
	void EmitGlobalCtor(QDataType qdt, Int offset);

	void Emit(UInt ins);
	bool EmitConv(AstNode *n, const QDataType &src, const QDataType &dstq, bool warn = true);
	bool EmitGlobalCopy(AstNode *n, const DataType &src, Int offset);
	bool EmitGlobalDtor(AstNode *n, const DataType &src, Int offset);
	void EmitLocalDtor(const DataType &src, Int offset);

	// special I24 version
	void EmitI24(Int opc, Int data);
	void EmitU24(Int opc, Int data);
	// converts PUSH_RAW into PUSHZ_RAW in debug mode
	void EmitI24Zero(Int opc, Int data);
	void EmitU24Zero(Int opc, Int data);

	UInt GenIntConst(Int iconst);
	UInt GenUIntConst(UInt iconst);
	UInt GenFloatConst(Float fconst);
	UInt GenDoubleConst(Double dconst);
	void EmitIntConst(Int iconst);
	void EmitUIntConst(UInt iconst);
	void EmitLongConst(Long iconst);
	void EmitULongConst(ULong iconst);
	void EmitFloatConst(Float fconst);
	void EmitDoubleConst(Double dconst);
	void EmitNameConst(Name n);

	void EmitAddRef(QDataType dt);

	// register vtbl so that it can be converted to pointers later
	void AddVtbl(Int offset, Int count);

	// all vtbls resolved?
	bool VtblOk() const;

	// convert vtbls to pointers
	void FixupVtbl();
	void FixupVtblJit(const Array<Int> &pcToCode, const Byte *code);

	// get program counter (emit)
	Int GetPc() const;
	// returns forward target handle idx
	Int EmitForwardJump(UInt ins);
	bool EmitBackwardJump(UInt ins, Int target);
	bool FixupForwardTarget(Int fwHandle);

	// add forward jump handles
	void AddReturnHandle(Int handle);

	// when JITting, native stack will be used for return addr
	bool IsFastCall() const
	{
		return jitFriendly;
	}

	bool FixupHandles(Array<Int> &handles);
	bool FixupReturnHandles();
	inline Array<Int> &GetReturnHandles() {return returnHandles;}

	// flow analysis (all paths must return a value)
	void MarkReturnValue(Int delta = 0);
	void InitValidReturnPath(Int from, Int to);
	void MarkValidReturnPathTarget(Int pc);
	bool IsValidReturnPathTarget(Int pc) const;

	// codegen helper
	static UInt ConvJump(DataTypeEnum dte, UInt ins);

	DataType elemTypes[DT_MAX];

	const DataType &Coerce(const DataType &dt0, const DataType &dt1) const;

	void PushStackType(const QDataType &qdt);
	void PopStackType(bool nocleanup = 0);

	// because of stupid comma operator
	Int ExprStackMark() const;
	void ExprStackCleanupTo(Int mark);

	// flush opt base => cannot optimize past instructions
	void FlushOpt(bool nobarrier = false);

	// can optimize previous instruction?
	inline bool CanOptPrevious() const
	{
		return emitOptBase < instructions.GetSize();
	}

	inline bool GetUnsafe() const
	{
		return unsafe;
	}
	inline void SetUnsafe(bool nunsafe = true)
	{
		unsafe = nunsafe;
	}

	inline bool GetJitFriendly() const
	{
		return jitFriendly;
	}

	inline bool GetProfiling() const
	{
		return profiling;
	}

	void SetProfiling(bool nprofiling = true)
	{
		profiling = nprofiling;
	}

	void ProfEnter(const String &fname);
	void ProfExit();

	bool IsValidCodePtr(const Int *iptr) const;

	void SetLocation(const TokenLocation &loc);

	void SetInline(Int inlDelta)
	{
		inlineCall += inlDelta;
	}
	Int GetInline() const
	{
		return inlineCall;
	}
	bool InlineExpansionAllowed() const
	{
		return inlineExpansionAllowed;
	}

	static bool IsConvToBool(Int ins);

	static bool IsCall(Int ins);

	void SetupVtbl(Int ofs);

	// current expression stack
	Array< QDataType > exprStack;
	Array< Int > exprStackOffset;
	// expr_stack args ofs
	Int exprStackOfs;
	// extra offset because of nrvo and initializer lists
	Int initializerDelta;

	// index of last forward jump, -1 if none
	Int lastForwardJump;
	NamedScope *curScope;
	// current scope index
	Int curScopeIndex;
	// global scope UID
	Int scopeIndex;

	// last global ctor instruction
	Int globalConstIndex;
	// global destructors PC index (-1 if none)
	Int globalDestIndex;

	// native function map (temporary)
	HashMap<String, AstNode *> nativeMap;

	struct LocalVarDebugKey
	{
		// scope index
		Int index;
		Int offset;

		bool operator ==(const LocalVarDebugKey &o) const
		{
			return index == o.index && offset == o.offset;
		}
	};

	struct DebugInfoVar
	{
		String name;
		QDataType type;
		Int offset;
		Int startPC;
		Int endPC;
		bool isLocal;
	};

	HashMap<LocalVarDebugKey, DebugInfoVar> localVars;

	// convert qualified state class name to local state name
	HashMap<Name, Name> stateToLocalNameMap;

	// this is necessary to fixup state name from base class to new
	static inline ULong PackNames(Name n0, Name n1)
	{
		return ((ULong)n0.GetIndex() << 32) | (UInt)(n1.GetIndex());
	}

	// Pack(class name, state localName) => new qualified state class name
	HashMap<ULong, Name> fixupStateMap;

	void StartStackFrame();
	void EndStackFrame();
	void StartLocalVarLifeTime(const QDataType &qdt, Int offset, const String &name);
	void EndLocalVarLifeTime(Int offset);

	VmJitBase *jitRef;

	// dtor cache (we only have to generate once)
	Int strongDtor;
	Int strongVDtor;
	Int weakDtor;
	Int weakVDtor;

	enum InternalFunc
	{
		IFUNC_INIT,
		IFUNC_COPY,
		IFUNC_MAX
	};

	const String &GetInternalFuncName(InternalFunc ifunc) const;

	void EmitGlobalDestCall(Int pc);

	// latent call support:
	void ClearLatentCounter();
	// returns -1 if stack level compromised
	Int IncLatentCounter();
	inline Int GetLatentCounter() const {return compiledProgramLatentCounter;}
	// returns false if locals are on stack
	bool CheckLatentStack() const;

	// debugging:
	bool SetBreakpoint(Int pc, bool enable);

	// returns true if code at PC is not actually code but switch table data
	bool IsSwitchTable(Int pc) const;

private:
	friend class ScriptEngine;

	enum ElemConvType
	{
		ECONV_BOOL,
		ECONV_CHAR,
		ECONV_INT,
		ECONV_UINT,
		ECONV_LONG,
		ECONV_ULONG,
		ECONV_FLOAT,
		ECONV_DOUBLE,
		ECONV_NAME,
		ECONV_STRING,
		ECONV_NULL,
		// max or invalid
		ECONV_MAX
	};

	struct ScopeDesc
	{
		NamedScope *oldScope;
		Int varOfsBase;
		Int scopeIndex;
	};

	String ifuncNames[IFUNC_MAX];

	ScriptEngine *engineRef;

	// forward fixups:
	FreeIdList flist;
	Array<Int> fixupTargets;
	// we can emit-opt since this instruction

	// return handles
	Array< Int > returnHandles;
	// vtables (index, count) pairs
	Array<Int> vtbls;

	struct GlobalDestFixup
	{
		Int target;
		// reverse pc for fixup from end of queue
		Int revPc;
	};

	// global destructor call fixups
	Array<GlobalDestFixup> globalDestFixups;

	// scope stack
	Array< ScopeDesc > scopeStack;

	Int stackFrameBase;

	Int emitOptBase;
	Int jumpOptBase;

	bool unsafe;
	// JIT-friendly mode generates special opcodes for floats
	bool jitFriendly;
	// profiling mode
	bool profiling;
	// can be disabled via ScriptEngine
	bool inlineExpansionAllowed = true;
	// set before inline call
	Int inlineCall;

	// latent counter
	Int compiledProgramLatentCounter = 0;
	Int latentStackLevel = 0;

	// state break lock
	Int stateBreakLock = 0;

	static const Int elemConvTab[ECONV_MAX][ECONV_MAX];
	ElemConvType ElemConvFromDataType(const DataTypeEnum dte);

	// float analysis temps
	BitSet validReturnTargets;
	Int validReturnTargetStart;

	// for profiling instrumentation
	Array<String> profFuncName;

	void AddGlobalDestFixup(Int pc);

	Int GetInsType(Int idx) const;
	Int GetInsImm24(Int idx) const;

	void EmitIns(UInt ins);
	void EmitInternal(UInt ins);

	static bool IsCondJump(Int ins);
	static bool IsCondJumpNoFloat(Int ins);
	static Int FlipJump(Int ins);

	void Finalize();

	void EmitDelStr(Int offset);

	// encode helper
	bool CanEncodeI24(Int val) const;

	bool EmitDefer(NamedScope *cscope, Int nstart = 0);
};

}
