#include "VmJitX86.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Program/ConstPool.h>
#include "../Builtin.h"

#if LETHE_JIT_X86

namespace lethe
{

#if !LETHE_32BIT
#	if LETHE_COMPILER_MSC_ONLY
extern "C" void VmJitX64_Stub(void *param);
#	endif

#endif

static bool hwPopCnt = false;

static void DetectHwPopCnt()
{
#ifdef _MSC_VER
	int id[4] = {0};
	__cpuid(id, 0);
	int nids = id[0];
	if ( nids >= 2 )
	{
		id[2] = 0;
		__cpuid(id, 1);
		hwPopCnt = (id[2] & 0x800000) != 0;
	}
#elif defined(__GNUC__) && !defined(__ANDROID__)
	int id[4] = {0};
	asm(
		"cpuid":
		"=a" (id[0]),
		"=b" (id[1]),
		"=c" (id[2]),
		"=d" (id[3]) :
		"a" (0)
	);
	int nids = id[0];
	if ( nids >= 2 )
	{
		id[2] = 0;
		asm(
			"cpuid":
			"=a" (id[0]),
			"=b" (id[1]),
			"=c" (id[2]),
			"=d" (id[3]) :
			"a" (1)
		);
		hwPopCnt = (id[2] & 0x800000) != 0;
	}
#endif
}

static bool IsNiceScale(Int scl)
{
	return scl == 1 || scl == 2 || scl == 4 || scl == 8;
}

static void NativeFToUI(Stack &stk)
{
	stk.SetInt(0, WellDefinedFloatToUnsigned<UInt>(stk.GetFloat(0)));
}

void VmJitX86::FToUI()
{
	EmitNCall(-2, reinterpret_cast<void *>(NativeFToUI));
}

static void NativeDToUI(Stack &stk)
{
	auto val = stk.GetDouble(0);
	stk.Pop(Stack::DOUBLE_WORDS);
	stk.PushInt(WellDefinedFloatToUnsigned<UInt>(val));
}

void VmJitX86::DToUI()
{
	EmitNCall(-1, reinterpret_cast<void *>(NativeDToUI), true);
}

// templates

template< bool imm >
RegExpr VmJitX86::GetIndirectAdr(Int &scl, RegExpr &ofs)
{
	DontFlush _(*this);

	if (stackOpt == lastAdr)
	{
		if (imm)
		{
			lastAdr = INVALID_STACK_INDEX;
			return lastAdrExpr;
		}

		// FIXME: should never get there
		FlushLastAdr();
	}

	RegExpr reg = imm ? GetPtr(0) : GetInt(0);

	if (imm)
		return reg;

	if (!IsNiceScale(scl))
	{
		MulAccum(reg, scl, 1, 1);
		scl = 1;
	}

	// mov edx, [edi + 4]
	if (1+stackOpt == lastAdr)
	{
		ofs = lastAdrExpr;
		lastAdr = INVALID_STACK_INDEX;
	}
	else
	{
		ofs = AllocGprPtr(1 + stackOpt, 1).ToReg32();
		FlushLastAdr();
	}

	return reg.ToReg32();
}

template< bool imm >
void VmJitX86::GetIndirectSByte(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocGprWrite(stackOpt+sofs);
	// movsx eax,byte [eax]
	Movsx(dst, imm ? Mem8(reg + scl) : Mem8(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectByte(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocGprWrite(stackOpt+sofs);
	// movzx eax,byte [eax]
	Movzx(dst, imm ? Mem8(reg + scl) : Mem8(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectShort(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocGprWrite(stackOpt+sofs);
	// movsx eax,word [eax]
	Movsx(dst, imm ? Mem16(reg + scl) : Mem16(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectUShort(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocGprWrite(stackOpt+sofs);
	// movzx eax,word [eax]
	Movzx(dst, imm ? Mem16(reg + scl) : Mem16(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectInt(Int scl, Int sofs, bool isPtr)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = isPtr ? AllocGprWritePtr(stackOpt+sofs) : AllocGprWrite(stackOpt+sofs);

	// mov eax,[eax*n+edx]
	Mov(dst, imm ? Mem32(reg + scl) : Mem32(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectFloat(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocSseWrite(stackOpt+sofs);
	// mov eax,[eax*n+edx]
	Movd(dst, imm ? Mem32(reg + scl) : Mem32(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectDouble(Int scl, Int sofs)
{
	RegExpr ofs;
	RegExpr reg = GetIndirectAdr<imm>(scl, ofs);
	DontFlush _(*this);

	RegExpr dst = AllocSseWriteDouble(stackOpt + sofs);
	// mov eax,[eax*n+edx]
	Movq(dst, imm ? Mem32(reg + scl) : Mem32(reg*scl + ofs));
}

template< bool imm >
void VmJitX86::GetIndirectPtr(Int scale, Int sofs)
{
	GetIndirectInt<imm>(scale, sofs, 1);
}

template<int size, bool uns>
void VmJitX86::PInc(Int ofs)
{
	DontFlush _(*this);
	RegExpr reg;

	if (stackOpt == lastAdr)
	{
		reg = lastAdrExpr;
		lastAdr = INVALID_STACK_INDEX;
	}
	else
		reg = GetPtr(0);

	// add [eax], const
	if (size == 1)
		Add(Mem8(reg), ofs);
	else if (size == 2)
		Add(Mem16(reg), ofs);
	else if (size == 4)
		Add(Mem32(reg), ofs);
	else
		Add(Mem64(reg), ofs);

	gprCache.reserved = reg.GetRegMask();
	RegExpr dst = size > 4 ? AllocGprWritePtr(stackOpt) : AllocGprWrite(stackOpt);
	gprCache.reserved = 0;

	// mov eax,[eax]
	if (size == 1)
		uns ? Movzx(dst, Mem8(reg)) : Movsx(dst, Mem8(reg));
	else if (size == 2)
		uns ? Movzx(dst, Mem16(reg)) : Movsx(dst, Mem16(reg));
	else
		Mov(dst, Mem32(reg));
}

template<int size, bool uns>
void VmJitX86::PIncPost(Int ofs)
{
	PInc<size, uns>(ofs);
	DontFlush _(*this);

	RegExpr dst = size > 4 ? AllocGprWritePtr(stackOpt) : AllocGprWrite(stackOpt);

	LETHE_ASSERT(dst.IsRegister());
	Add(dst, -ofs);
}

// edi  = stack ptr
// esi  = Stack object ptr (x86/global data x64; for x64 stack object ptr is r12)
// ebp  = this ptr
// r13  = (x64 only) code base ptr
// r14  = (x64 only) temporary rsp storage

#define VMJITX86_CAN_CHAIN_AADD() \
	bool canChain = canFuseNext; \
	if (canChain) { \
		UInt nextOp = prog.instructions[i+1] & 255u; \
		/* FIXME: better! */ \
		canChain = nextOp >= OPC_PSTORE8_IMM && nextOp <= OPC_PSTOREPTR_IMM_NP; \
	}

void VmJitX86::AlignCode(Int align, bool dumb)
{
	const Int mask = align-1;
	Int calign = (align - (code.GetSize() & mask)) & mask;

	if (dumb)
	{
		for (Int j = 0; j < calign; j++)
			code.Add(0x90);

		return;
	}

	while (calign >= 9)
	{
		code.Add(0x66);
		code.Add(0xf);
		code.Add(0x1f);
		code.Add(0x84);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		calign -= 9;
		continue;
	}

	switch(calign)
	{
	case 2:
		code.Add(0x66);
		// fall through
	case 1:
		code.Add(0x90);
		break;

	case 3:
		code.Add(0x0f);
		code.Add(0x1f);
		code.Add(0);
		break;

	case 4:
		code.Add(0x0f);
		code.Add(0x1f);
		code.Add(0x40);
		code.Add(0);
		break;

	case 6:
		code.Add(0x66);
		// fall through
	case 5:
		code.Add(0x0f);
		code.Add(0x1f);
		code.Add(0x44);
		code.Add(0);
		code.Add(0);
		break;

	case 7:
		code.Add(0x0f);
		code.Add(0x1f);
		code.Add(0x80);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		break;

	case 8:
		code.Add(0x0f);
		code.Add(0x1f);
		code.Add(0x84);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		code.Add(0);
		break;
	}
}

bool VmJitX86::CodeGen(CompiledProgram &prog)
{
#define VMJITX86_OPT_CMP_JMP_FLOAT(cond) VMJITX86_OPT_CMP_JMP_FLOAT_CUSTOM(FCmp, FBx2, cond)
#define VMJITX86_OPT_CMP_JMP_DOUBLE(cond) VMJITX86_OPT_CMP_JMP_FLOAT_CUSTOM(DCmp, DBx2, cond)
#define VMJITX86_OPT_CMP_JMP_FLOAT_CUSTOM(fcmp, fbx2, cond) \
	do { \
		if (canFuseNext && (Byte)prog.instructions[i+1] == OPC_IBNZ_P) \
		{ \
			fbx2(cond, i+2+DecodeImm24(prog.instructions[i+1])); \
			i++; \
		} \
		else \
			fcmp(cond); \
	} while(false)

	DetectHwPopCnt();

	for (Int i=0; i<1; i++)
	{
		LETHE_RET_FALSE(CodeGenPass(prog, i));
/*		File f;
		f.Create(String::Printf("$pass%d.bin", i));
		f.Write(code.GetData(), code.GetSize());*/
	}

	prevPcToCode.Reset();
	jumpSource.Reset();
	prevJumpSource.Reset();

	// finalize funcCodeOfs

	funcCodeToPC.Clear();

	for (Int i=0; i<funcCodeOfs.GetSize(); i++)
		funcCodeToPC[code.GetData() + funcCodeOfs[i]] = funcOfs[i];

	funcCodeOfs.Reset();
	funcOfs.Reset();

	// security: write protect once done; we only JIT once
	code.WriteProtect();

	return true;
}

bool VmJitX86::CodeGenPass(CompiledProgram &prog, Int pass)
{
	prevJumpSource = jumpSource;
	jumpSource.Clear();

	lastIns = -1;
	lastRex = -1;
	stackOpt = 0;
	dontFlush = 0;
	preserveFlags = 0;
	code.Clear();
	fixups.Clear();
	pcToCode.Clear();
	pcToCode.Resize(prog.instructions.GetSize(), -1);

	gprCache = RegCache();
	sseCache = RegCache();

	if (pass == 0)
		BuildFuncOffsets(prog);

	if (IsX64)
		gprCache.Init(4, Eax, 4, R8d);
	else
		gprCache.Init(4, Eax);

	sseCache.Init(8, Xmm0);

	lastAdrExpr = RegExpr();
	lastAdr = INVALID_STACK_INDEX;

	QDataType qdt;

	if (pass == 0)
	{
		prog.cpool.Align(8);

		qdt = QDataType::MakeConstType(prog.elemTypes[DT_FLOAT]);
		uiConvTable = prog.cpool.AllocGlobal(qdt);
		prog.cpool.AllocGlobal(qdt);

		qdt = QDataType::MakeConstType(prog.elemTypes[DT_DOUBLE]);
		prog.cpool.AllocGlobal(qdt);
		prog.cpool.AllocGlobal(qdt);
	}

	if (IsX64)
	{
#if LETHE_OS_WINDOWS
		firstArgReg = Rcx;
#else
		firstArgReg = Rdi;
#endif

		if (pass == 0)
		{
			// initialize native func ptrs
			qdt = QDataType::MakeConstType(prog.elemTypes[DT_FUNC_PTR]);
			auto &cpool = prog.cpool;

			Int ofs = cpool.AllocGlobal(qdt);
			nativeFuncPtr = Esi + ofs;
			auto fptr = NativeFToUI;
			MemCpy(&cpool.data[ofs], &fptr, sizeof(void *));

			ofs = cpool.AllocGlobal(qdt);
			fptr = NativeDToUI;
			MemCpy(&cpool.data[ofs], &fptr, sizeof(void *));

			for (Int i=0; i<cpool.nFunc.GetSize(); i++)
			{
				ofs = cpool.AllocGlobal(qdt);
				MemCpy(&cpool.data[ofs], &cpool.nFunc[i], sizeof(ConstPool::NativeCallback));
			}
		}
	}

	if (pass == 0)
	{
		auto uiConv = reinterpret_cast<Float *>(prog.cpool.data.GetData() + uiConvTable);
		uiConv[0] = 0.0f;
		uiConv[1] = 2147483648.0f*2.0f;

		auto uiConvd = reinterpret_cast<Double *>(prog.cpool.data.GetData() + uiConvTable + 8);
		uiConvd[0] = 0.0;
		uiConvd[1] = 2147483648.0*2.0;

		// build float masks
		prog.cpool.Align(16);
		qdt = QDataType::MakeConstType(prog.elemTypes[DT_FLOAT]);

		fxorBase = prog.cpool.data.GetSize();

		// xor mask (flip sign)
		for (Int i=0; i<4; i++)
		{
			auto ofs = prog.cpool.AllocGlobal(qdt);
			auto xorMask = 0x80000000u;
			MemCpy(&prog.cpool.data[ofs], &xorMask, sizeof(Float));
		}

		// xor mask (flip sign, double)
		for (Int i = 0; i<4; i++)
		{
			auto ofs = prog.cpool.AllocGlobal(qdt);
			auto xorMask = (i & 1) ? 0x80000000u : 0u;
			MemCpy(&prog.cpool.data[ofs], &xorMask, sizeof(Float));
		}

		// bake float constants
		for (Int i=0; i<prog.cpool.fPool.GetSize(); i++)
		{
			auto ofs = prog.cpool.AllocGlobal(qdt);

			if (!i)
				fconstBase = ofs;

			*reinterpret_cast<Float *>(prog.cpool.data.GetData() + ofs) = prog.cpool.fPool[i];
		}

		prog.cpool.Align(8);

		// bake double constants
		qdt = QDataType::MakeConstType(prog.elemTypes[DT_DOUBLE]);

		for (Int i = 0; i<prog.cpool.dPool.GetSize(); i++)
		{
			auto ofs = prog.cpool.AllocGlobal(qdt);

			if (!i)
				dconstBase = ofs;

			*reinterpret_cast<Double *>(prog.cpool.data.GetData() + ofs) = prog.cpool.dPool[i];
		}
	}

	fixups.Clear();

	if (IsX64)
	{
		globalBase = Esi;
		stackObjectPtr = R12d;
	}
	else
	{
		globalBase = (UInt)(UIntPtr)prog.cpool.data.GetData();
		stackObjectPtr = Esi;
	}

	Int nextBarrierIndex = 0;
	Int nextBarrier = prog.barriers[0];

	Int nextLoopIndex = 0;
	Int nextLoop = prog.loops[0];

	Int nextFuncIndex = 0;
	Int nextFunc = funcOfs[0];

	bool lastConst = 0;
	Int lastIntConst = 0;

	funcCodeOfs.Clear();

	for (Int i=0; i<prog.instructions.GetSize(); i++)
	{
		const ConstPool &cpool = prog.cpool;
		Int ins = prog.instructions[i];

		/*printf("pc=%08x, gpr:\n", i);
		gprCache.Dump();
		printf("sse:\n");
		sseCache.Dump();*/

		if (i == nextFunc)
		{
			nextFunc = funcOfs[++nextFuncIndex];
			// note: must align functions to at least 4 bytes because of fcall_dg!!!
			AlignCode(16, true);
			funcCodeOfs.Add(code.GetSize());
		}

		if (i == nextBarrier)
		{
			lastIns = -1;
			lastConst = 0;
			FlushStackOpt();
			nextBarrier = prog.barriers[++nextBarrierIndex];

			//AlignCode(16);
		}

		if (i == nextLoop)
		{
			nextLoop = prog.loops[++nextLoopIndex];
			// unfortunately we can't align loop starts due to the way we "optimize" short jumps
			// hmm, loop alignment seems to actually hurt a bit on Ryzen in some cases => removed
			// but: gcc seems to align to 8 bytes, clang does unroll + 16-byte align; I guess it depends on how long the loop actually is?
			//AlignCode(32);
		}

		pcToCode[i] = code.GetSize();

		RegExpr reg;

		const bool canFuseNext = nextBarrier != i+1 && i+1 < prog.instructions.GetSize();

		switch((Byte)ins)
		{
		case OPC_PUSH_ICONST:
			lastIntConst = DecodeImm24(ins);
			PushInt(lastIntConst);
			lastConst = 1;
			continue;

		case OPC_PUSHC_ICONST:
			lastIntConst = cpool.iPool[DecodeUImm24(ins)];
			PushInt(lastIntConst);
			lastConst = 1;
			continue;

		case OPC_PUSH_FCONST:
			PushFloat(prog, cpool.fPool[cpool.fPoolMap[(Float)DecodeImm24(ins)]]);
			break;

		case OPC_PUSHC_FCONST:
			PushFloat(prog, cpool.fPool[DecodeUImm24(ins)]);
			break;

		case OPC_PUSH_DCONST:
			PushDouble(prog, cpool.dPool[cpool.dPoolMap[(Double)DecodeImm24(ins)]]);
			break;

		case OPC_PUSHC_DCONST:
			PushDouble(prog, cpool.dPool[DecodeUImm24(ins)]);
			break;

		case OPC_LPUSH32:
		{
			Int tofs = DecodeUImm24(ins);
			reg = GetInt(tofs);
			Push(1, 0);
			TrackGpr(stackOpt, stackOpt + 1 + tofs);
		}
		break;

		case OPC_LPUSH32F:
		{
			Int tofs = DecodeUImm24(ins);
			reg = GetFloat(tofs);
			Push(1, 0);
			TrackSse(stackOpt, stackOpt + 1 + tofs);
		}
		break;

		case OPC_LPUSH64D:
		{
			Int tofs = DecodeUImm24(ins);
			reg = GetDouble(tofs);
			Push(Stack::DOUBLE_WORDS, 0);
			TrackSse(stackOpt, stackOpt + Stack::DOUBLE_WORDS + tofs);
		}
		break;

		case OPC_LPUSHADR:
		{
			// note: flushing here seems necessary (related to delegates) if we do pushthis/lpushadr 0
			FlushLastAdr();
			Int tofs = DecodeUImm24(ins);
			bool doSpill = true;

			if (canFuseNext)
			{
				const auto nins = prog.instructions[i+1];
				const auto nopc = nins & 255u;

				if ((nopc >= OPC_PLOAD8 && nopc < OPC_PSTORE8_IMM) || nins == OPC_PLOAD64)
					doSpill = false;
			}

			// TODO: verify that spill really works
			// because of aliasing, we have to spill everything above tofs!
			// but this is stupid as it does unnecessary spills
			if (doSpill)
			{
				gprCache.SpillAbove(stackOpt + tofs, *this);
				sseCache.SpillAbove(stackOpt + tofs, *this);
			}
			else
			{
				gprCache.Spill(stackOpt + tofs, *this);
				sseCache.Spill(stackOpt + tofs, *this);
			}

			/*reg = GetStackAdr( DecodeUImm24(ins) );
			PushPtrAccum(reg);*/
			Push(1, 0);

			SetLastAdr(Edi + (stackOpt + 1 + tofs)*Stack::WORD_SIZE);
		}
		break;

		case OPC_LPUSHPTR:
			if (canFuseNext)
			{
				auto nextOpc = prog.instructions[i+1] & 255;
				if (nextOpc >= OPC_PLOAD8_IMM && nextOpc <= OPC_PSTOREPTR_IMM_NP/*OPC_PLOADPTR_IMM*/)
				{
					// FIXME!!! crashes... something's wrong... => yes, if we lose the cached register! [BUT that didn't happen actually!]
					Int tofs = DecodeUImm24(ins);
					reg = GetPtr(tofs);
					Push(1, 0);
					TrackGpr(stackOpt, stackOpt + 1 + tofs);
					SetLastAdr(reg);
					break;
				}
			}
			reg = GetPtr(DecodeUImm24(ins));
			PushPtrAccum(reg);
			break;

		case OPC_LPUSH32_ICONST:
		{
			Int tofs = DecodeUImm8(ins, 0);
			reg = GetInt(tofs);
			Push(1, 0);
			TrackGpr(stackOpt, stackOpt + 1 + tofs);
			////PushIntAccum(reg);
			lastIntConst = DecodeImm16Top(ins);
			PushInt(lastIntConst);
			lastConst = true;
		}
		break;

		case OPC_LPUSH32_CICONST:
		{
			Int tofs = DecodeUImm8(ins, 0);
			reg = GetInt(tofs);
			Push(1, 0);
			TrackGpr(stackOpt, stackOpt + 1 + tofs);
			////PushIntAccum(reg);
			lastIntConst = Int(cpool.iPool[DecodeUImm16Top(ins)]);
			PushInt(lastIntConst);
			lastConst = true;
		}
		break;

		case OPC_GLOAD32:
			reg = GetGlobalInt(DecodeUImm24(ins));
			////PushIntAccum(reg);
			break;

		case OPC_GLOAD32F:
			reg = GetGlobalFloat(DecodeUImm24(ins));
			////PushIntAccum(reg);
			break;

		case OPC_GLOAD64D:
			reg = GetGlobalDouble(DecodeUImm24(ins));
			////PushIntAccum(reg);
			break;

		case OPC_GLOADPTR:
			reg = GetGlobalPtr(DecodeUImm24(ins));
			////PushPtrAccum(reg);
			break;

		case OPC_GSTORE32:
			reg = GetInt(0);
			SetGlobalInt(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_GSTORE32F:
			reg = GetFloat(0);
			SetGlobalFloat(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_GSTORE64D:
			reg = GetDouble(0);
			SetGlobalDouble(DecodeUImm24(ins), reg);
			Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_GSTORE32_NP:
			reg = GetInt(0);
			SetGlobalInt(DecodeUImm24(ins), reg);
			break;

		case OPC_GSTORE32F_NP:
			reg = GetFloat(0);
			SetGlobalFloat(DecodeUImm24(ins), reg);
			break;

		case OPC_GSTORE64D_NP:
			reg = GetDouble(0);
			SetGlobalDouble(DecodeUImm24(ins), reg);
			break;

		case OPC_GSTOREPTR:
			reg = GetPtr(0);
			SetGlobalPtr(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_GSTOREPTR_NP:
			reg = GetPtr(0);
			SetGlobalPtr(DecodeUImm24(ins), reg);
			break;

		case OPC_GLOAD8:
			reg = GetGlobalSByte(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_GLOAD8U:
			reg = GetGlobalByte(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_GLOAD16:
			reg = GetGlobalShort(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_GLOAD16U:
			reg = GetGlobalUShort(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_GSTORE8:
			reg = GetInt(0);
			SetGlobalByte(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_GSTORE8_NP:
			reg = GetInt(0);
			SetGlobalByte(DecodeUImm24(ins), reg);
			break;

		case OPC_GSTORE16:
			reg = GetInt(0);
			SetGlobalUShort(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_GSTORE16_NP:
			reg = GetInt(0);
			SetGlobalUShort(DecodeUImm24(ins), reg);
			break;

		case OPC_GLOADADR:
			Push(1, 0);

			SetLastAdr(GlobalBase() + DecodeUImm24(ins));
			//reg = GetGlobalAdr( DecodeUImm24(ins) );
			break;

		case OPC_PUSH_FUNC:
			PushFuncPtr(i + 1 + DecodeImm24(ins));
			break;

		case OPC_POP:
			Pop(DecodeUImm24(ins));
			break;

		case OPC_LPUSH8:
			reg = GetLocalSByte(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_LPUSH8U:
			reg = GetLocalByte(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_LPUSH16:
			reg = GetLocalShort(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_LPUSH16U:
			reg = GetLocalUShort(DecodeUImm24(ins));
			PushIntAccum(reg);
			break;

		case OPC_LSTORE8:
			reg = GetInt(0);
			SetLocalByte(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_LSTORE8_NP:
			reg = GetInt(0);
			SetLocalByte(DecodeUImm24(ins), reg);
			break;

		case OPC_LSTORE16:
			reg = GetInt(0);
			SetLocalUShort(DecodeUImm24(ins), reg);
			Pop(1);
			break;

		case OPC_LSTORE16_NP:
			reg = GetInt(0);
			SetLocalUShort(DecodeUImm24(ins), reg);
			break;

		case OPC_LSTORE32:
			reg = GetInt(0);
			SetLocalInt(DecodeUImm24(ins), reg, 1);
			Pop(1);
			break;

		case OPC_LSTORE32F:
			reg = GetFloat(0);
			SetLocalFloat(DecodeUImm24(ins), reg, 1);
			Pop(1);
			break;

		case OPC_LSTORE64D:
			reg = GetDouble(0);
			SetLocalDouble(DecodeUImm24(ins), reg, 1);
			Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_LSTORE32_NP:
			reg = GetInt(0);
			SetLocalInt(DecodeUImm24(ins), reg);
			break;

		case OPC_LSTORE32F_NP:
			reg = GetFloat(0);
			SetLocalFloat(DecodeUImm24(ins), reg);
			break;

		case OPC_LSTORE64D_NP:
			reg = GetDouble(0);
			SetLocalDouble(DecodeUImm24(ins), reg);
			break;

		case OPC_LSTOREPTR:
			reg = GetPtr(0);
			SetLocalPtr(DecodeUImm24(ins), reg, 1);
			Pop(1);
			break;

		case OPC_LSTOREPTR_NP:
			reg = GetPtr(0);
			SetLocalPtr(DecodeUImm24(ins), reg);
			break;

		case OPC_LMOVE32:
			reg = GetInt(DecodeUImm8(ins, 1));
			SetLocalInt(DecodeUImm8(ins, 0), reg);
			break;

		case OPC_LMOVEPTR:
			reg = GetPtr(DecodeUImm8(ins, 1));
			SetLocalPtr(DecodeUImm8(ins, 0), reg);
			break;

		case OPC_LSWAPPTR:
		{
			reg = GetPtr(0);
			PushPtrAccum(reg);
			RegExpr reg1 = GetPtr(2);
			SetLocalPtr(1, reg1);
			SetLocalPtr(2, GetPtr(0));
			Pop(1);
		}
		break;

		case OPC_RANGE_ICONST:
			GetInt(0);
			RangeCheck(DecodeUImm24(ins));
			break;

		case OPC_RANGE_CICONST:
			GetInt(0);
			RangeCheck(cpool.iPool[DecodeUImm24(ins)]);
			break;

		case OPC_RANGE:
			GetInt(0);
			RangeCheck();
			break;

		case OPC_PLOAD8:
			GetIndirectSByte<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD8U:
			GetIndirectByte<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD16:
			GetIndirectShort<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD16U:
			GetIndirectUShort<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD32:
			GetIndirectInt<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD32F:
			GetIndirectFloat<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD64D:
		{
			auto sofs = 2-Stack::DOUBLE_WORDS;
			GetIndirectDouble<0>(DecodeUImm24(ins), sofs);
			gprCache.Free(stackOpt+1, *this, true);
			gprCache.Free(stackOpt, *this, true);
			Pop(sofs);
			break;
		}

		case OPC_PLOADPTR:
			GetIndirectPtr<0>(DecodeUImm24(ins), 1);
			Pop(1);
			break;

		case OPC_PLOAD8_IMM:
			GetIndirectSByte<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD8U_IMM:
			GetIndirectByte<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD16_IMM:
			GetIndirectShort<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD16U_IMM:
			GetIndirectUShort<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD32_IMM:
			GetIndirectInt<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD32F_IMM:
			GetIndirectFloat<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PLOAD64D_IMM:
		{
			Int sofs = 1 - Stack::DOUBLE_WORDS;
			GetIndirectDouble<1>(DecodeImm24(ins), sofs);
			gprCache.Free(stackOpt, *this, true);

			if (sofs < 0)
				Push(-sofs, false);
			break;
		}

		case OPC_PLOADPTR_IMM:
			GetIndirectPtr<1>(DecodeImm24(ins), 0);
			break;

		case OPC_PSTORE8_IMM:
			StoreIndirectImmByte(DecodeImm24(ins));
			Pop(2);
			break;

		case OPC_PSTORE8_IMM_NP:
			StoreIndirectImmByte(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PSTORE16_IMM:
			StoreIndirectImmUShort(DecodeImm24(ins));
			Pop(2);
			break;

		case OPC_PSTORE16_IMM_NP:
			StoreIndirectImmUShort(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PSTORE32_IMM:
			StoreIndirectImmInt(DecodeImm24(ins));
			Pop(2);
			break;

		case OPC_PSTORE32_IMM_NP:
			StoreIndirectImmInt(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PSTORE32F_IMM:
			StoreIndirectImmFloat(DecodeImm24(ins));
			Pop(2);
			break;

		case OPC_PSTORE32F_IMM_NP:
			StoreIndirectImmFloat(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PSTORE64D_IMM:
			StoreIndirectImmDouble(DecodeImm24(ins));
			Pop(1 + Stack::DOUBLE_WORDS);
			break;

		case OPC_PSTORE64D_IMM_NP:
			StoreIndirectImmDouble(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PSTOREPTR_IMM:
			StoreIndirectImmPtr(DecodeImm24(ins));
			Pop(2);
			break;

		case OPC_PSTOREPTR_IMM_NP:
			StoreIndirectImmPtr(DecodeImm24(ins));
			Pop(1);
			break;

		case OPC_PINC8:
			PInc<1, 0>(DecodeImm24(ins));
			break;

		case OPC_PINC8U:
			PInc<1, 1>(DecodeImm24(ins));
			break;

		case OPC_PINC16:
			PInc<2, 0>(DecodeImm24(ins));
			break;

		case OPC_PINC16U:
			PInc<2, 1>(DecodeImm24(ins));
			break;

		case OPC_PINC32:
			PInc<4, 0>(DecodeImm24(ins));
			break;

		case OPC_PINC8_POST:
			PIncPost<1, 0>(DecodeImm24(ins));
			break;

		case OPC_PINC8U_POST:
			PIncPost<1, 1>(DecodeImm24(ins));
			break;

		case OPC_PINC16_POST:
			PIncPost<2, 0>(DecodeImm24(ins));
			break;

		case OPC_PINC16U_POST:
			PIncPost<2, 1>(DecodeImm24(ins));
			break;

		case OPC_PINC32_POST:
			PIncPost<4, 0>(DecodeImm24(ins));
			break;

		case OPC_PCOPY:
			PCopy(DecodeUImm24(ins));
			Pop(2);
			break;

		case OPC_PCOPY_REV:
			PCopy(DecodeUImm24(ins), 1);
			Pop(2);
			break;

		case OPC_PCOPY_NP:
			PCopy(DecodeUImm24(ins));
			Pop(1);
			break;

		case OPC_PSWAP:
			PSwap(DecodeUImm24(ins));
			Pop(2);
			break;

		case OPC_PUSH_RAW:
			Push(DecodeUImm24(ins), 0);
			break;

		case OPC_PUSHZ_RAW:
			Push(DecodeUImm24(ins), 1);
			break;

		case OPC_AADD:
		{
			VMJITX86_CAN_CHAIN_AADD();
			reg = GetIntSignExtend(0);

			Int scl = DecodeUImm24(ins);

			if (!IsNiceScale(scl))
				MulAccum(reg, scl);
			else
				reg = reg * scl;

			AddLocalPtrAccum(reg, 1, canChain);
		}

		Pop(1);
		break;

		case OPC_LAADD:
		{
			VMJITX86_CAN_CHAIN_AADD();
			Int ofs = DecodeUImm16Top(ins);
			reg = GetIntSignExtend(ofs);

			Int scl = DecodeUImm8(ins, 0);

			if (!IsNiceScale(scl))
				MulAccum(reg, scl);
			else
				reg = reg * scl;

			AddLocalPtrAccum(reg, 0, canChain);
		}
		break;

		case OPC_AADD_ICONST:
			PtrAddIConst(DecodeImm24(ins));
			break;

		case OPC_AADDH_ICONST:
			PtrAddIConst((UInt)DecodeImm24(ins) << 16);
			break;

		case OPC_IADD:
			IAdd();
			break;

		case OPC_ISUB:
			ISub();
			break;

		case OPC_IMUL:
			IMul();
			break;

		case OPC_IMUL_ICONST:
			reg = AllocGprWrite(stackOpt, 1);
			MulAccum(reg, DecodeImm24(ins), 0);
			break;

		case OPC_IDIV:
			IDiv();
			break;

		case OPC_UIDIV:
			UIDiv();
			break;

		case OPC_IMOD:
			IMod();
			break;

		case OPC_UIMOD:
			UIMod();
			break;

		case OPC_IAND:
			IAnd();
			break;

		case OPC_IAND_ICONST:
			IAnd(DecodeImm24(ins));
			break;

		case OPC_IOR:
			IOr();
			break;

		case OPC_IOR_ICONST:
			IOr(DecodeImm24(ins));
			break;

		case OPC_IXOR:
			IXor();
			break;

		case OPC_IXOR_ICONST:
			IXor(DecodeImm24(ins));
			break;

		case OPC_ISHL:
			IShl();
			break;

		case OPC_ISHL_ICONST:
			IShl(DecodeImm24(ins));
			break;

		case OPC_ISHR:
			IShr();
			break;

		case OPC_ISHR_ICONST:
			IShr(DecodeImm24(ins));
			break;

		case OPC_ISAR:
			ISar();
			break;

		case OPC_ISAR_ICONST:
			ISar(DecodeImm24(ins));
			break;

		case OPC_FADD:
			FAdd();
			break;

		case OPC_DADD:
			DAdd();
			break;

		case OPC_FADD_ICONST:
			PushFloat(prog, (Float)DecodeImm24(ins));
			FAdd();
			break;

		case OPC_LFADD_ICONST:
			PushFloat(prog, (Float)DecodeImm8Top(ins));
			reg = GetFloat(DecodeUImm8(ins, 1)+1);
			FAddTop(DecodeUImm8(ins, 0)+1, reg);
			Pop(1);
			break;

		case OPC_LFADD:
			reg = GetFloat(DecodeUImm8(ins, 1));
			FAddTop(DecodeUImm8(ins, 0), reg);
			Pop(1);
			break;

		case OPC_FSUB:
			FSub();
			break;

		case OPC_DSUB:
			DSub();
			break;

		case OPC_LFSUB:
			reg = GetFloat(DecodeUImm8(ins, 1));
			FSubTop(DecodeUImm8(ins, 0), reg);
			Pop(1);
			break;

		case OPC_FMUL:
			FMul();
			break;

		case OPC_DMUL:
			DMul();
			break;

		case OPC_LFMUL:
			reg = GetFloat(DecodeUImm8(ins, 1));
			FMulTop(DecodeUImm8(ins, 0), reg);
			Pop(1);
			break;

		case OPC_FDIV:
			FDiv();
			break;

		case OPC_DDIV:
			DDiv();
			break;

		case OPC_LFDIV:
			reg = GetFloat(DecodeUImm8(ins, 1));
			FDivTop(DecodeUImm8(ins, 0), reg);
			Pop(1);
			break;

		case OPC_ICMPZ:
			ICmpZ();
			break;

		case OPC_ICMPNZ:
			ICmpNZ();
			break;

		case OPC_FCMPZ:
			FCmpZ();
			break;

		case OPC_FCMPNZ:
			FCmpNZ();
			break;

		case OPC_DCMPZ:
			DCmpZ();
			break;

		case OPC_DCMPNZ:
			DCmpNZ();
			break;

		case OPC_ICMPNZ_BZ:
			ICmpNzBX(0, i+1+DecodeImm24(ins));
			break;

		case OPC_ICMPNZ_BNZ:
			ICmpNzBX(1, i+1+DecodeImm24(ins));
			break;

		case OPC_FCMPNZ_BZ:
			FCmpNzBX(0, i+1+DecodeImm24(ins));
			break;

		case OPC_FCMPNZ_BNZ:
			FCmpNzBX(1, i+1+DecodeImm24(ins));
			break;

		case OPC_DCMPNZ_BZ:
			DCmpNzBX(false, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DCMPNZ_BNZ:
			DCmpNzBX(true, i + 1 + DecodeImm24(ins));
			break;

		case OPC_ICMPEQ:
			ICmp(COND_Z);
			break;

		case OPC_ICMPNE:
			ICmp(COND_NZ);
			break;

		case OPC_ICMPLT:
			ICmp(COND_LT);
			break;

		case OPC_ICMPLE:
			ICmp(COND_LE);
			break;

		case OPC_ICMPGT:
			ICmp(COND_GT);
			break;

		case OPC_ICMPGE:
			ICmp(COND_GE);
			break;

		case OPC_UICMPLT:
			ICmp(COND_ULT);
			break;

		case OPC_UICMPLE:
			ICmp(COND_ULE);
			break;

		case OPC_UICMPGT:
			ICmp(COND_UGT);
			break;

		case OPC_UICMPGE:
			ICmp(COND_UGE);
			break;

		case OPC_FCMPEQ:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_Z);
			break;

		case OPC_FCMPNE:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_NZ);
			break;

		case OPC_FCMPLT:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_ULT);
			break;

		case OPC_FCMPLE:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_ULE);
			break;

		case OPC_FCMPGT:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_UGT);
			break;

		case OPC_FCMPGE:
			VMJITX86_OPT_CMP_JMP_FLOAT(COND_UGE);
			break;

		case OPC_DCMPEQ:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_Z);
			break;

		case OPC_DCMPNE:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_NZ);
			break;

		case OPC_DCMPLT:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_ULT);
			break;

		case OPC_DCMPLE:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_ULE);
			break;

		case OPC_DCMPGT:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_UGT);
			break;

		case OPC_DCMPGE:
			VMJITX86_OPT_CMP_JMP_DOUBLE(COND_UGE);
			break;

		case OPC_BR:
			EmitJump(COND_ALWAYS, i+1+DecodeImm24(ins));
			break;

		case OPC_IBZ_P:
			IBx(COND_Z, i+1+DecodeImm24(ins), 1);
			break;

		case OPC_IBNZ_P:
			IBx(COND_NZ, i+1+DecodeImm24(ins), 1);
			break;

		case OPC_IBZ:
			ICmpNzBX(false, i + 1 + DecodeImm24(ins), true);
			break;

		case OPC_IBNZ:
			ICmpNzBX(true, i + 1 + DecodeImm24(ins), true);
			break;

		case OPC_FBZ_P:
			FBxP(COND_Z, i+1+DecodeImm24(ins));
			break;

		case OPC_FBNZ_P:
			FBxP(COND_NZ, i+1+DecodeImm24(ins));
			break;

		case OPC_DBZ_P:
			DBxP(COND_Z, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBNZ_P:
			DBxP(COND_NZ, i + 1 + DecodeImm24(ins));
			break;

		case OPC_IBEQ:
			IBx2(COND_Z, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_IBNE:
			IBx2(COND_NZ, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_IBLT:
			IBx2(COND_LT, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_IBLE:
			IBx2(COND_LE, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_IBGT:
			IBx2(COND_GT, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_IBGE:
			IBx2(COND_GE, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_UIBLT:
			IBx2(COND_ULT, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_UIBLE:
			IBx2(COND_ULE, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_UIBGT:
			IBx2(COND_UGT, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_UIBGE:
			IBx2(COND_UGE, i+1+DecodeImm24(ins), lastIntConst, lastConst);
			break;

		case OPC_FBEQ:
			FBx2(COND_Z, i+1+DecodeImm24(ins));
			break;

		case OPC_FBNE:
			FBx2(COND_NZ, i+1+DecodeImm24(ins));
			break;

		case OPC_FBLT:
			FBx2(COND_ULT, i+1+DecodeImm24(ins));
			break;

		case OPC_FBLE:
			FBx2(COND_ULE, i+1+DecodeImm24(ins));
			break;

		case OPC_FBGT:
			FBx2(COND_UGT, i+1+DecodeImm24(ins));
			break;

		case OPC_FBGE:
			FBx2(COND_UGE, i+1+DecodeImm24(ins));
			break;

		case OPC_DBEQ:
			DBx2(COND_Z, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBNE:
			DBx2(COND_NZ, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBLT:
			DBx2(COND_ULT, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBLE:
			DBx2(COND_ULE, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBGT:
			DBx2(COND_UGT, i + 1 + DecodeImm24(ins));
			break;

		case OPC_DBGE:
			DBx2(COND_UGE, i + 1 + DecodeImm24(ins));
			break;

		case OPC_PCMPEQ:
			PCx2(COND_Z);
			break;

		case OPC_PCMPNE:
			PCx2(COND_NZ);
			break;

		case OPC_PCMPZ:
			PCx(COND_Z);
			break;

		case OPC_PCMPNZ:
			PCx(COND_NZ);
			break;

		case OPC_IADD_ICONST:
			IAddIConst(DecodeImm24(ins));
			break;

		case OPC_LIADD_ICONST:
		{
			Int src = DecodeUImm8(ins, 1);
			Int dst = DecodeUImm8(ins, 0);

			if (src == dst)
			{
				// optimize special case...
				if (src == 0)
					IAddIConst(DecodeImm8Top(ins));
				else
					AddLocal(src, DecodeImm8Top(ins));
			}
			else
			{
				RegExpr dreg = AllocGprWrite(stackOpt + dst);
				reg = AllocGpr(stackOpt + src, 1);
				{
					DontFlush _(*this);
					Mov(dreg, reg);
				}
				AddAccum(DecodeImm8Top(ins), dreg);
			}
		}
		break;

		case OPC_LIADD:
			AddInt(DecodeUImm8(ins, 0), DecodeUImm8(ins, 1), &VmJitX86::Add);
			break;

		case OPC_LISUB:
			AddInt(DecodeUImm8(ins, 0), DecodeUImm8(ins, 1), &VmJitX86::Sub);
			break;

		case OPC_CALL:
			EmitCall(i+1+DecodeImm24(ins));
			break;

		case OPC_FCALL:
			EmitFCall();
			break;

		case OPC_FCALL_DG:
			EmitFCallDg();
			break;

		case OPC_NCALL:
		case OPC_NMCALL:
		{
			Int nofs = DecodeUImm24(ins);
			EmitNCall(nofs, reinterpret_cast<void *>(cpool.nFunc[nofs]), false, (Byte)ins == OPC_NMCALL);
		}
		break;

		case OPC_BCALL:
		case OPC_BCALL_TRAP:
		case OPC_BMCALL:
		{
			Int nofs = DecodeUImm24(ins);

			if (nofs == BUILTIN_ADD_STRONG)
			{
				// we can do better here!
				AddStrong();
				break;
			}

#if LETHE_64BIT
			do {
#define VMJITX86_POPCONST \
					LETHE_ASSERT(lastConst); \
					code.Resize(lastIns); \
					pcToCode.Back() = pcToCode[pcToCode.GetSize()-2]; \
					Pop(1);

				// time to inline 64-bit builtins!
				switch(nofs)
				{
				case BUILTIN_PUSH_LCONST:
					{
						VMJITX86_POPCONST

						PushPtrAccum(lastIntConst);
						nofs = -1;
						break;
					}
				case BUILTIN_PUSHC_LCONST:
					{
						VMJITX86_POPCONST

						PushPtrAccum(cpool.lPool[lastIntConst]);
						nofs = -1;
						break;
					}
				case BUILTIN_LPUSH64:
					{
						VMJITX86_POPCONST

						reg = GetPtrNoAdr(lastIntConst);
						Push(1, 0);
						TrackGpr(stackOpt, stackOpt + 1 + lastIntConst);
						nofs = -1;
						break;
					}
				case BUILTIN_LSTORE64:
					{
						VMJITX86_POPCONST

						reg = GetPtrNoAdr(0);
						SetLocalPtr(lastIntConst, reg, 1);
						Pop(1);
						nofs = -1;
						break;
					}
				case BUILTIN_LSTORE64_NP:
					{
						VMJITX86_POPCONST

						reg = GetPtrNoAdr(0);
						SetLocalPtr(lastIntConst, reg, 1);
						nofs = -1;
						break;
					}
				case BUILTIN_LMUL:
					{
						LMul();
						nofs = -1;
						break;
					}
				case BUILTIN_LADD:
					{
						LAdd();
						nofs = -1;
						break;
					}
				case BUILTIN_LSUB:
					{
						LSub();
						nofs = -1;
						break;
					}
				case BUILTIN_LAND:
					{
						LAnd();
						nofs = -1;
						break;
					}
				case BUILTIN_LOR:
					{
						LOr();
						nofs = -1;
						break;
					}
				case BUILTIN_LXOR:
					{
						LXor();
						nofs = -1;
						break;
					}
				case BUILTIN_LSHL:
					{
						LShl();
						nofs = -1;
						break;
					}
				case BUILTIN_LSHR:
					{
						LShr();
						nofs = -1;
						break;
					}
				case BUILTIN_LSAR:
					{
						LSar();
						nofs = -1;
						break;
					}
				case BUILTIN_CONV_LTOI:
					{
						SetLocalInt(0, GetInt(0));
						nofs = -1;
						break;
					}
				case BUILTIN_CONV_ITOL:
					{
						DontFlush _(*this);
						reg = GetPtrNoAdr(0);
						SetLocalPtr(0, reg, 1);
						reg = GetPtrNoAdr(0);
						Movsxd(reg.ToRegPtr(), reg.ToReg32());
						nofs = -1;
						break;
					}
				case BUILTIN_CONV_UITOL:
					// note: this can't be a nop because sometimes we sign-extend regs in 64-bit mode
					// (index address)
					{
						DontFlush _(*this);
						reg = GetPtrNoAdr(0);
						SetLocalPtr(0, reg, 1);
						reg = GetPtrNoAdr(0);
						Mov(reg.ToReg32(), reg.ToReg32());
						nofs = -1;
						break;
					}
				case BUILTIN_PLOAD64:
					{
						VMJITX86_POPCONST

						GetIndirectPtr<0>(lastIntConst, 1);
						Pop(1);

						nofs = -1;
						break;
					}
				case BUILTIN_PSTORE64_IMM0:
				case BUILTIN_PSTORE64_IMM0_NP:
					{
						// note: offset is zero!
						StoreIndirectImmPtr(0);
						Pop(2 - (nofs == BUILTIN_PSTORE64_IMM0_NP));
						nofs = -1;
						break;
					}
				case BUILTIN_LNEG:
					{
						LNeg();
						nofs = -1;
						break;
					}
				case BUILTIN_LNOT:
					{
						LNot();
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPNZ:
					{
						PCx(COND_NZ, true);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPZ:
					{
						PCx(COND_Z, true);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPEQ:
					{
						PCx2(COND_Z, true);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPNE:
					{
						PCx2(COND_NZ, true);
						nofs = -1;
						break;
					}
				case BUILTIN_INTRIN_BSFL:
					{
						LBsf();
						nofs = -1;
						break;
					}
				case BUILTIN_INTRIN_BSRL:
					{
						LBsr();
						nofs = -1;
						break;
					}
				case BUILTIN_INTRIN_POPCNTL:
					if (hwPopCnt)
					{
						LPopCnt();
						nofs = -1;
					}
					break;
				case BUILTIN_INTRIN_BSWAPL:
					{
						LBswap();
						nofs = -1;
						break;
					}
				case BUILTIN_GLOAD64:
					{
						VMJITX86_POPCONST

						reg = GetGlobalPtr(lastIntConst);
						nofs = -1;
						break;
					}
				case BUILTIN_GSTORE64:
					{
						VMJITX86_POPCONST

						reg = GetPtrNoAdr(0);
						SetGlobalPtr(lastIntConst, reg);
						Pop(1);
						nofs = -1;
						break;
					}
				case BUILTIN_PINC64:
					{
						VMJITX86_POPCONST
						PInc<8, 0>(lastIntConst);
						nofs = -1;
						break;
					}
				case BUILTIN_PINC64_POST:
					{
						VMJITX86_POPCONST

						PIncPost<8, 0>(lastIntConst);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPLT:
					{
						LCmp(COND_LT);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPLE:
					{
						LCmp(COND_LE);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPGT:
					{
						LCmp(COND_GT);
						nofs = -1;
						break;
					}
				case BUILTIN_LCMPGE:
					{
						LCmp(COND_GE);
						nofs = -1;
						break;
					}
				case BUILTIN_ULCMPLT:
					{
						LCmp(COND_ULT);
						nofs = -1;
						break;
					}
				case BUILTIN_ULCMPLE:
					{
						LCmp(COND_ULE);
						nofs = -1;
						break;
					}
				case BUILTIN_ULCMPGT:
					{
						LCmp(COND_UGT);
						nofs = -1;
						break;
					}
				case BUILTIN_ULCMPGE:
					{
						LCmp(COND_UGE);
						nofs = -1;
						break;
					}
				}
#undef VMJITX86_POPCONST
			} while(false);
#endif

			switch(nofs)
			{
			case BUILTIN_INTRIN_BSF:
				{
					UBsf();
					nofs = -1;
					break;
				}
			case BUILTIN_INTRIN_BSR:
				{
					UBsr();
					nofs = -1;
					break;
				}
			case BUILTIN_INTRIN_POPCNT:
				if (hwPopCnt)
				{
					UPopCnt();
					nofs = -1;
				}
				break;
			case BUILTIN_INTRIN_BSWAP:
				{
					UBswap();
					nofs = -1;
					break;
				}
			default:;
			}

			if (nofs != -1)
				EmitNCall(nofs, reinterpret_cast<void *>(cpool.nFunc[nofs]), true, (Byte)ins == OPC_BMCALL, (Byte)ins == OPC_BCALL_TRAP);
		}
		break;

		case OPC_VCALL:
			EmitVCall(DecodeImm24(ins));
			break;

		case OPC_RET:
			if (fastCall)
			{
				Pop(DecodeUImm24(ins));
				Retn();
			}
			else
			{
				Pop(DecodeUImm24(ins) + 1);
				// jmp dword [edi-4]
				EmitNew(0xff);
				Emit(0x67);
				Emit(0xfc);
			}

			break;

		case OPC_LOADTHIS:
			LoadThis(0, 0);
			break;

		case OPC_LOADTHIS_IMM:
			LoadThis(1, DecodeImm24(ins));
			break;

		case OPC_PUSHTHIS:
		case OPC_PUSHTHIS_TEMP:
			PushThis();
			break;

		case OPC_POPTHIS:
			PopThis();
			break;

		case OPC_CONV_ITOF:
			IToF();
			break;

		case OPC_CONV_UITOF:
			UIToF();
			break;

		case OPC_CONV_FTOI:
			FToI();
			break;

		case OPC_CONV_FTOUI:
			FToUI();
			break;

		case OPC_CONV_FTOD:
			FToD();
			break;

		case OPC_CONV_DTOF:
			DToF();
			break;

		case OPC_CONV_ITOD:
			IToD();
			break;

		case OPC_CONV_UITOD:
			UIToD();
			break;

		case OPC_CONV_DTOI:
			DToI();
			break;

		case OPC_CONV_DTOUI:
			DToUI();
			break;

		case OPC_CONV_ITOS:
			IToS();
			break;

		case OPC_CONV_ITOSB:
			IToSB();
			break;

		case OPC_CONV_PTOB:
			PToB();
			break;

		case OPC_INEG:
			INeg();
			break;

		case OPC_INOT:
			INot();
			break;

		case OPC_FNEG:
			FNeg();
			break;

		case OPC_DNEG:
			DNeg();
			break;

		case OPC_HALT:
		case OPC_BREAK:

			// plain ret
			if (fastCall)
				Int3();
			else
				Retn();

			break;

		case OPC_CHKSTK:
		{
			Int limit = DecodeUImm24(ins);
			limit *= Stack::WORD_SIZE;

			// lea eax, [edi+const]
			Lea(Eax.ToRegPtr(), MemPtr(Edi - limit));
			// [esi] = stack bottom
			// sub eax,[esi + ...]
			Sub(Eax.ToRegPtr(), MemPtr(StackObjectPtr() + 2*Stack::WORD_SIZE));
			// jae $+1
			EmitNew(0x73);
			Emit(0x01);
			// int 3
			Int3();
		}
		break;

		case OPC_SWITCH:
		{
			Int range = DecodeUImm24(ins);
			SwitchTable(prog, range, i);
			i += range + 1;
			nextBarrier = prog.barriers[++nextBarrierIndex];
		}
		break;

		case OPC_FSQRT:
			FSqrt();
			break;

		case OPC_DSQRT:
			DSqrt();
			break;

		default:
			LETHE_ASSERT(false && "opcode not handled!");
		}

		// silence clang analyzer
		(void)lastConst;

		lastConst = 0;
	}

	prevPcToCode = pcToCode;

	DoFixups(prog, pass);

	prog.jitRef = this;
	return 1;

#undef VMJITX86_OPT_CMP_JMP_FLOAT
#undef VMJITX86_OPT_CMP_JMP_DOUBLE
#undef VMJITX86_OPT_CMP_JMP_FLOAT_CUSTOM
}

Int VmJitX86::GetPCFromCodePtr(const void *codePtr) const
{
	const auto *cbase = code.GetData();
	const Byte *cptr = static_cast<const Byte *>(codePtr);

	if (cptr < cbase || cptr > cbase + code.GetSize())
		return -1;

	Int res = -1;
	IntPtr bestDist = IntPtr(~(UIntPtr)0 >> 1);

	// find closest pcToCode less than this
	for (Int i=0; i<pcToCode.GetSize(); i++)
	{
		const auto *adr = cbase + pcToCode[i];

		if (adr > cptr)
			continue;

		auto diff = IntPtr(cptr - adr);

		if (diff < bestDist)
		{
			bestDist = diff;
			res = i;
		}
	}

	return res;
}

const void *VmJitX86::GetCodePtr(Int pc) const
{
	if (pc < 0 || pc >= pcToCode.GetSize())
		return nullptr;

	return code.GetData() + pcToCode[pc];
}

Int VmJitX86::FindFunctionPC(const void *address) const
{
	auto ci = funcCodeToPC.Find(address);
	return ci == funcCodeToPC.End() ? -1 : ci->value;
}

#undef VMJITX86_CAN_CHAIN_AADD

#if LETHE_COMPILER_MSC_ONLY
#	pragma warning(push)
#	pragma warning(disable:4740)
#	pragma warning(disable:4731)
#endif

ExecResult VmJitX86::ExecScriptFunc(Vm &vm, Int scriptPC)
{
	return ExecScriptFuncPtr(vm, code.GetData() + pcToCode[scriptPC]);
}

ExecResult VmJitX86::ExecScriptFuncPtr(Vm &vm, const void *codeadr)
{
	// edi  = stack ptr
	// esi  = Stack object ptr
	// ebp  = this ptr

	// x64:
	// REX prefix comes after word-size prefixes (66/67)
	// r12..r15 preserved across calls by both Win/Unix ABI
	// first arg passed in: rcx for Win, rdi for Unix
	//
	// rsi = global ptr (because we want short encoding)
	// r12 = stack object ptr
	//
	// => need to rename a bit...
	// stkPtrReg = Edi
	// stkObjReg = R12d
	// thisPtrReg = Ebp
	// globalBaseReg = 0/Esi => will be used for native calls

	Stack &stk = *vm.stack;
	void *stkadr = &stk;
	void *stktop = stk.GetTop();
	const void *thisadr = stk.GetThis();

	stk.cpool = &vm.prog->cpool;

#if LETHE_32BIT

	if (fastCall)
	{
#if LETHE_COMPILER_MSC_ONLY
		__asm
		{
			pushad
			mov edi, [stktop]
			mov esi, [stkadr]
			mov eax,[codeadr]
			mov ebp, [thisadr]
			call eax
			mov [esi+0], edi
			popad
		}
#else
		asm(
			"movl %0, %%edi;"
			"movl %1, %%esi;"
			"movl %2, %%eax;"
			"movl %3, %%ecx;"
			"pushl %%ebp;"
			"movl %%ecx, %%ebp;"
			"call *%%eax;"
			"popl %%ebp;"
			"movl %%edi, 0(%%esi);"
			: : "m"(stktop), "m"(stkadr), "m"(codeadr), "m"(thisadr) :
			"cc", "eax", "ebx", "ecx", "edx", "esi", "edi"
		);
#endif
		return EXEC_OK;
	}

	// FIXME: doesn't set/restore ebp (thisPtr)

#if LETHE_COMPILER_MSC_ONLY
	__asm
	{
		pushad
		mov edi,[stktop]
		mov esi,[stkadr]
		mov ecx, [codeadr]
		mov ebp,[thisadr]
		sub edi,4
		lea eax,[_retadr]
		mov dword ptr [edi],eax
		jmp dword ptr [ecx]
		_retadr:
		mov [esi+0],edi
		popad
	}
#else
	asm(
		"movl %0, %%edi;"
		"movl %1, %%esi;"
		"subl $4, %%edi;"
		"leal (_retadr), %%eax;"
		"movl %%eax, (%%edi);"
		"movl %3, %%ecx;"
		"movl %2, %%eax;"
		"pushl %%ebp;"
		"movl %%ecx,%%ebp;"
		"jmp *%%eax;"
		"_retadr:;"
		"popl %%ebp;"
		"movl %%edi, 0(%%esi);"
		: : "m"(stktop), "m"(stkadr), "m"(codeadr), "m"(thisadr) :
		"cc", "eax", "ebx", "ecx", "edx", "esi", "edi"
	);
#endif

#else
	// TODO: 64-bit stub...
	LETHE_ASSERT(fastCall);

	const void *param[] =
	{
		stktop,
		stkadr,
		codeadr,
		thisadr,
		stk.GetConstantPool().GetGlobalData(),
		code.GetData()
	};
#	if LETHE_COMPILER_MSC_ONLY
	VmJitX64_Stub(param);
#	else
	// the rest of the world (gcc/clang) having inline assembly
	// unfortunately, some clang versions report "inline assembly requires more registers than available"
	// so I have to use a temporary buffer
	const void *paramptr = param;
	asm(
		"movq %0, %%rax;"
		"movq 0*8(%%rax), %%rdi;"
		"movq 1*8(%%rax), %%r12;"
		"movq 3*8(%%rax), %%rcx;"
		"movq 4*8(%%rax), %%rsi;"
		"movq 5*8(%%rax), %%r13;"
		"movq 2*8(%%rax), %%rax;"
		"pushq %%rbp;"
		"movq %%rcx, %%rbp;"
		"call *%%rax;"
		"popq %%rbp;"
		"movq %%rdi, 0(%%r12);"
		: : "m"(paramptr) :
		"cc", "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r12", "r13", "r14",
		"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
	);
#	endif

#endif

	return EXEC_OK;
}


}

#if LETHE_COMPILER_MSC_ONLY
#	pragma warning(pop)
#endif

#endif
