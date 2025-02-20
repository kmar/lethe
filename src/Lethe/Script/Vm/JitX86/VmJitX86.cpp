#include "VmJitX86.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Program/ConstPool.h>
#include "../Builtin.h"
#include <Lethe/Script/TypeInfo/BaseObject.h>
#include <Lethe/Core/Math/Math.h>
#include <Lethe/Core/Sys/Endian.h>

#include <Lethe/Core/Io/File.h>

//#include <stdio.h>

namespace lethe
{

#if LETHE_JIT_X86

// VmJitX86

VmJitX86::VmJitX86()
	: fastCall(1)
{
}

VmJitX86::~VmJitX86()
{
	UnregisterCode();
}

RegExpr VmJitX86::FindGpr(Int offset, bool write)
{
	return gprCache.Find(offset, write);
}

RegExpr VmJitX86::AllocGprPtr(Int offset, bool load, bool write)
{
	return AllocGpr(offset, load, write, 1);
}

RegExpr VmJitX86::AllocGpr(Int offset, bool load, bool write, bool pointer)
{
	Int flags = RA_LOAD*load + RA_WRITE*write + RA_PTR*pointer;

	bool discard = 0;//!load && write;
	RegExpr sse = sseCache.Find(offset, 0);

	if (sse.IsRegister())
	{
		// already cached in sse => transfer or throw away...

		// disabled this assert because I copy pointers for temp => const ref calls...
		//LETHE_ASSERT(!pointer);
		RegExpr res = gprCache.Alloc(offset, *this, RA_WRITE*write + RA_PTR*pointer);

		if (load)
		{
			if (res.GetSize() == MEM_QWORD)
				Movq(res, sse);
			else
				Movd(res, sse);
		}

		// must move tracked vars!
		sseCache.MoveTracked(gprCache, offset);

		if (sseCache.Free(offset, *this, 1) && !discard)
			gprCache.SetWrite(offset);

		return res;
	}

	return gprCache.Alloc(offset, *this, flags);
}

RegExpr VmJitX86::AllocGprWritePtr(Int offset, bool load)
{
	return AllocGprPtr(offset, load, 1);
}

RegExpr VmJitX86::AllocGprWrite(Int offset, bool load)
{
	return AllocGpr(offset, load, 1);
}

RegExpr VmJitX86::FindSse(Int offset, bool write)
{
	return sseCache.Find(offset, write);
}

RegExpr VmJitX86::AllocSseDouble(Int offset, bool load, bool write)
{
	return AllocSse(offset, load, write, true);
}

RegExpr VmJitX86::AllocSse(Int offset, bool load, bool write, bool isDouble)
{
	Int flags = RA_LOAD*load + RA_WRITE*write + RA_DOUBLE*isDouble;

	bool discard = false;//!load && write;
	RegExpr gpr = gprCache.Find(offset, false);

	if (gpr.IsRegister())
	{
		// already cached in gpr => transfer or throw away...

		RegExpr res = sseCache.Alloc(offset, *this, write*RA_WRITE + isDouble*RA_DOUBLE);

		if (load)
		{
			if (isDouble)
				Movq(res, gpr);
			else
				Movd(res, gpr.ToReg32());
		}

		// must move tracked vars!
		gprCache.MoveTracked(sseCache, offset);

		if (gprCache.Free(offset, *this, true) && !discard)
			sseCache.SetWrite(offset);

		return res;
	}

	return sseCache.Alloc(offset, *this, flags);
}

RegExpr VmJitX86::AllocSseWrite(Int offset, bool load, bool isDouble)
{
	return AllocSse(offset, load, true, isDouble);
}

RegExpr VmJitX86::AllocSseWriteDouble(Int offset, bool load)
{
	return AllocSseWrite(offset, load, true);
}

void VmJitX86::TrackGpr(Int targ, Int src)
{
	for (;;)
	{
		Int idx = gprCache.trackMap.FindIndex(src);

		if (idx < 0)
			break;

		src = gprCache.trackMap.GetValue(idx);
	}

	gprCache.trackMap[targ] = src;
}

void VmJitX86::TrackSse(Int targ, Int src)
{
	for (;;)
	{
		Int idx = sseCache.trackMap.FindIndex(src);

		if (idx < 0)
			break;

		src = sseCache.trackMap.GetValue(idx);
	}

	sseCache.trackMap[targ] = src;
}

void VmJitX86::PushInt(UInt i, bool isPtr)
{
	DontFlush _(*this);
	Push(1, 0);

	RegExpr reg = isPtr ? AllocGprWritePtr(stackOpt) : AllocGprWrite(stackOpt);

	if (!i)
		Xor(reg.ToReg32(), reg.ToReg32());
	else
	{
		if constexpr (IsX64)
		{
			if (isPtr)
				Mov(reg, (Long)i | ((Long)1 << 32));
			else
				Mov(reg, i);
		}
		else
			Mov(reg, i);
	}
}

void VmJitX86::PushFloat(const CompiledProgram &prog, Float f)
{
	DontFlush _(*this);
	Push(1, 0);

	RegExpr reg = AllocSseWrite(stackOpt);

	if (!f)
	{
		Pxor(reg, reg);
	}
	else
	{
		auto idx = prog.cpool.fPoolMap[f];

		Mov(reg, Mem32(GlobalBase() + fconstBase + idx * sizeof(Float)));
	}
}

void VmJitX86::PushDouble(const CompiledProgram &prog, Double d)
{
	DontFlush _(*this);
	Push(Stack::DOUBLE_WORDS, 0);

	RegExpr reg = AllocSseWriteDouble(stackOpt);

	if (!d)
	{
		Pxor(reg, reg);
	}
	else
	{
		auto idx = prog.cpool.dPoolMap[d];

		Movq(reg, Mem32(GlobalBase() + dconstBase + idx * sizeof(Double)));
	}
}

void VmJitX86::PushPtr()
{
	PushInt(0x12345678, 1);
}

void VmJitX86::PushIntAccum(const RegExpr &nreg, bool isPtr)
{
	DontFlush _(*this);
	Push(1, 0);

	RegExpr dst = isPtr ? AllocGprWritePtr(stackOpt) : AllocGprWrite(stackOpt);

	if (dst.base != nreg.base)
	{
		LETHE_ASSERT(!nreg.IsRegister() || dst.GetSize() == nreg.GetSize());
		Mov(dst, nreg);
	}
}

RegExpr VmJitX86::GetInt(Int i)
{
	DontFlush _(*this);
	i += stackOpt;

	RegExpr reg = FindGpr(i, 0);

	if (reg.IsRegister())
		return reg;

	reg = AllocGpr(i, 1);

	return reg;
}

RegExpr VmJitX86::GetIntSignExtend(Int i)
{
	Int old = lastIns;

	if (IsX64)
	{
		ForceMovsxd(true);
	}

	auto reg = GetInt(i);

	if (IsX64)
	{
		ForceMovsxd(false);

		reg = reg.ToRegPtr();

		// if last instruction is movsxd, we're done
		if (lastIns != old)
		{
			if (lastIns+1 <= code.GetSize() && code[lastIns] == 0x63)
				return reg;

			if (lastIns+2 <= code.GetSize() && code[lastIns] == 0x48 && code[lastIns+1] == 0x63)
				return reg;
		}

		// unfortunately we have to sign-extend to 64-bit to avoid crashes when indexing with negative_var + constant
		DontFlush _(*this);
		Movsxd(reg, reg.ToReg32());
	}

	return reg;
}

RegExpr VmJitX86::GetFloat(Int i)
{
	DontFlush _(*this);
	i += stackOpt;

	RegExpr reg = FindSse(i, 0);

	if (reg.IsRegister())
		return reg;

	reg = AllocSse(i, 1);

	return reg;
}

RegExpr VmJitX86::GetDouble(Int i)
{
	DontFlush _(*this);
	i += stackOpt;

	RegExpr reg = FindSse(i, false);

	if (reg.IsRegister())
		return reg;

	reg = AllocSseDouble(i, true);

	return reg;
}

void VmJitX86::PushPtrAccum(const RegExpr &nreg)
{
	PushIntAccum(nreg, 1);
}

RegExpr VmJitX86::GetPtr(Int i)
{
	// FIXME: fix this once it happens
	//LETHE_ASSERT( lastAdr != stackOpt + i );
	if (lastAdr == stackOpt + i)
		FlushLastAdr();

	return GetPtrNoAdr(i);
}

RegExpr VmJitX86::GetPtrNoAdr(Int i)
{
	DontFlush _(*this);
	i += stackOpt;

	RegExpr reg = FindGpr(i, 0);

	if constexpr (!IsX64)
		if (reg.IsRegister())
			return reg;

	reg = AllocGprPtr(i, 1);

	return reg.ToRegPtr();
}

RegExpr VmJitX86::GetStackAdr(Int i)
{
	DontFlush _(*this);
	i += stackOpt;

	i *= Stack::WORD_SIZE;

	RegExpr reg = AllocGprWritePtr(stackOpt-1);

	// lea eax,[edi + constant]
	Lea(reg.ToRegPtr(), MemPtr(Edi + i));

	return reg;
}

RegExpr VmJitX86::GlobalBase() const
{
	return globalBase;
}

RegExpr VmJitX86::StackObjectPtr() const
{
	return stackObjectPtr;
}

RegExpr VmJitX86::FirstArg() const
{
	return firstArgReg;
}

RegExpr VmJitX86::GetGlobalInt(Int i, bool isPtr)
{
	DontFlush _(*this);

	Push(1, 0);
	RegExpr reg = isPtr ? AllocGprWritePtr(stackOpt) : AllocGprWrite(stackOpt);
	// note: global ofs is absolute, not in stackword units
	Mov(reg, Mem32(GlobalBase() + i));

	return reg;
}

RegExpr VmJitX86::GetGlobalFloat(Int i)
{
	DontFlush _(*this);

	Push(1, 0);
	RegExpr reg = AllocSseWrite(stackOpt);
	// note: global ofs is absolute, not in stackword units
	Movd(reg, Mem32(GlobalBase() + i));

	return reg;
}

RegExpr VmJitX86::GetGlobalDouble(Int i)
{
	DontFlush _(*this);

	Push(Stack::DOUBLE_WORDS, false);
	RegExpr reg = AllocSseWriteDouble(stackOpt);
	// note: global ofs is absolute, not in stackword units
	Movq(reg, Mem32(GlobalBase() + i));

	return reg;
}

RegExpr VmJitX86::GetGlobalPtr(Int ofs)
{
	return GetGlobalInt(ofs, 1);
}

void VmJitX86::SetGlobalInt(Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units
	// mov [base+const],eax
	Mov(Mem32(GlobalBase() + ofs), nreg);
}

void VmJitX86::SetGlobalFloat(Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units
	// mov [base+const],eax
	Movd(Mem32(GlobalBase() + ofs), nreg);
}

void VmJitX86::SetGlobalDouble(Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units
	// mov [base+const],eax
	Movq(Mem32(GlobalBase() + ofs), nreg);
}

void VmJitX86::SetGlobalPtr(Int ofs, const RegExpr &nreg)
{
	SetGlobalInt(ofs, nreg.ToRegPtr());
}

void VmJitX86::Pop(Int count)
{
	stackOpt += count;

	if (lastAdr < stackOpt)
		lastAdr = INVALID_STACK_INDEX;

	gprCache.FlushBelow(stackOpt);
	sseCache.FlushBelow(stackOpt);
}

void VmJitX86::PopGen(Int count)
{
	DontFlush _(*this);
	count *= Stack::WORD_SIZE;

	if (preserveFlags)
		Lea(Edi.ToRegPtr(), MemPtr(Edi + count));
	else
	{
		// add edi, const
		Add(Edi.ToRegPtr(), count);
	}
}

RegExpr VmJitX86::GetGlobalSByte(Int ofs)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units

	RegExpr reg = AllocGpr(stackOpt-1);
	// movsx eax, byte [base + const]
	Movsx(reg, Mem8(GlobalBase() + ofs));

	return reg;
}

RegExpr VmJitX86::GetGlobalByte(Int ofs)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units

	RegExpr reg = AllocGprWrite(stackOpt-1);
	// movzx eax, byte [base + const]
	Movzx(reg, Mem8(GlobalBase() + ofs));

	return reg;
}

RegExpr VmJitX86::GetGlobalShort(Int ofs)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units

	RegExpr reg = AllocGprWrite(stackOpt-1);
	// movsx eax, word [base + const]
	Movsx(reg, Mem16(GlobalBase() + ofs));

	return reg;
}

RegExpr VmJitX86::GetGlobalUShort(Int ofs)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units

	RegExpr reg = AllocGprWrite(stackOpt-1);
	// movzx eax, word [base + const]
	Movzx(reg, Mem16(GlobalBase() + ofs));

	return reg;
}

void VmJitX86::SetGlobalByte(Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units
	// mov [base + const], al
	Mov(Mem8(GlobalBase() + ofs), nreg.ToReg8());
}

void VmJitX86::SetGlobalUShort(Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	// note: global ofs is absolute, not in stackword units
	// mov [base + const], ax
	Mov(Mem16(GlobalBase() + ofs), nreg.ToReg16());
}

RegExpr VmJitX86::GetGlobalAdr(Int i)
{
	DontFlush _(*this);

	Push(1, 0);
	RegExpr reg = AllocGprWritePtr(stackOpt);

	// note: global ofs is absolute, not in stackword units
	// lea eax,[base + constant]
	Lea(reg, Mem32(GlobalBase() + i));

	return reg;
}

RegExpr VmJitX86::GetLocalSByte(Int ofs)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr sreg = AllocGpr(ofs, 1);
	RegExpr dreg = AllocGprWrite(stackOpt-1);
	RegExpr breg = sreg.ToReg8();

	Movsx(dreg, breg);

	return dreg;
}

RegExpr VmJitX86::GetLocalByte(Int ofs)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr sreg = AllocGpr(ofs, 1);
	RegExpr dreg = AllocGprWrite(stackOpt-1);
	RegExpr breg = sreg.ToReg8();

	Movzx(dreg, breg);

	return dreg;
}

RegExpr VmJitX86::GetLocalShort(Int ofs)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr sreg = AllocGpr(ofs, 1);
	RegExpr dreg = AllocGprWrite(stackOpt-1);
	RegExpr wreg = sreg.ToReg16();

	Movsx(dreg, wreg);

	return dreg;
}

RegExpr VmJitX86::GetLocalUShort(Int ofs)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr sreg = AllocGpr(ofs, 1);
	RegExpr dreg = AllocGprWrite(stackOpt-1);
	RegExpr wreg = sreg.ToReg16();

	Movzx(dreg, wreg);

	return dreg;
}

void VmJitX86::SetLocalByte(Int ofs, const RegExpr &src)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr reg = AllocGprWrite(ofs, 1);
	// mov [edi + const], al
	Mov(reg.ToReg8(), src.ToReg8());
}

void VmJitX86::SetLocalUShort(Int ofs, const RegExpr &src)
{
	DontFlush _(*this);
	ofs += stackOpt;

	RegExpr reg = AllocGprWrite(ofs, 1);
	// mov [edi + const], ax
	Mov(reg.ToReg16(), src.ToReg16());
}

void VmJitX86::SetLocalInt(Int ofs, const RegExpr &src, bool transfer, bool pointer)
{
	DontFlush _(*this);
	ofs += stackOpt;

	if (transfer && gprCache.TransferRegWrite(src, ofs))
		return;

	RegExpr reg = pointer ? AllocGprWritePtr(ofs) : AllocGprWrite(ofs);

	// this is used by LtoI; simply bypass useless move (nop)
	if (src.IsRegister() && src.ToRegPtr().base == reg.ToRegPtr().base)
		return;

	// mov [edi+const],eax
	Mov(reg, src);
}

void VmJitX86::SetLocalFloat(Int ofs, const RegExpr &src, bool transfer)
{
	DontFlush _(*this);
	ofs += stackOpt;

	if (transfer && sseCache.TransferRegWrite(src, ofs))
		return;

	RegExpr reg = AllocSseWrite(ofs);
	// mov [edi+const],eax
	Movss(reg, src);
}

void VmJitX86::SetLocalDouble(Int ofs, const RegExpr &src, bool transfer)
{
	DontFlush _(*this);
	ofs += stackOpt;

	if (transfer && sseCache.TransferRegWrite(src, ofs))
		return;

	RegExpr reg = AllocSseWriteDouble(ofs);
	// mov [edi+const],eax
	Movsd(reg, src);
}

void VmJitX86::SetLocalPtr(Int ofs, const RegExpr &src, bool transfer)
{
	SetLocalInt(ofs, src.ToRegPtr(), transfer, 1);
}

void VmJitX86::StoreIndirectImm(Int ofs, MemSize msz, bool isDouble)
{
	DontFlush _(*this);
	RegExpr reg;

	if (lastAdr == stackOpt)
	{
		reg = lastAdrExpr;
		lastAdr = INVALID_STACK_INDEX;
	}
	else
	{
		FlushLastAdr();
		reg = GetPtr(0);
	}

	gprCache.reserved = reg.GetRegMask();
	RegExpr src = msz == MEM_XMM ? (isDouble ? AllocSseDouble(1+stackOpt, true) : AllocSse(1+stackOpt, true)) :
				  (msz == MEM_QWORD ? AllocGprPtr(1+stackOpt, 1) : AllocGpr(1+stackOpt, 1));
	gprCache.reserved = 0;

	switch(msz)
	{
	case MEM_BYTE:
		Mov(Mem8(reg + ofs), src.ToReg8());
		break;

	case MEM_WORD:
		Mov(Mem16(reg + ofs), src.ToReg16());
		break;

	case MEM_DWORD:
		Mov(Mem32(reg + ofs), src);
		break;

	case MEM_QWORD:
		Mov(Mem64(reg + ofs), src.ToReg64());
		break;

	case MEM_XMM:
		if (isDouble)
			Movq(Mem32(reg + ofs), src);
		else
			Movd(Mem32(reg + ofs), src);
		break;

	default:
		;
	}
}

void VmJitX86::StoreIndirectImmByte(Int ofs)
{
	StoreIndirectImm(ofs, MEM_BYTE);
}

void VmJitX86::StoreIndirectImmUShort(Int ofs)
{
	StoreIndirectImm(ofs, MEM_WORD);
}

void VmJitX86::StoreIndirectImmInt(Int ofs)
{
	StoreIndirectImm(ofs, MEM_DWORD);
}

void VmJitX86::StoreIndirectImmFloat(Int ofs)
{
	StoreIndirectImm(ofs, MEM_XMM);
}

void VmJitX86::StoreIndirectImmDouble(Int ofs)
{
	StoreIndirectImm(ofs, MEM_XMM, true);
}

void VmJitX86::StoreIndirectImmPtr(Int ofs)
{
	StoreIndirectImm(ofs, IsX64 ? MEM_QWORD : MEM_DWORD);
}

void VmJitX86::Push(Int count, bool zero)
{
	stackOpt -= count;

	if (zero)
		PushGen(count, zero, 1);
}

void VmJitX86::PushGen(Int count, bool zero, bool noAdjust)
{
	LETHE_ASSERT(count > 0);

	if (count <= 0)
		return;

	count *= Stack::WORD_SIZE;

	DontFlush _(*this);

	if (!noAdjust)
	{
		// sub edi, count
		if (preserveFlags)
			Lea(Edi, Mem32(Edi - count));
		else
			Sub(Edi.ToRegPtr(), count);
	}

	if (zero)
	{
		count /= Stack::WORD_SIZE;
		LETHE_ASSERT(count > 0);

		if (count <= 4)
		{
			RegExpr scratch = Eax;

			for (Int i=0; i<gprCache.size; i++)
			{
				const RegCacheEntry &e = gprCache.cache[i];

				if (e.offset == INVALID_STACK_INDEX)
				{
					scratch = e.reg;
					break;
				}
			}

			bool preserveEax = gprCache.RegInUse(scratch);

			if (preserveEax)
				RPush(scratch.ToRegPtr());

			Xor(scratch, scratch);

			for (Int i=0; i<count; i++)
				Mov(MemPtr(Edi + Stack::WORD_SIZE*(i+stackOpt)), scratch.ToRegPtr());

			if (preserveEax)
				RPop(scratch.ToRegPtr());
		}
		else
		{
			bool preserveEax = gprCache.RegInUse(Eax);
			bool preserveEdx = gprCache.RegInUse(Edx);

			if (preserveEdx)
				RPush(Edx.ToRegPtr());

			if (preserveEax)
				RPush(Eax.ToRegPtr());

			// xor eax, eax
			// mov edx, count-1
			// loop: mov [edi+edx*4],eax
			// dec edx
			// jns loop
			Xor(Eax, Eax);
			Mov(Edx, count-1);
			Int label = code.GetSize();
			Mov(MemPtr(Edi + Edx*Stack::WORD_SIZE + stackOpt*Stack::WORD_SIZE), Eax.ToRegPtr());
			Dec(Edx);
			EmitNew(0x79);
			Emit(label - (code.GetSize()+1));

			if (preserveEax)
				RPop(Eax.ToRegPtr());

			if (preserveEdx)
				RPop(Edx.ToRegPtr());
		}
	}
}

void VmJitX86::MulAccum(const RegExpr &nreg, Int scl, bool isTemp, bool isPtr)
{
	DontFlush _(*this);

	if (scl == 1)
		return;

	// here we destroy the contents of the reg!
	if (isTemp)
		gprCache.MarkAsTemp(nreg, *this);

	RegExpr targ = nreg;

	if (isPtr)
		targ = targ.ToRegPtr();

	if (!scl)
	{
		// must handle zero explicitly -> shl would miscompile
		Xor(targ, targ);
	}
	else if (IsPowerOfTwo(scl))
	{
		// shl eax,const
		Shl(targ, Log2Int(scl));
	}
	else
	{
		// imul eax, eax, const
		AsmX86::IMul(targ, targ, scl);
	}
}

void VmJitX86::AddLocalPtrAccum(const RegExpr &nreg, Int ofs, bool canChain)
{
	DontFlush _(*this);
	// add [edi+4*ofs],eax
	RegExpr dst;

	if (lastAdr == ofs + stackOpt)
	{
		lastAdr = INVALID_STACK_INDEX;

		if (canChain)
		{
			lastAdr = ofs + stackOpt;
			lastAdrExpr = nreg + lastAdrExpr;
		}
		else
		{
			dst = AllocGprWritePtr(ofs + stackOpt);
			Lea(dst, Mem32(nreg + lastAdrExpr));
		}

		return;
	}

	FlushLastAdr();
	dst = AllocGprWritePtr(ofs + stackOpt, 1);

	if (canChain)
	{
		lastAdr = ofs + stackOpt;
		lastAdrExpr = nreg + dst;
	}
	else
	{
		lastAdr = INVALID_STACK_INDEX;
		Lea(dst, Mem32(nreg + dst));
	}
}

void VmJitX86::IAdd()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	Add(dst, src);
	Pop(1);
}

void VmJitX86::ISub()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	Sub(dst, src);
	Pop(1);
}

void VmJitX86::IMul()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	AsmX86::IMul(dst, src);
	Pop(1);
}

#if LETHE_64BIT
void VmJitX86::LAdd()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	Add(dst, src);
	Pop(1);
}

void VmJitX86::LSub()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	Sub(dst, src);
	Pop(1);
}

void VmJitX86::LMul()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	AsmX86::IMul(dst, src);
	Pop(1);
}

void VmJitX86::LAnd()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	And(dst, src);
	Pop(1);
}

void VmJitX86::LOr()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	Or(dst, src);
	Pop(1);
}

void VmJitX86::LXor()
{
	DontFlush _(*this);
	RegExpr src = GetPtrNoAdr(0);
	RegExpr dst = AllocGprWritePtr(1+stackOpt, 1);
	Xor(dst, src);
	Pop(1);
}

void VmJitX86::LShr()
{
	IShlLike(&VmJitX86::Shr, true);
}

void VmJitX86::LSar()
{
	IShlLike(&VmJitX86::Sar, true);
}

void VmJitX86::LShl()
{
	IShlLike(&VmJitX86::Shl, true);
}

void VmJitX86::LNeg()
{
	DontFlush _(*this);
	// neg dword [edi]
	RegExpr reg = AllocGprWritePtr(stackOpt, 1);
	Neg(reg);
}

void VmJitX86::LNot()
{
	DontFlush _(*this);
	// not dword [edi]
	RegExpr reg = AllocGprWritePtr(stackOpt, 1);
	Not(reg);
}

void VmJitX86::LBsf()
{
	DontFlush _(*this);
	auto src = AllocGprPtr(stackOpt, true, true);
	Bsf(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::LBsr()
{
	DontFlush _(*this);
	auto src = AllocGprPtr(stackOpt, true, true);
	Bsr(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::LPopCnt()
{
	DontFlush _(*this);
	auto src = AllocGprPtr(stackOpt, true, true);
	PopCnt(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::LBswap()
{
	DontFlush _(*this);
	auto src = AllocGprPtr(stackOpt, true, true);
	Bswap(src);
}

void VmJitX86::LCmp(Cond cond)
{
	DontFlush _(*this);
	RegExpr r0 = AllocGprWritePtr(stackOpt+1, 1);
	RegExpr r1 = GetPtrNoAdr(0);
	// cmp eax,[edi]
	Cmp(r0, r1);
	// set?? al
	Setxx(cond, r0.ToReg8());
	// movzx eax,al
	Movzx(r0, r0.ToReg8());

	Pop(1);
}

#endif

void VmJitX86::UBsf()
{
	DontFlush _(*this);
	auto src = AllocGpr(stackOpt, true, true);
	Bsf(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::UBsr()
{
	DontFlush _(*this);
	auto src = AllocGpr(stackOpt, true, true);
	Bsr(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::UPopCnt()
{
	DontFlush _(*this);
	auto src = AllocGpr(stackOpt, true, true);
	PopCnt(src, src);
	AllocGprWrite(stackOpt);
}

void VmJitX86::UBswap()
{
	DontFlush _(*this);
	auto src = AllocGpr(stackOpt, true, true);
	Bswap(src);
}

void VmJitX86::IDivLike(bool isSigned, bool remainder)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	RegExpr src = GetInt(0);

	// make sure Eax is dst

	LETHE_ASSERT(!IsX64 || (dst.GetSize() == MEM_DWORD && src.GetSize() == MEM_DWORD));

	if (src.base == EAX)
	{
		// we have to remap (swap) those now
		gprCache.SwapRegs(dst, src);
		Xchg(dst, src);
		Swap(dst, src);
	}

	if (dst.base != EAX)
	{
		// make sure dst is eax
		gprCache.SwapRegs(dst, Eax);
		Xchg(dst.ToRegPtr(), Eax.ToRegPtr());
		dst = Eax;
	}

	bool preserveEdx = gprCache.RegInUse(Edx);

	if (preserveEdx)
		RPush(Edx.ToRegPtr());

	RegExpr denom = src;

	if (src.base == EDX)
	{
		LETHE_ASSERT(preserveEdx);
		denom = Mem32(Esp);
	}

	// cdq
	if (isSigned)
	{
		Cdq();
		AsmX86::IDiv(denom);
	}
	else
	{
		Xor(Edx, Edx);
		Div(denom);
	}

	if (remainder)
		Mov(dst, Edx);

	if (preserveEdx)
		RPop(Edx.ToRegPtr());

	Pop(1);
}

void VmJitX86::IDiv()
{
	IDivLike(1, 0);
}

void VmJitX86::UIDiv()
{
	IDivLike(0, 0);
}

void VmJitX86::IMod()
{
	IDivLike(1, 1);
}

void VmJitX86::UIMod()
{
	IDivLike(0, 1);
}

void VmJitX86::IAnd()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	And(dst, src);
	Pop(1);
}

void VmJitX86::IAnd(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	And(dst, val);
}

void VmJitX86::IOr()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	Or(dst, src);
	Pop(1);
}

void VmJitX86::IOr(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Or(dst, val);
}

void VmJitX86::IXor()
{
	DontFlush _(*this);
	RegExpr src = GetInt(0);
	RegExpr dst = AllocGprWrite(1+stackOpt, 1);
	Xor(dst, src);
	Pop(1);
}

void VmJitX86::IXor(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Xor(dst, val);
}

void VmJitX86::IShlLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &), bool isPtr)
{
	DontFlush _(*this);

	RegExpr src = GetInt(0);
	RegExpr dst = isPtr ? AllocGprWritePtr(1+stackOpt, 1) : AllocGprWrite(1+stackOpt, 1);

	LETHE_ASSERT(src.GetSize() == MEM_DWORD);

	if (dst.base == ECX || dst.base == RCX)
	{
		// we have to remap (swap) those now
		gprCache.SwapRegs(dst, src);

		Xchg(dst.ToRegPtr(), src.ToRegPtr());

		Swap(dst, src);

		if (isPtr)
		{
			dst = dst.ToRegPtr();
			src = src.ToReg32();
		}
	}

	if (src.base != ECX)
	{
		// simply swap to Ecx
		gprCache.SwapRegs(src, Ecx);
		Xchg(Ecx.ToRegPtr(), src.ToRegPtr());
	}

	(this->*ins)(dst, Cl);

	Pop(1);
}

void VmJitX86::IShl()
{
	IShlLike(&VmJitX86::Shl);
}

void VmJitX86::IShl(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Shl(dst, val);
}

void VmJitX86::IShr()
{
	IShlLike(&VmJitX86::Shr);
}

void VmJitX86::IShr(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Shr(dst, val);
}

void VmJitX86::ISar()
{
	IShlLike(&VmJitX86::Sar);
}

void VmJitX86::ISar(Int val)
{
	DontFlush _(*this);
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Sar(dst, val);
}

void VmJitX86::FAddLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &))
{
	DontFlush _(*this);

	RegExpr dst2 = FindSse(stackOpt+1);

	RegExpr src = GetFloat(0);

	if (dst2.base == src.base)
	{
		if (sseCache.TransferRegWrite(src, stackOpt+1, stackOpt))
		{
			(this->*ins)(src, src);
			Pop(1);
			return;
		}
	}

	RegExpr dst = AllocSseWrite(stackOpt+1, 1);

	// addss xmm0, [edi+4]
	(this->*ins)(dst, src);

	Pop(1);
}

void VmJitX86::FAddLikeDouble(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &))
{
	DontFlush _(*this);

	RegExpr dst2 = FindSse(stackOpt + Stack::DOUBLE_WORDS);

	RegExpr src = GetDouble(0);

	if (dst2.base == src.base)
	{
		if (sseCache.TransferRegWrite(src, stackOpt + Stack::DOUBLE_WORDS, stackOpt))
		{
			(this->*ins)(src, src);
			Pop(Stack::DOUBLE_WORDS);
			return;
		}
	}

	RegExpr dst = AllocSseWriteDouble(stackOpt + Stack::DOUBLE_WORDS, 1);

	// addss xmm0, [edi+4]
	(this->*ins)(dst, src);

	Pop(Stack::DOUBLE_WORDS);
}

void VmJitX86::FAdd()
{
	FAddLike(&VmJitX86::Addss);
}

void VmJitX86::DAdd()
{
	FAddLikeDouble(&VmJitX86::Addsd);
}

void VmJitX86::FAddTopLike(void (VmJitX86::*ins)(const RegExpr &, const RegExpr &), Int ofs, const RegExpr &nreg)
{
	DontFlush _(*this);
	RegExpr rtop = AllocSseWrite(stackOpt, 1);
	(this->*ins)(rtop, nreg);
	// now simply move stackOpt + ofs to stackOpt, because stackOpt will be removed soon
	sseCache.Free(stackOpt+ofs, *this, 1);
	sseCache.cache[rtop.base - XMM0].offset = stackOpt + ofs;
}

void VmJitX86::FAddTop(Int ofs, const RegExpr &nreg)
{
	FAddTopLike(&VmJitX86::Addss, ofs, nreg);
}

void VmJitX86::FMul()
{
	FAddLike(&VmJitX86::Mulss);
}

void VmJitX86::DMul()
{
	FAddLikeDouble(&VmJitX86::Mulsd);
}

void VmJitX86::FMulTop(Int ofs, const RegExpr &nreg)
{
	FAddTopLike(&VmJitX86::Mulss, ofs, nreg);
}

void VmJitX86::FSub()
{
	DontFlush _(*this);

	RegExpr src = GetFloat(0);
	RegExpr dst = AllocSseWrite(stackOpt+1, 1);

	Subss(dst, src);

	Pop(1);
}

void VmJitX86::DSub()
{
	DontFlush _(*this);

	RegExpr src = GetDouble(0);
	RegExpr dst = AllocSseWriteDouble(stackOpt + Stack::DOUBLE_WORDS, 1);

	Subsd(dst, src);

	Pop(Stack::DOUBLE_WORDS);
}

void VmJitX86::FSubTop(Int ofs, const RegExpr &nreg)
{
	FAddTopLike(&VmJitX86::Subss, ofs, nreg);
}

void VmJitX86::FDiv()
{
	DontFlush _(*this);

	RegExpr src = GetFloat(0);
	RegExpr dst = AllocSseWrite(stackOpt+1, 1);

	// divss xmm0, [edi]
	Divss(dst, src);

	Pop(1);
}

void VmJitX86::DDiv()
{
	DontFlush _(*this);

	RegExpr src = GetDouble(0);
	RegExpr dst = AllocSseWriteDouble(stackOpt + Stack::DOUBLE_WORDS, 1);

	// divss xmm0, [edi]
	Divsd(dst, src);

	Pop(Stack::DOUBLE_WORDS);
}

void VmJitX86::FDivTop(Int ofs, const RegExpr &nreg)
{
	FAddTopLike(&VmJitX86::Divss, ofs, nreg);
}

void VmJitX86::FSqrt()
{
	DontFlush _(*this);
	RegExpr reg = AllocSseWrite(stackOpt, 1);
	Sqrtss(reg, reg);
}

void VmJitX86::DSqrt()
{
	DontFlush _(*this);
	RegExpr reg = AllocSseWriteDouble(stackOpt, 1);
	Sqrtsd(reg, reg);
}

void VmJitX86::ICmpZ()
{
	DontFlush _(*this);
	RegExpr reg = AllocGprWrite(stackOpt, 1);

	// test eax,eax
	Test(reg, reg);
	// setz al
	Setxx(COND_Z, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
}

void VmJitX86::ICmpNZ()
{
	DontFlush _(*this);
	RegExpr reg = AllocGprWrite(stackOpt, 1);

	// test eax,eax
	Test(reg, reg);
	// setnz al
	Setxx(COND_NZ, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
}

void VmJitX86::FCmpZ()
{
	DontFlush _(*this);

	FCmpZero();

	Pop(1);
	Push(1, false);

	RegExpr reg = AllocGprWrite(stackOpt);

	// setz al
	Setxx(COND_Z, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
	NanAfterSet(COND_Z, reg);
}

void VmJitX86::FCmpNZ()
{
	DontFlush _(*this);

	FCmpZero();

	Pop(1);
	Push(1, false);

	RegExpr reg = AllocGprWrite(stackOpt);

	// setnz al
	Setxx(COND_NZ, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
	NanAfterSet(COND_NZ, reg);
}

void VmJitX86::DCmpZ()
{
	DontFlush _(*this);

	DCmpZero();

	Pop(Stack::DOUBLE_WORDS);
	Push(1, false);

	RegExpr reg = AllocGprWrite(stackOpt);

	// setz al
	Setxx(COND_Z, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
	NanAfterSet(COND_Z, reg);
}

void VmJitX86::DCmpNZ()
{
	DontFlush _(*this);

	DCmpZero();

	Pop(Stack::DOUBLE_WORDS);
	Push(1, false);

	RegExpr reg = AllocGprWrite(stackOpt);

	// setnz al
	Setxx(COND_NZ, reg.ToReg8());
	// movzx eax,al
	Movzx(reg, reg.ToReg8());
	NanAfterSet(COND_NZ, reg);
}

static const Byte ShortJumps[] =
{

	//COND_ALWAYS,
	0xeb,
	//COND_Z,
	0x74,
	//COND_NZ,
	0x75,
	//COND_LT,
	0x7c,
	//COND_LE,
	0x7e,
	//COND_GT,
	0x7f,
	//COND_GE,
	0x7d,
	//COND_ULT,
	0x72,
	//COND_ULE,
	0x76,
	//COND_UGT,
	0x77,
	//COND_UGE
	0x73,
	//COND_P
	0x7a,
	//COND_NP
	0x7b
};

static const UShort NearJumps[] =
{
	//COND_ALWAYS,
	0xe9,
	//COND_Z
	0x840f,
	//COND_NZ,
	0x850f,
	//COND_LT,
	0x8c0f,
	//COND_LE,
	0x8e0f,
	//COND_GT,
	0x8f0f,
	//COND_GE,
	0x8d0f,
	//COND_ULT,
	0x820f,
	//COND_ULE,
	0x860f,
	//COND_UGT,
	0x870f,
	//COND_UGE
	0x830f,
	//COND_P
	0x8a0f,
	//COND_NP
	0x8b0f
};

void VmJitX86::EmitJumpNan(Cond cond, Int target)
{
	if (!nanAware || cond == COND_ALWAYS || cond == COND_UGT || cond == COND_UGE)
		EmitJump(cond, target);
	else
	{
		Int base = -1;

		PreserveFlags _(*this);

		// for COND_NZ, we jump always if parity set
		if (cond == COND_NZ)
		{
			EmitJump(COND_P, target);
		}
		else
		{
			// otherwise we bypass the jump
			EmitNew(ShortJumps[COND_P]);
			base = code.GetSize();
			Emit(0);
		}

		EmitJump(cond, target);

		if (base >= 0)
			code[base] = Byte(code.GetSize() - base - 1);
	}
}

void VmJitX86::EmitJump(Cond cond, Int target)
{
	LETHE_ASSERT((UInt)target < (UInt)pcToCode.GetSize());

	Int targcode = pcToCode[target];

	if (targcode >= 0)
	{
		// jumping backward
		FlushStackOpt(true);
		Int cur = code.GetSize()+2;
		Int shortDelta = targcode - cur;

		if (Abs(shortDelta) <= 127)
		{
			// short
			EmitNew(ShortJumps[cond]);
			Emit((Byte)shortDelta);
		}
		else
		{
			// near
			Int ins = NearJumps[cond];
			EmitNew((Byte)ins);
			ins >>= 8;

			if (ins)
				Emit((Byte)ins);

			Int nearDelta = targcode - (code.GetSize()+4);
			Emit32(nearDelta);
		}
	}
	else
	{
		auto fwdJumpIndex = jumpSource.Add(code.GetSize());

		Int prevtargcode = prevPcToCode.IsEmpty() ? -1 : prevPcToCode[target];

		// jumping forward

		// short jump optimization using previous pass
		if (prevtargcode >= 0)
		{
			Int cur = prevJumpSource[fwdJumpIndex]+4;
			Int shortDelta = prevtargcode - cur;

			LETHE_ASSERT(shortDelta >= 0);

			if (Abs(shortDelta) <= 127)
			{
				EmitNewSoft(ShortJumps[cond]);
				DontFlush _(*this);
				Emit((Byte)0);

				Int adr = code.GetSize() - 1;
				AddFixup(adr, target, 3);
				return;
			}
		}

		Int ins = NearJumps[cond];
		EmitNewSoft((Byte)ins);

		DontFlush _(*this);
		ins >>= 8;

		if (ins)
			Emit((Byte)ins);

		Emit32(0);

		Int adr = code.GetSize() - 4;
		AddFixup(adr, target);
	}
}

void VmJitX86::EmitCall(Int target)
{
	// hmm, I start to hate the way I did this...
	// should have used normal stack because now calls will be slow and bloated...
	// => fixed now with fastCall (default)

	if (fastCall)
	{
		EmitNew(0xe8);
		AddFixup(code.GetSize(), target);
		Emit32(0);
		return;
	}


	Push(1, 0);
	// call $+0
	EmitNew(0xe8);
	Emit32(0);
	Int retAdr = code.GetSize();
	// pop dword [edi]
	EmitNew(0x8f);
	Emit(0x07);
	// add dword [edi],const
	EmitNew(0x83);
	Emit(0x07);
	Int addOfs = code.GetSize();
	Emit(0);
	// jump!
	EmitJump(COND_ALWAYS, target);
	// local fixup
	code[addOfs] = (Byte)(code.GetSize() - retAdr);
}

void VmJitX86::EmitFCall()
{
	FlushStackOpt();

	GetPtr(0);

	if (fastCall)
	{
		Add(Edi.ToRegPtr(), Stack::WORD_SIZE);
		// call eax
		EmitNew(0xff);
		Emit(0xd0);
		return;
	}

	// call $+0
	EmitNew(0xe8);
	Emit32(0);
	Int retAdr = code.GetSize();
	// pop dword [edi]
	EmitNew(0x8f);
	Emit(0x07);
	// add dword [edi],const
	EmitNew(0x83);
	Emit(0x07);
	Int addOfs = code.GetSize();
	Emit(0);
	// jump!
	// jmp eax
	EmitNew(0xff);
	Emit(0xe0);
	// local fixup
	code[addOfs] = (Byte)(code.GetSize() - retAdr);
}

void VmJitX86::EmitFCallDg()
{
	LETHE_ASSERT(fastCall);

	FlushStackOpt();

	GetPtr(0);

	Add(Edi.ToRegPtr(), Stack::WORD_SIZE);

	Test(Al, 1);
	// jnz
	EmitNew(ShortJumps[COND_Z]);
	auto jump0 = code.GetSize();
	Emit(0xff);

	Shr(Eax, 2);

	// load vtbl
	Mov(Edx.ToRegPtr(), MemPtr(Ebp + BaseObject::OFS_VTBL));

	// call [edx + 4*eax]
	// ff 14 c2 in 64-bit mode
	// ff 14 82 in 32-bit mode

	EmitNew(0xff);
	Emit(0x14);
	Emit(IsX64 ? 0xc2 : 0x82);

	EmitNew(ShortJumps[COND_ALWAYS]);
	auto jump1 = code.GetSize();
	Emit(0xff);

	code[jump0] += Byte(code.GetSize() - jump0);

	// call eax
	And(Eax.ToRegPtr(), -4);
	EmitNew(0xff);
	Emit(0xd0);

	code[jump1] += Byte(code.GetSize() - jump1);
}

void VmJitX86::EmitVCall(Int idx)
{
	// load vtbl
	Mov(Eax.ToRegPtr(), MemPtr(Ebp + BaseObject::OFS_VTBL));

	Int ofs = idx*sizeof(void *);

	// call [eax + 4*idx]
	EmitNew();
	Emit(0xff);

	if (ofs >= 0 && ofs < 0x80)
	{
		Emit(0x50);
		Emit(ofs);
	}
	else
	{
		Emit(0x90);
		Emit32(ofs);
	}
}

void VmJitX86::EmitNCall(Int offset, void *nfptr, bool builtin, bool method, bool trap)
{
	// note: assuming x86 cdecl call ABI preserves esi and edi! (should be true for win/msc and linux/gcc)
	// update stktop!
	// mov [esi + top_ofs], edi
	// push esi
	// call ....
	// pop eax

	Mov(MemPtr(StackObjectPtr() + 0*Stack::WORD_SIZE), Edi.ToRegPtr());

	if (method)
		Mov(MemPtr(StackObjectPtr() + 1*Stack::WORD_SIZE), Ebp.ToRegPtr());

	auto doTrap = [trap, this]()
	{
		if (trap)
		{
			Test(Eax.ToRegPtr(), Eax.ToRegPtr());
			// jz skip
			EmitNew(0x74);
			Emit(0x1);
			Int3();
		}
	};

	if (IsX64)
	{
#if !LETHE_OS_WINDOWS
		RPush(Rdi);
		RPush(Rsi);
#endif
		Mov(FirstArg(), StackObjectPtr().ToRegPtr());
		Mov(R14d.ToReg64(), Rsp);

#if LETHE_OS_WINDOWS
		// 32-byte shadow space
		Sub(Rsp, 32);
#endif

		// 16-byte align stack, required by ABI
		// and rsp,-16
		EmitNew(0x48);
		Emit(0x83);
		Emit(0xe4);
		Emit(0xf0);

		// call q,[rsi + ...]
		EmitNew(0xff);
		Emit(0x96);
		Emit32(UInt((offset+2) * Stack::WORD_SIZE + nativeFuncPtr.offset));

		doTrap();

		Mov(Rsp, R14d.ToReg64());

#if !LETHE_OS_WINDOWS
		RPop(Rsi);
		RPop(Rdi);
#endif
	}
	else
	{
		RPush(StackObjectPtr().ToRegPtr());
		EmitNew(0xe8);
		Emit32((UInt)(UIntPtr)(nfptr));
		Int adr = code.GetSize() - sizeof(void *);
		AddFixup(adr, -1, 1);

		doTrap();

		RPop(Eax.ToRegPtr());
	}

	// reload stktop!
	// mov edi,[esi + top_ofs]
	// necessary to reload stktop because of native builtin callbacks!
	if (builtin)
		Mov(Edi.ToRegPtr(), MemPtr(StackObjectPtr() + 0*Stack::WORD_SIZE));
}

void VmJitX86::ICmpNzBX(bool jmpnz, Int target, bool nocmp)
{
	{
		DontFlush _(*this);
		auto reg = AllocGprWrite(stackOpt, 1);
		// cmp dword [edi], 0
		Test(reg, reg);

		if (!nocmp)
		{
			// setnz al
			Setxx(COND_NZ, reg.ToReg8());
			// movzx eax,al
			Movzx(reg, reg.ToReg8());
		}
	}

	PreserveFlags _(*this);
	EmitJump(jmpnz ? COND_NZ : COND_Z, target);
	Pop(1);
}

void VmJitX86::FCmpNzBX(bool jmpnz, Int target)
{
	{
		DontFlush _(*this);
		FCmpZero();

		Pop(1);
		Push(1, false);
		auto reg = AllocGpr(stackOpt);

		// setnz al
		Setxx(COND_NZ, reg.ToReg8());
		// movzx eax,al
		Movzx(reg, reg.ToReg8());
	}

	PreserveFlags _(*this);
	EmitJumpNan(jmpnz ? COND_NZ : COND_Z, target);
	Pop(1);
}

void VmJitX86::DCmpNzBX(bool jmpnz, Int target)
{
	{
		DontFlush _(*this);
		DCmpZero();

		Pop(Stack::DOUBLE_WORDS);
		Push(1, false);
		auto reg = AllocGpr(stackOpt);

		// setnz al
		Setxx(COND_NZ, reg.ToReg8());
		// movzx eax,al
		Movzx(reg, reg.ToReg8());
	}

	PreserveFlags _(*this);
	EmitJumpNan(jmpnz ? COND_NZ : COND_Z, target);
	Pop(1);
}

void VmJitX86::ICmp(Cond cond)
{
	DontFlush _(*this);
	RegExpr r0 = AllocGprWrite(stackOpt+1, 1);
	RegExpr r1 = GetInt(0);
	// cmp eax,[edi]
	Cmp(r0, r1);
	// set?? al
	Setxx(cond, r0.ToReg8());
	// movzx eax,al
	Movzx(r0, r0.ToReg8());

	Pop(1);
}

void VmJitX86::NanAfterSet(Cond cond, const RegExpr &dst)
{
	if (!nanAware || (cond == COND_UGT || cond == COND_UGE))
		return;

	// jnp forward, set to false (or true in the case of COND_NZ)
	EmitNew(ShortJumps[COND_NP]);
	Int base = code.GetSize();
	Emit(0);

	if (cond == COND_NZ)
		Mov(dst.ToReg8(), 1);
	else
		Xor(dst, dst);

	code[base] += Byte(code.GetSize() - base - 1);
}

void VmJitX86::PrepFloatCond(Cond &cond, RegExpr &r0, RegExpr &r1)
{
	if (cond == COND_ULT)
	{
		cond = COND_UGT;
		Swap(r0, r1);
	}
	else if (cond == COND_ULE)
	{
		cond = COND_UGE;
		Swap(r0, r1);
	}
}

void VmJitX86::FCmp(Cond cond)
{
	DontFlush _(*this);
	RegExpr r0 = GetFloat(1);
	RegExpr r1 = GetFloat(0);

	PrepFloatCond(cond, r0, r1);

	// ucomiss xmm0,[edi]
	UComiss(r0, r1);

	// FIXME: should handle NaN properly, depending on parity flag, but this would slow us down a lot, generating bloat
	// ZF = 1 if equal
	// CF = 1 if less

	RegExpr dst = AllocGprWrite(stackOpt+1);

	// set?? al
	Setxx(cond, dst.ToReg8());

	// movzx eax,al
	Movzx(dst, dst.ToReg8());

	NanAfterSet(cond, dst);

	Pop(1);
}

void VmJitX86::DCmp(Cond cond)
{
	DontFlush _(*this);
	RegExpr r0 = GetDouble(Stack::DOUBLE_WORDS);
	RegExpr r1 = GetDouble(0);

	Pop(2*Stack::DOUBLE_WORDS);

	PrepFloatCond(cond, r0, r1);

	UComisd(r0, r1);

	Push(1, false);

	// FIXME: should handle NaN properly, depending on parity flag, but this would slow us down a lot, generating bloat
	// ZF = 1 if equal
	// CF = 1 if less

	RegExpr dst = AllocGprWrite(stackOpt);

	// set?? al
	Setxx(cond, dst.ToReg8());
	// movzx eax,al
	Movzx(dst, dst.ToReg8());

	NanAfterSet(cond, dst);
}

void VmJitX86::IBx(Cond cond, Int target, bool pop)
{
	// cmp dword [edi],0
	DontFlush _(*this);
	auto reg = AllocGpr(stackOpt, 1);

	if (pop)
		Pop(1);

	if (WouldFlush())
		_.Dismiss();

	{
		EmitNewSoft();
		DontFlush _2(*this);
		Test(reg, reg);
	}
	EmitJump(cond, target);
}

void VmJitX86::IBx2(Cond cond, Int target, Int iconst, bool isConst)
{
	DontFlush _(*this);

	RegExpr csrc;
	RegExpr cdst;

	if (isConst)
	{
		Pop(1);
		code.Resize(lastIns);
		pcToCode.Back() = pcToCode[pcToCode.GetSize()-2];
		csrc = GetInt(0);
		cdst = iconst;
		Pop(1);
	}
	else
	{
		cdst = GetInt(0);
		csrc = GetInt(1);
		Pop(2);
	}

	if (WouldFlush())
		_.Dismiss();

	{
		EmitNewSoft();
		DontFlush _2(*this);
		Cmp(csrc, cdst);
	}
	EmitJump(cond, target);
}

void VmJitX86::FBxP(Cond cond, Int target)
{
	DontFlush _(*this);
	FCmpZero();

	Pop(1);

	if (WouldFlush())
		_.Dismiss();

	EmitJumpNan(cond, target);
}

void VmJitX86::DBxP(Cond cond, Int target)
{
	DontFlush _(*this);
	DCmpZero();

	Pop(Stack::DOUBLE_WORDS);

	if (WouldFlush())
		_.Dismiss();

	EmitJumpNan(cond, target);
}

void VmJitX86::FBx2(Cond cond, Int target)
{
	RegExpr r0, r1;

	DontFlush _(*this);
	r1 = GetFloat(0);
	r0 = GetFloat(1);

	Pop(2);

	if (WouldFlush())
		_.Dismiss();

	{
		EmitNewSoft();
		DontFlush _2(*this);
		PrepFloatCond(cond, r0, r1);
		// ucomiss xmm0, xmm1
		UComiss(r0, r1);
	}

	EmitJumpNan(cond, target);
}

void VmJitX86::DBx2(Cond cond, Int target)
{
	RegExpr r0, r1;

	DontFlush _(*this);
	r1 = GetDouble(0);
	r0 = GetDouble(Stack::DOUBLE_WORDS);

	Pop(2*Stack::DOUBLE_WORDS);

	if (WouldFlush())
		_.Dismiss();

	{
		EmitNewSoft();
		DontFlush _2(*this);
		PrepFloatCond(cond, r0, r1);
		// ucomisd xmm0, xmm1
		UComisd(r0, r1);
	}

	EmitJumpNan(cond, target);
}

void VmJitX86::PCx(Cond cond, bool noadr)
{
	RegExpr r0;

	DontFlush _(*this);
	r0 = noadr ? GetPtrNoAdr(0) : GetPtr(0);
	Test(r0, r0);

	if (!noadr)
		Push(1, 0);

	RegExpr dst = AllocGprWrite(stackOpt);
	Setxx(cond, dst.ToReg8());
	Movzx(dst, dst.ToReg8());
}

void VmJitX86::PCx2(Cond cond, bool noadr)
{
	RegExpr r0, r1;

	DontFlush _(*this);
	r1 = noadr ? GetPtrNoAdr(0) : GetPtr(0);
	r0 = noadr ? GetPtrNoAdr(1) : GetPtr(1);
	Cmp(r0, r1);

	if (!noadr)
		Push(1, 0);
	else
		Pop(1);

	RegExpr dst = AllocGprWrite(stackOpt);
	Setxx(cond, dst.ToReg8());
	Movzx(dst, dst.ToReg8());
}

void VmJitX86::IAddIConst(Int i, bool isPtr)
{
	DontFlush _(*this);

	// add dword [edi], const
	if (i != 0)
	{
		RegExpr dst = isPtr ? AllocGprWritePtr(stackOpt, 1) : AllocGprWrite(stackOpt, 1);
		Add(dst, i);
	}
}

void VmJitX86::PtrAddIConst(Int i)
{
	if (lastAdr == stackOpt)
	{
		lastAdrExpr = lastAdrExpr + i;
		return;
	}

	FlushLastAdr();
	IAddIConst(i, 1);
}

void VmJitX86::AddAccum(Int i, const RegExpr &reg)
{
	DontFlush _(*this);

	// add eax, const
	if (i != 0)
		Add(reg, i);
}

void VmJitX86::AddLocal(Int ofs, Int i)
{
	DontFlush _(*this);
	ofs += stackOpt;

	if (i != 0)
	{
		RegExpr dst = AllocGprWrite(ofs, 1);
		Add(dst, i);
	}
}

void VmJitX86::AddInt(Int dofs, Int ofs, void (VmJitX86::*opc)(const RegExpr &, const RegExpr &))
{
	DontFlush _(*this);

	auto it = gprCache.trackMap.Find(stackOpt);

	if (it != gprCache.trackMap.End() && it->value == stackOpt + dofs)
	{
		gprCache.trackMap.Erase(it);
		RegExpr src = GetInt(ofs);
		RegExpr dst = AllocGprWrite(stackOpt + dofs);
		(*this.*opc)(dst, src);
		Pop(1);
		return;
	}

	RegExpr src = GetInt(ofs);
	RegExpr dst = AllocGprWrite(stackOpt + dofs);

	RegExpr rtop = AllocGprWrite(stackOpt, 1);

	// add eax, [edi + ofs]
	(*this.*opc)(rtop, src);
	Mov(dst, rtop);
	Pop(1);
}

void VmJitX86::IToF()
{
	DontFlush _(*this);
	// cvtsi2ss xmm0,[edi]
	RegExpr src = AllocGpr(stackOpt, 1);
	RegExpr dst = AllocSseWrite(stackOpt);
	Cvtsi2ss(dst, src);
}

void VmJitX86::UIToF()
{
	DontFlush _(*this);

	RegExpr src = AllocGpr(stackOpt, 1);
	RegExpr dst = AllocSseWrite(stackOpt);
	Cvtsi2ss(dst, src);
	RPush(src.ToRegPtr());
	Shr(src, 31);
	Addss(dst, Mem32(GlobalBase() + uiConvTable + src*4));
	RPop(src.ToRegPtr());
}

void VmJitX86::FToI()
{
	DontFlush _(*this);
	RegExpr src = AllocSse(stackOpt, 1);
	RegExpr dst = AllocGprWrite(stackOpt);
	// cvttss2si eax,[edi]
	Cvttss2si(dst, src);
}

void VmJitX86::FToD()
{
	DontFlush _(*this);
	RegExpr src = AllocSse(stackOpt, true);
	Pop(1);
	Push(Stack::DOUBLE_WORDS, false);
	RegExpr dst = AllocSseWriteDouble(stackOpt);
	Cvtss2sd(dst, src);
}

void VmJitX86::DToF()
{
	DontFlush _(*this);
	RegExpr src = AllocSseDouble(stackOpt, true);
	Pop(Stack::DOUBLE_WORDS);
	Push(1, false);
	RegExpr dst = AllocSseWrite(stackOpt);
	Cvtsd2ss(dst, src);
}

void VmJitX86::IToD()
{
	DontFlush _(*this);
	RegExpr src = AllocGpr(stackOpt, true);
	Pop(1);
	Push(Stack::DOUBLE_WORDS, false);
	RegExpr dst = AllocSseWriteDouble(stackOpt);
	Cvtsi2sd(dst, src);
}

void VmJitX86::UIToD()
{
	DontFlush _(*this);

	RegExpr src = AllocGpr(stackOpt, true);
	Pop(1);
	Push(Stack::DOUBLE_WORDS, false);
	RegExpr dst = AllocSseWriteDouble(stackOpt);
	Cvtsi2sd(dst, src);
	RPush(src.ToRegPtr());
	Shr(src, 31);
	Addsd(dst, Mem32(GlobalBase() + uiConvTable + 8 + src * 8));
	RPop(src.ToRegPtr());
}

void VmJitX86::DToI()
{
	DontFlush _(*this);
	RegExpr src = AllocSseDouble(stackOpt, true);
	Pop(Stack::DOUBLE_WORDS);
	Push(1, false);
	RegExpr dst = AllocGprWrite(stackOpt);
	Cvttsd2si(dst, src);
}

void VmJitX86::IToS()
{
	DontFlush _(*this);
	// movsx eax, ax
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Movsx(dst, dst.ToReg16());
}

void VmJitX86::IToSB()
{
	DontFlush _(*this);
	// movsx eax, al
	RegExpr dst = AllocGprWrite(stackOpt, 1);
	Movsx(dst, dst.ToReg8());
}

void VmJitX86::PToB()
{
	DontFlush _(*this);
	RegExpr src = AllocGprPtr(stackOpt, true);
	Test(src, src);
	Setxx(COND_NZ, src.ToReg8());
	Movzx(src.ToReg32(), src.ToReg8());
	auto nreg = AllocGprWrite(stackOpt);
	LETHE_ASSERT(nreg.ToReg32().base == src.ToReg32().base);
	(void)nreg;
}

void VmJitX86::INeg()
{
	DontFlush _(*this);
	// neg dword [edi]
	RegExpr reg = AllocGprWrite(stackOpt, 1);
	Neg(reg);
}

void VmJitX86::INot()
{
	DontFlush _(*this);
	// not dword [edi]
	RegExpr reg = AllocGprWrite(stackOpt, 1);
	Not(reg);
}

void VmJitX86::FNeg()
{
	DontFlush _(*this);
	RegExpr reg = AllocSseWrite(stackOpt, 1);
	Xorps(reg, Mem32(GlobalBase() + fxorBase));
}

void VmJitX86::DNeg()
{
	DontFlush _(*this);
	RegExpr reg = AllocSseWriteDouble(stackOpt, 1);
	// FIXME: should use Xorpd?
	Xorps(reg, Mem32(GlobalBase() + fxorBase + 16));
}

void VmJitX86::FCmpZero()
{
	DontFlush _(*this);
	auto scratchReg = AllocSse(TEMP_STACK_INDEX);
	Pxor(scratchReg, scratchReg);
	auto reg = AllocSse(stackOpt, true);
	UComiss(reg, scratchReg);
	sseCache.Free(TEMP_STACK_INDEX, *this, true);
}

void VmJitX86::DCmpZero()
{
	DontFlush _(*this);
	auto scratchReg = AllocSse(TEMP_STACK_INDEX);
	Pxor(scratchReg, scratchReg);
	auto reg = AllocSse(stackOpt, true, false, true);
	UComisd(reg, scratchReg);
	sseCache.Free(TEMP_STACK_INDEX, *this, true);
}

void VmJitX86::LoadThis(bool imm, Int ofs)
{
	DontFlush _(*this);
	FlushLastAdr();

	if (imm)
	{
		RegExpr reg = AllocGprPtr(stackOpt + ofs, 1);
		Mov(Ebp.ToRegPtr(), reg.ToRegPtr());
		return;
	}

	RegExpr reg = AllocGprWritePtr(stackOpt, 1);
	Xchg(reg.ToRegPtr(), Ebp.ToRegPtr());
}

void VmJitX86::PushThis()
{
	DontFlush _(*this);
	Push(1, 0);

	SetLastAdr(Ebp);

	/*	RegExpr reg = gprCache.AllocWrite( stackOpt, *this );
		// mov [edi+ofs],ebp
		Mov( reg, Ebp );*/
}

void VmJitX86::PopThis()
{
	DontFlush _(*this);
	RegExpr reg = GetPtr(0);
	// mov ebp,[edi+x]
	Mov(Ebp.ToRegPtr(), reg.ToRegPtr());
	Pop(1);
}

void VmJitX86::PCopyCommon(Int count)
{
	Int scount = count / Stack::WORD_SIZE;
	count &= Stack::WORD_SIZE-1;

	if (scount <= 4)
	{
		// unroll
		for (Int i=0; i<scount; i++)
		{
			Mov(Eax.ToRegPtr(), MemPtr(Ecx + Stack::WORD_SIZE*i));
			Mov(MemPtr(Edx + Stack::WORD_SIZE*i), Eax.ToRegPtr());
		}
	}
	else
	{
		Mov(Ebx, scount-1);

		Int label = code.GetSize();

		Mov(Eax.ToRegPtr(), MemPtr(Ecx + Stack::WORD_SIZE*Ebx));
		Mov(MemPtr(Edx + Stack::WORD_SIZE*Ebx), Eax.ToRegPtr());
		Dec(Ebx);
		// jns up
		EmitNew(0x79);
		Emit(label - (code.GetSize()+1));
	}

	// copy the rest...
	Int ofs = Stack::WORD_SIZE*scount;

	if (count >= 4)
	{
		LETHE_ASSERT(IsX64);
		Mov(Eax, Mem32(Ecx + ofs));
		Mov(Mem32(Edx + ofs), Eax);
		count -= 4;
		ofs += 4;
	}

	if (count >= 2)
	{
		Mov(Ax, Mem16(Ecx + ofs));
		Mov(Mem16(Edx + ofs), Ax);
		count -= 2;
		ofs += 2;
	}

	if (count)
	{
		Mov(Al, Mem8(Ecx + ofs));
		Mov(Mem8(Edx + ofs), Al);
	}
}

void VmJitX86::PCopyLocal(Int ofs0, Int ofs1, Int count)
{
	if (!count)
		return;

	// we have to flush here because of regs
	FlushStackOpt();

	// okay: get ptrs...
	// free regs: eax, ebx, ecx, edx

	Int scount = count / Stack::WORD_SIZE;

	if (scount <= 4)
	{
		count &= Stack::WORD_SIZE-1;

		Int srcOfs = (ofs0 + stackOpt) * Stack::WORD_SIZE;
		Int dstOfs = (ofs1 + stackOpt) * Stack::WORD_SIZE;

		// unroll
		for (Int i=0; i<scount; i++)
		{
			Mov(Eax.ToRegPtr(), MemPtr(Edi + Stack::WORD_SIZE*i+srcOfs));
			Mov(MemPtr(Edi + Stack::WORD_SIZE*i+dstOfs), Eax.ToRegPtr());
		}

		srcOfs += scount*Stack::WORD_SIZE;
		dstOfs += scount*Stack::WORD_SIZE;

		if (count >= 4)
		{
			LETHE_ASSERT(IsX64);
			Mov(Eax, Mem32(Edi + srcOfs));
			Mov(Mem32(Edi + dstOfs), Eax);
			count -= 4;
			srcOfs += 4;
			dstOfs += 4;
		}

		if (count >= 2)
		{
			Mov(Ax, Mem16(Edi + srcOfs));
			Mov(Mem16(Edi + dstOfs), Ax);
			count -= 2;
			srcOfs += 2;
			dstOfs += 2;
		}

		if (count)
		{
			Mov(Al, Mem8(Edi + srcOfs));
			Mov(Mem8(Edi + dstOfs), Al);
		}

		return;
	}


	// load dst
	Lea(Edx.ToRegPtr(), MemPtr(Edi + (stackOpt + ofs1)*Stack::WORD_SIZE));
	// load src
	Lea(Ecx.ToRegPtr(), MemPtr(Edi + (stackOpt + ofs0)*Stack::WORD_SIZE));

	PCopyCommon(count);
}

void VmJitX86::PCopy(Int count, bool reverse)
{
	if (!count)
		return;

	// we have to flush here because of regs
	FlushStackOpt();

	// okay: get ptrs...
	// free regs: eax, ebx, ecx, edx

	// load dst
	Mov(Edx.ToRegPtr(), MemPtr(Edi + (stackOpt + reverse)*Stack::WORD_SIZE));
	// load src
	Mov(Ecx.ToRegPtr(), MemPtr(Edi + (stackOpt + 1 - reverse)*Stack::WORD_SIZE));

	PCopyCommon(count);
}

void VmJitX86::PSwap(Int count)
{
	if (!count)
		return;

	// we have to flush here because of regs
	FlushStackOpt();

	// okay: get ptrs...
	// free regs: eax, ebx, ecx, edx

	Int scount = count / Stack::WORD_SIZE;
	count &= (Stack::WORD_SIZE - 1);

	// load dst
	Mov(Edx.ToRegPtr(), MemPtr(Edi + (stackOpt)*Stack::WORD_SIZE));
	// load src
	Mov(Ecx.ToRegPtr(), MemPtr(Edi + (stackOpt + 1)*Stack::WORD_SIZE));

	if (scount <= 4)
	{
		// unroll
		for (Int i = 0; i < scount; i++)
		{
			Mov(Eax.ToRegPtr(), MemPtr(Ecx + Stack::WORD_SIZE*i));
			Mov(Ebx.ToRegPtr(), MemPtr(Edx + Stack::WORD_SIZE*i));
			Mov(MemPtr(Ecx + Stack::WORD_SIZE*i), Ebx.ToRegPtr());
			Mov(MemPtr(Edx + Stack::WORD_SIZE*i), Eax.ToRegPtr());
		}
	}
	else
	{
		RPush(Edi);
		Mov(Ebx, scount - 1);

		Int label = code.GetSize();

		Mov(Eax.ToRegPtr(), MemPtr(Ecx + Stack::WORD_SIZE*Ebx));
		Mov(Edi.ToRegPtr(), MemPtr(Edx + Stack::WORD_SIZE*Ebx));
		Mov(MemPtr(Ecx + Stack::WORD_SIZE*Ebx), Edi.ToRegPtr());
		Mov(MemPtr(Edx + Stack::WORD_SIZE*Ebx), Eax.ToRegPtr());
		Dec(Ebx);
		// jns up
		EmitNew(0x79);
		Emit(label - (code.GetSize() + 1));
		RPop(Edi);
	}

	// copy the rest...
	Int ofs = Stack::WORD_SIZE*scount;

	if (count >= 4)
	{
		LETHE_ASSERT(IsX64);
		Mov(Eax, Mem32(Ecx + ofs));
		Mov(Ebx, Mem32(Edx + ofs));
		Mov(Mem32(Ecx + ofs), Ebx);
		Mov(Mem32(Edx + ofs), Eax);
		count -= 4;
		ofs += 4;
	}

	if (count >= 2)
	{
		Mov(Ax, Mem16(Ecx + ofs));
		Mov(Bx, Mem16(Edx + ofs));
		Mov(Mem16(Ecx + ofs), Bx);
		Mov(Mem16(Edx + ofs), Ax);
		count -= 2;
		ofs += 2;
	}

	if (count)
	{
		Mov(Al, Mem8(Ecx + ofs));
		Mov(Bl, Mem8(Edx + ofs));
		Mov(Mem8(Ecx + ofs), Bl);
		Mov(Mem8(Edx + ofs), Al);
	}
}

void VmJitX86::AddFixup(Int adr, Int pc, Byte relative)
{
	Fixup fixup;
	fixup.codeOfs  = adr;
	fixup.byteOfs  = pc;
	fixup.relative = relative;

	fixups.Add(fixup);
}

void VmJitX86::PushFuncPtr(Int pc)
{
	// needs late resolve...
	PushPtr();
	Int adr = code.GetSize() - sizeof(void *);
	AddFixup(adr, pc, 0);
}

void VmJitX86::SwitchTable(const CompiledProgram &prog, Int range, Int pc)
{
	// this one is very tricky!
	// mov eax,last_expr_reg
	// cmp eax, range
	// jae => ofs1
	// jmp [4*eax + blah]

	auto reg = gprCache.Alloc(stackOpt, *this, 1);

	Pop(1);

	Cmp(reg, range);
	const Int *table = prog.instructions.GetData()+pc+1;
	EmitJump(COND_UGE, pc+1+table[0]);

	if (IsX64)
	{
		Emit(0x41 + 2*((reg.base & 8) != 0));
		Emit(0xff);
		Emit(0xa4);
		Emit(0xc5 + ((reg.base & 7)*8));
	}
	else
	{
		Emit(0xff);
		Emit(0x24);
		Emit(0x85 + ((reg.base & 7)*8));
		AddFixup(code.GetSize(), -2, 0);
	}

	const Int mask = Stack::WORD_SIZE-1;
	Int align = code.GetSize() & mask;
	align = (mask+1 - align) & mask;
	Emit32(IsX64 ? code.GetSize() + align + 4 : align);

	FlushStackOpt();

	for (Int i=0; i<align; i++)
		Emit(0x90);

	for (Int i=0; i<range; i++)
	{
		AddFixup(code.GetSize(), pc+1+table[1+i], 0);

		if (IsX64)
			Emit64(0);
		else
			Emit32(0);
	}
}

void VmJitX86::RangeCheck()
{
	DontFlush _(*this);
	RegExpr reg = FindGpr(stackOpt, 0);
	RegExpr reg2 = FindGpr(stackOpt+1, 0);
	// cmp eax,const
	Cmp(reg, reg2);
	// jb +1
	EmitNew(0x72);
	Emit(0x01);
	// int 3
	Int3();
	// we now have to move and pop!
	Mov(reg2, reg);
	Pop(1);
}

void VmJitX86::RangeCheck(Int val)
{
	DontFlush _(*this);
	RegExpr reg = FindGpr(stackOpt, 0);
	// cmp eax,const
	Cmp(reg, val);
	// jb +1
	EmitNew(0x72);
	Emit(0x01);
	// int 3
	Int3();
}

bool VmJitX86::WouldFlush() const
{
	return stackOpt || WouldFlushLastAdr() || gprCache.WouldFlush() || sseCache.WouldFlush();
}

void VmJitX86::FlushStackOpt(bool soft)
{
	// TODO: investigate why this, despite saving bytes and reducing instruction count,
	// runs slower in bubbletest! (pipeline/superscalar?)
	//soft = false;

	if (!dontFlush)
	{
		DontFlush _(*this);
		FlushLastAdr();
		gprCache.Flush(*this, soft, stackOpt);
		sseCache.Flush(*this, soft, stackOpt);
	}

	if (dontFlush || !stackOpt)
		return;

	if (stackOpt < 0)
		PushGen(-stackOpt, 0);
	else
		PopGen(stackOpt);

	stackOpt = 0;
}

void VmJitX86::DoFixups(CompiledProgram &prog, Int pass)
{
	for (Int i=0; i<fixups.GetSize(); i++)
	{
		const Fixup &f = fixups[i];

		if (f.byteOfs < 0)
		{
			// fixup native call, we have abs ptr
			UInt absAdr = Endian::ReadUInt(code.GetData() + f.codeOfs);
			UInt delta = (UInt)(UIntPtr)(code.GetData() + f.codeOfs + 4);

			if (f.byteOfs == -2)
				absAdr += delta;
			else
				absAdr -= delta;

			Endian::WriteUInt(code.GetData() + f.codeOfs, absAdr);
			continue;
		}

		if (f.relative)
		{
			if (f.relative & 2)
			{
				UInt relAdr = (UInt)(UIntPtr)(code.GetData() + pcToCode[f.byteOfs]);
				UInt targAdr = (UInt)(UIntPtr)(code.GetData() + f.codeOfs + 1);
				auto delta = relAdr - targAdr;
				// unfortunately this can happen with loop alignment now, sigh...
				LETHE_RUNTIME_ASSERT((Int)delta == (SByte)(Byte)delta);

				code[f.codeOfs] = Byte(delta);
			}
			else
			{
				UInt relAdr = (UInt)(UIntPtr)(code.GetData() + pcToCode[f.byteOfs]);
				UInt targAdr = (UInt)(UIntPtr)(code.GetData() + f.codeOfs + 4);
				Endian::WriteUInt(code.GetData() + f.codeOfs, relAdr - targAdr);
			}
		}
		else
		{
			// absolute... (funptr)
			UIntPtr absAdr = (UIntPtr)(code.GetData() + pcToCode[f.byteOfs]);
			Endian::WriteUIntPtr(code.GetData() + f.codeOfs, absAdr);
		}
	}

	if (pass == 0)
	{
		// last thing to do: recompute vtbls
		prog.FixupVtblJit(pcToCode, code.GetData());
	}
}

bool VmJitX86::WouldFlushLastAdr() const
{
	return lastAdr != INVALID_STACK_INDEX;
}

void VmJitX86::FlushLastAdr()
{
	if (lastAdr != INVALID_STACK_INDEX)
	{
		DontFlush _(*this);
		RegExpr reg = AllocGprWritePtr(lastAdr);

		lastAdr = INVALID_STACK_INDEX;
		Lea(reg, Mem32(lastAdrExpr));
	}
}

void VmJitX86::SetLastAdr(const RegExpr &re)
{
	FlushLastAdr();
	lastAdr = stackOpt;
	lastAdrExpr = re;
}

void VmJitX86::AddStrong()
{
	DontFlush _(*this);
	auto reg = GetPtr(0);
	PushInt(1);
	auto sreg = GetInt(0);

	Test(reg, reg);
	EmitNew(0x74);
	Int jmp = code.GetSize();
	Emit(0);
	// lock xadd dword [reg+ofs], sreg
	XAdd(Mem32(reg + BaseObject::OFS_REFC), sreg);
	code[jmp] = Byte(code.GetSize() - jmp - 1);

	Pop(1);
}

bool VmJitX86::GetJitCode(const Byte *&ptr, Int &size)
{
	ptr = code.GetData();
	size = code.GetSize();
	return size != 0;
}

void VmJitX86::BuildFuncOffsets(const CompiledProgram &prog)
{
	funcOfs.Reserve(prog.funcMap.GetSize() + 1);
	funcOfs.Clear();

	for (auto &&ci : prog.funcMap)
		funcOfs.Add(ci.key);

	funcOfs.Add(prog.instructions.GetSize());

	funcOfs.Sort();
}

#else

VmJitX86::VmJitX86()
{
}

const void *VmJitX86::GetCodePtr(Int) const
{
	return nullptr;
}

bool VmJitX86::GetJitCode(const Byte *&ptr, Int &size)
{
	ptr = nullptr;
	size = 0;
	return false;
}

bool VmJitX86::CodeGen(CompiledProgram &)
{
	return false;
}

ExecResult VmJitX86::ExecScriptFunc(Vm &, Int)
{
	return EXEC_NO_JIT;
}

ExecResult VmJitX86::ExecScriptFuncPtr(Vm &, const void *)
{
	return EXEC_NO_JIT;
}

Int VmJitX86::FindFunctionPC(const void *) const
{
	return -1;
}

void VmJitX86::FlushStackOpt(bool)
{
}

#endif

}
