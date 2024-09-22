#pragma once

#include "AsmX86.h"

#include "../Vm.h"

#include <Lethe/Core/Collect/HashMap.h>

namespace lethe
{

// reg alloc bit flags
enum RegAlloc
{
	RA_LOAD = 1,
	RA_WRITE = 2,
	RA_PTR = 4,
	RA_DOUBLE = 8
};

LETHE_API_BEGIN

class LETHE_API VmJitX86 : public AsmX86, public VmJitBase
{
public:
	VmJitX86();
	~VmJitX86();

	Int GetPCFromCodePtr(const void *codePtr) const override;

	const void *GetCodePtr(Int pc) const override;

	bool GetJitCode(const Byte *&ptr, Int &size) override;

	bool CodeGen(CompiledProgram &prog) override;

	ExecResult ExecScriptFunc(Vm &vm, Int scriptPC) override;

	ExecResult ExecScriptFuncPtr(Vm &vm, const void *address) override;

	Int FindFunctionPC(const void *address) const override;

private:

	static inline Int DecodeImm24(Int ins)
	{
		return ins >> 8;
	}

	static inline Int DecodeUImm24(Int ins)
	{
		return (UInt)((UInt)ins >> 8);
	}

	static inline Int DecodeUImm8(Int ins, Int idx)
	{
		return (Int)(((UInt)ins >> (8*(idx+1))) & 255);
	}

	static inline Int DecodeImm8Top(Int ins)
	{
		return ins >> 24;
	}

	static inline Int DecodeImm16Top(Int ins)
	{
		return ins >> 16;
	}

	static inline UInt DecodeUImm16Top(Int ins)
	{
		return (UInt)ins >> 16;
	}

	struct Fixup
	{
		// generated code offset
		Int codeOfs;
		// byte code offset
		Int byteOfs;
		// relative flag (bit 0 = flag, bit 1 = short)
		Byte relative;
	};

	// don't flush stackopt guard
	struct DontFlush
	{
		DontFlush(VmJitX86 &nvmj) : vmj(&nvmj)
		{
			old = vmj->dontFlush;
			vmj->dontFlush = 1;
		}

		~DontFlush()
		{
			Dismiss();
		}

		void Dismiss()
		{
			if (old >= 0)
			{
				vmj->dontFlush = old != 0;
				old = -1;
			}
		}
		VmJitX86 *vmj;
		Int old;
	};

	struct PreserveFlags
	{
		PreserveFlags(VmJitX86 &nvmj) : vmj(&nvmj)
		{
			old = vmj->preserveFlags;
			vmj->preserveFlags = 1;
		}
		~PreserveFlags()
		{
			vmj->preserveFlags = old;
		}
		VmJitX86 *vmj;
		bool old;
	};

	Array< Fixup > fixups;
	// map bytecode pc to machine code offset
	Array< Int > pcToCode;
	// previous bytecode map to optimize fwd jumps
	Array< Int > prevPcToCode;
	// sorted function PC offsets (in bytecode)
	Array<Int> funcOfs;
	// code offsets for functions
	Array<Int> funcCodeOfs;
	// function code ptr to PC
	HashMap<const void *, Int> funcCodeToPC;

	Int stackOpt;

	RegExpr lastAdrExpr;
	Int lastAdr;

	RegExpr globalBase;
	RegExpr stackObjectPtr;
	RegExpr nativeFuncPtr;
	RegExpr firstArgReg;
	// global data: ui to float conversion table
	Int uiConvTable;

	// temporaries for short jump optimization
	Array<Int> jumpSource;
	Array<Int> prevJumpSource;

	static const Int INVALID_STACK_INDEX = 0x7fffffff;
	static const Int TEMP_STACK_INDEX    = -0x7fffffff;

	bool dontFlush;
	bool preserveFlags;
	bool fastCall;

	void AlignCode(Int align, bool dumb = false);

	void SetLastAdr(const RegExpr &re);
	void FlushLastAdr();

	bool WouldFlushLastAdr() const;
	bool WouldFlush() const;

	void EmitJump(Cond cond, Int target);
	void EmitJumpNan(Cond cond, Int target);

	// push constant on stack
	void PushInt(UInt b, bool isPtr = 0);
	// push float constant on stack
	void PushFloat(const CompiledProgram &prog, Float f);
	// push double constant on stack
	void PushDouble(const CompiledProgram &prog, Double d);
	// push ptr constant 0 on stack (reserves)
	void PushPtr();
	// push accum on stack
	void PushIntAccum(const RegExpr &nreg, bool isPtr = 0);
	// load stk-rel int into accum
	RegExpr GetInt(Int i);
	// get sign-extended int in 64-bit mode => used by AADD_xxx
	RegExpr GetIntSignExtend(Int i);

	// load stk-rel float into xmm accum
	RegExpr GetFloat(Int i);
	RegExpr GetDouble(Int i);
	// push accum on stack
	void PushPtrAccum(const RegExpr &nreg);
	// load stk-rel ptr into accum
	RegExpr GetPtr(Int i);
	// ignore lastadr
	RegExpr GetPtrNoAdr(Int i);
	// load stk-rel addr into accum
	RegExpr GetStackAdr(Int ofs);
	// load global int/ptr into accum
	RegExpr GetGlobalInt(Int ofs, bool isPtr = 0);
	RegExpr GetGlobalFloat(Int ofs);
	RegExpr GetGlobalDouble(Int ofs);
	RegExpr GetGlobalPtr(Int ofs);
	void SetGlobalInt(Int ofs, const RegExpr &nreg);
	void SetGlobalFloat(Int ofs, const RegExpr &nreg);
	void SetGlobalDouble(Int ofs, const RegExpr &nreg);
	void SetGlobalPtr(Int ofs, const RegExpr &nreg);
	// pop stack words
	void Pop(Int count);
	void PopGen(Int count);

	RegExpr GetGlobalSByte(Int ofs);
	RegExpr GetGlobalByte(Int ofs);
	RegExpr GetGlobalShort(Int ofs);
	RegExpr GetGlobalUShort(Int ofs);
	void SetGlobalByte(Int ofs, const RegExpr &nreg);
	void SetGlobalUShort(Int ofs, const RegExpr &nreg);

	RegExpr GetGlobalAdr(Int ofs);

	RegExpr GetLocalSByte(Int ofs);
	RegExpr GetLocalByte(Int ofs);
	RegExpr GetLocalShort(Int ofs);
	RegExpr GetLocalUShort(Int ofs);

	void SetLocalByte(Int ofs, const RegExpr &src);
	void SetLocalUShort(Int ofs, const RegExpr &src);
	void SetLocalInt(Int ofs, const RegExpr &src, bool transfer = 0, bool pointer = 0);
	void SetLocalFloat(Int ofs, const RegExpr &src, bool transfer = 0);
	void SetLocalDouble(Int ofs, const RegExpr &src, bool transfer = 0);
	void SetLocalPtr(Int ofs, const RegExpr &src, bool transfer = 0);

	template< bool imm >
	RegExpr GetIndirectAdr(Int &scl, RegExpr &ofs);

	template< bool imm >
	void GetIndirectSByte(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectByte(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectShort(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectUShort(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectInt(Int scl, Int ofs, bool isPtr = 0);
	template< bool imm >
	void GetIndirectFloat(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectDouble(Int scl, Int ofs);
	template< bool imm >
	void GetIndirectPtr(Int scl, Int ofs);

	void StoreIndirectImm(Int ofs, MemSize msz, bool isDouble = false);
	void StoreIndirectImmByte(Int ofs);
	void StoreIndirectImmUShort(Int ofs);
	void StoreIndirectImmInt(Int ofs);
	void StoreIndirectImmFloat(Int ofs);
	void StoreIndirectImmDouble(Int ofs);
	void StoreIndirectImmPtr(Int ofs);

	template<int size, bool uns>
	void PInc(Int ofs);
	template<int size, bool uns>
	void PIncPost(Int ofs);

	void Push(Int count, bool zero);
	void PushGen(Int count, bool zero, bool noAdjust = false);
	void MulAccum(const RegExpr &nreg, Int scl, bool isTemp = true, bool isPtr = false);
	void AddAccum(Int i, const RegExpr &nreg);
	// local[ofs] += i
	void AddLocal(Int ofs, Int i);

	void AddLocalPtrAccum(const RegExpr &nreg, Int ofs, bool canChain = false);

	void IAdd();
	void ISub();
	void IMul();

	void IDivLike(bool isSigned, bool remainder);

	void IDiv();
	void UIDiv();
	void IMod();
	void UIMod();

	void IAnd();
	void IAnd(Int val);
	void IOr();
	void IOr(Int val);
	void IXor();
	void IXor(Int val);

	void IShlLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &), bool isPtr = false);

	void IShl();
	void IShl(Int val);
	void IShr();
	void IShr(Int val);
	void ISar();
	void ISar(Int val);

#if LETHE_64BIT
	void LAdd();
	void LSub();
	void LMul();
	void LAnd();
	void LOr();
	void LXor();
	void LShl();
	void LShr();
	void LSar();
	void LNeg();
	void LNot();
	void LBsf();
	void LBsr();
	void LPopCnt();
	void LBswap();
	void LCmp(Cond cond);
#endif

	void UBsf();
	void UBsr();
	void UPopCnt();
	void UBswap();

	void FAddLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &));
	void FAddLikeDouble(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &));

	void FAdd();
	void DAdd();

	void FAddTopLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &), Int ofs, const RegExpr &nreg);

	void FAddTop(Int ofs, const RegExpr &nreg);
	void FSub();
	void DSub();
	void FSubTop(Int ofs, const RegExpr &nreg);
	void FMul();
	void DMul();
	void FMulTop(Int ofs, const RegExpr &nreg);
	void FDiv();
	void DDiv();
	void FDivTop(Int ofs, const RegExpr &nreg);

	void FSqrt();
	void DSqrt();

	void ICmpZ();
	void ICmpNZ();
	void FCmpZ();
	void FCmpNZ();
	void DCmpZ();
	void DCmpNZ();

	void ICmpNzBX(bool jmpnz, Int target, bool nocmp = false);
	void FCmpNzBX(bool jmpnz, Int target);
	void DCmpNzBX(bool jmpnz, Int target);

	void ICmp(Cond cond);
	void FCmp(Cond cond);
	void DCmp(Cond cond);

	void IBx(Cond cond, Int target, bool pop);
	void FBxP(Cond cond, Int target);
	void DBxP(Cond cond, Int target);

	void IBx2(Cond cond, Int target, Int iconst, bool isConst);
	void FBx2(Cond cond, Int target);
	void DBx2(Cond cond, Int target);

	void PCx2(Cond cond, bool noadr = false);
	void PCx(Cond cond, bool noadr = false);

	void IAddIConst(Int i, bool isPtr = 0);
	void PtrAddIConst(Int i);

	// add stack-local var to local var, pop
	void AddInt(Int dofs, Int ofs, void (VmJitX86::*opc)(const RegExpr &, const RegExpr &));

	void EmitCall(Int target);
	void EmitFCall();
	void EmitFCallDg();
	void EmitNCall(Int offset, void *nfptr, bool builtin = 0, bool method = false, bool trap = false);
	void EmitVCall(Int idx);

	void IToF();
	void UIToF();
	void FToI();
	void FToUI();
	void FToD();
	void DToF();
	void IToD();
	void UIToD();
	void DToI();
	void DToUI();
	void IToS();
	void IToSB();
	void PToB();

	void FCmpZero();
	void DCmpZero();

	void INeg();
	void INot();
	void FNeg();
	void DNeg();

	void LoadThis(bool imm, Int ofs);
	void PushThis();
	void PopThis();

	void PCopyLocal(Int ofs0, Int ofs1, Int size);
	void PCopyCommon(Int count);
	void PCopy(Int size, bool reverse = false);
	void PSwap(Int size);

	void RangeCheck();
	void RangeCheck(Int val);

	void SwitchTable(const CompiledProgram &prog, Int range, Int pc);

	void PushFuncPtr(Int pc);

	void AddStrong();

	// special nan handling
	void NanAfterSet(Cond cond, const RegExpr &reg);

	// if pc is -1, it's a native call fixup
	// if pc is -2, it's absolute fixup for switch table jump
	void AddFixup(Int adr, Int pc, Byte relative = 1);

	void FlushStackOpt(bool soft = false) override;

	void DoFixups(CompiledProgram &prog, Int pass);

	RegExpr GlobalBase() const;
	RegExpr StackObjectPtr() const;
	// x64 support:
	RegExpr FirstArg() const;

	static const Int REG_CACHE_MAX = 16;

	struct RegCacheEntry
	{
		RegExpr reg;
		// stack offset
		Int offset;
		// MRU counter
		UInt counter;
		// written to?
		bool write;
		// pointer?
		bool pointer;
		// double precision?
		bool doublePrec;
	};

	struct RegCache
	{
		RegCacheEntry cache[REG_CACHE_MAX];
		Int size;
		UInt mru;
		Int reserved;

		Byte index[REG_CACHE_MAX];

		HashMap< Int, Int > trackMap;

		void Init(Int count, RegExpr base);
		void Init(Int count, RegExpr base, Int count2, RegExpr base2);

		bool RegInUse(const RegExpr &nreg) const;

		// returns invalid regexpr if not found
		RegExpr Find(Int offset, bool write);
		// find entry index/-1 if not found
		Int FindEntry(Int offset, bool write);
		// allocate new (might reuse)
		RegExpr Alloc(Int offset, VmJitX86 &jit, Int flags = 0);

		// set write flag for offset
		void SetWrite(Int offset);

		// free register (stk index)
		// returns write status
		bool Free(Int offset, VmJitX86 &jit, bool nospill = 0);
		bool FreeAbove(Int offset, VmJitX86 &jit, bool nospill = 0);

		// flush and free all
		void Flush(VmJitX86 &jit, bool soft = false, Int softOfs = 0);

		bool WouldFlush() const;

		void FlushBelow(Int offset);

		void MoveTracked(RegCache &dst, Int ofs);

		void SwapRegs(const RegExpr &r0, const RegExpr &r1);

		// transfer src to map at ofs
		bool TransferRegWrite(const RegExpr &src, Int ofs, Int ignoreOfs = VmJitX86::INVALID_STACK_INDEX);

		// internal: spill
		void Spill(Int offset, VmJitX86 &jit);
		void SpillAbove(Int offset, VmJitX86 &jit);

		void SpillEntry(Int offset, RegCacheEntry &re, VmJitX86 &jit);

		void MarkAsTemp(const RegExpr &r, VmJitX86 &jit);

		void AdjustTrackmap(Int offset, RegCacheEntry &re, VmJitX86 &jit);
	};

	RegCache gprCache;
	RegCache sseCache;

	// flip sign bit (xor) table:
	Int fxorBase = 0;

	// base offsets for float/double constants
	Int fconstBase = 0;
	Int dconstBase = 0;

	void *codeJITRegistered = nullptr;

	RegExpr FindGpr(Int offset, bool write = 0);
	RegExpr AllocGpr(Int offset, bool load = 0, bool write = 0, bool pointer = 0);
	RegExpr AllocGprPtr(Int offset, bool load = 0, bool write = 0);
	RegExpr AllocGprWrite(Int offset, bool load = 0);
	RegExpr AllocGprWritePtr(Int offset, bool load = 0);
	RegExpr FindSse(Int offset, bool write = 0);
	RegExpr AllocSse(Int offset, bool load = false, bool write = false, bool isDouble = false);
	RegExpr AllocSseWrite(Int offset, bool load = false, bool isDouble = false);
	RegExpr AllocSseDouble(Int offset, bool load = false, bool write = false);
	RegExpr AllocSseWriteDouble(Int offset, bool load = false);

	void TrackGpr(Int targ, Int src);
	void TrackSse(Int targ, Int src);

	void BuildFuncOffsets(const CompiledProgram &prog);

	// flip condition for floating point comparison to avoid checking parity flag for unordered cmp (NaNs)
	static void PrepFloatCond(Cond &cond, RegExpr &r0, RegExpr &r1);

	// New JIT:

	bool CodeGenPass(CompiledProgram &prog, Int pass);

	void UnregisterCode();
};

LETHE_API_END

}
