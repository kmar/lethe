#pragma once

#include "../../Common.h"

#include <Lethe/Core/Collect/HashMap.h>
#include "../JITAllocator.h"

// we don't support JITting for 32-bit non-Windows platforms (due to 16-byte stack alignment requirements)
// might fix this later
#if LETHE_CPU_X86 && (LETHE_OS_WINDOWS || LETHE_64BIT)
#	define LETHE_JIT_X86 1
#endif

namespace lethe
{

// note: R8+, XMM8+ only available in long mode (64-bit)
enum GprEnum
{
	EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, R8D, R9D, R10D, R11D, R12D, R13D, R14D, R15D,
	AX, CX, DX, BX, SP, BP, SI, DI, R8W, R9W, R10W, R11W, R12W, R13W, R14W, R15W,
	// note: when REX is used, AH .. BH is SPL, BPL, SIL, DIL => not used by my JIT
	AL, CL, DL, BL, AH, CH, DH, BH, R8L, R9L, R10L, R11L, R12L, R13L, R14L, R15L,
	RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
	XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
	NoRegister
};

enum MemSize
{
	MEM_NONE,
	MEM_XMM,
	MEM_QWORD,
	MEM_DWORD,
	MEM_WORD,
	MEM_BYTE
};

LETHE_API_BEGIN

struct LETHE_API RegExpr
{
	RegExpr() : base(NoRegister), index(NoRegister), offset(0), scale(1), mem(MEM_NONE)  {}
	RegExpr(GprEnum nreg) : base(nreg), index(NoRegister), offset(0), scale(1), mem(MEM_NONE) {}
	RegExpr(Int ofs) : base(NoRegister), index(NoRegister), offset(ofs), scale(1), mem(MEM_NONE) {}
	RegExpr(UInt ofs) : base(NoRegister), index(NoRegister), offset((Int)ofs), scale(1), mem(MEM_NONE) {}
	RegExpr(Long ofs) : base(NoRegister), index(NoRegister), offset(ofs), scale(1), mem(MEM_NONE) {}
	RegExpr(ULong ofs) : base(NoRegister), index(NoRegister), offset((Long)ofs), scale(1), mem(MEM_NONE) {}

	friend RegExpr operator *(Int a, const RegExpr &b)
	{
		RegExpr res(b);

		if (res.index == NoRegister)
		{
			res.index = res.base;
			res.base = NoRegister;
		}

		res.scale *= a;
		return res;
	}

	friend RegExpr operator *(const RegExpr &b, Int a)
	{
		if (a == 1)
			return b;

		RegExpr res(b);

		if (res.index == NoRegister)
		{
			res.index = res.base;
			res.base = NoRegister;
		}

		res.scale *= a;
		return res;
	}

	friend RegExpr operator +(const RegExpr &a, const RegExpr &b)
	{
		RegExpr res = a;
		LETHE_VERIFY(res.Add(b));
		return res;
	}

	friend RegExpr operator +(const RegExpr &a, Int b)
	{
		RegExpr res = a;
		res.offset += b;
		return res;
	}

	friend RegExpr operator +(Int b, const RegExpr &a)
	{
		RegExpr res = a;
		res.offset += b;
		return res;
	}

	friend RegExpr operator -(const RegExpr &a, Int b)
	{
		RegExpr res = a;
		res.offset -= b;
		return res;
	}

	bool IsValid() const
	{
		if (base == NoRegister && index == NoRegister)
			return scale == 1;

		return (base != NoRegister || index != NoRegister) && (scale == 1 || scale == 2 || scale == 4 || scale == 8);
	}

	bool IsRegister() const
	{
		return index == NoRegister && offset == 0 && scale == 1 && base != NoRegister && mem == MEM_NONE;
	}

	MemSize GetSize() const
	{
		if (mem != MEM_NONE)
			return mem;

		if (base >= XMM0)
			return MEM_XMM;

		if (base >= RAX)
			return MEM_QWORD;

		if (base >= AL)
			return MEM_BYTE;

		if (base >= AX)
			return MEM_WORD;

		return MEM_DWORD;
	}

	bool IsImmediate() const
	{
		return base == NoRegister && index == NoRegister && scale == 1 && mem == MEM_NONE;
	}

	RegExpr ToReg8() const;
	RegExpr ToReg16() const;
	RegExpr ToReg32() const;
	RegExpr ToReg64() const;
	// to pointer-size reg on target platform
	RegExpr ToRegPtr() const;

	GprEnum base, index;
	Long offset;
	Int scale;
	MemSize mem;

	// get used mask (16 bit) to mark as reserved
	Int GetRegMask() const;

protected:
	bool Add(const RegExpr &o);
};

struct Mem8 : public RegExpr
{
	Mem8(const RegExpr &r)
	{
		*static_cast<RegExpr *>(this) = r;
		mem = MEM_BYTE;
	}
};

struct Mem16 : public RegExpr
{
	Mem16(const RegExpr &r)
	{
		*static_cast<RegExpr *>(this) = r;
		mem = MEM_WORD;
	}
};

struct Mem32 : public RegExpr
{
	Mem32(const RegExpr &r)
	{
		*static_cast<RegExpr *>(this) = r;
		mem = MEM_DWORD;
	}
};

struct Mem64 : public RegExpr
{
	Mem64(const RegExpr &r)
	{
		*static_cast<RegExpr *>(this) = r;
		mem = MEM_QWORD;
	}
};

struct MemPtr : public RegExpr
{
	MemPtr(const RegExpr &r, bool isPtr = true)
	{
		(void)isPtr;
		*static_cast<RegExpr *>(this) = r;
#if LETHE_32BIT
		mem = MEM_DWORD;
#else
		mem = isPtr ? MEM_QWORD : MEM_DWORD;
#endif
	}
};

class LETHE_API AsmX86
{
public:

	// machine code buffer
	JITPageAlignedArray<Byte> code;

	enum Cond
	{
		COND_ALWAYS,
		COND_Z,
		COND_NZ,
		COND_LT,
		COND_LE,
		COND_GT,
		COND_GE,
		COND_ULT,
		COND_ULE,
		COND_UGT,
		COND_UGE,
		// special parity
		COND_P,
		COND_NP
	};

	// used to mark new instruction
	void EmitNew();
	void EmitNewSoft();
	void EmitNew(Int b);
	void EmitNewSoft(Int b);

	void EmitRex(const RegExpr &r0, const RegExpr &r1 = RegExpr());

	inline void Emit(Int b)
	{
		// note: since we use EmitNew, no need to FlushOpt here
		//FlushStackOpt();
		code.Add((Byte)b);
	}

	// emit immediate disp
	void Emit64(ULong i);
	void Emit32(UInt i);
	void Emit16(UInt i);
	void EmitImm(UInt i, MemSize sz);

	Int GenSIB(const RegExpr &re, bool fulldisp);
	void EmitModRm(const RegExpr &dst, const RegExpr &src, Int modshift = 0);
	void EmitModRmDirect(Int val, const RegExpr &src, Int modshift = 0);

	void Mov(const RegExpr &dst, const RegExpr &src);
	void Movzx(const RegExpr &dst, const RegExpr &src);
	void Movsx(const RegExpr &dst, const RegExpr &src);
	void Lea(const RegExpr &dst, const RegExpr &src);
	void Movss(const RegExpr &dst, const RegExpr &src);
	void Movsd(const RegExpr &dst, const RegExpr &src);
	void Movd(const RegExpr &dst, const RegExpr &src);
	void Movq(const RegExpr &dst, const RegExpr &src);

	void MovxCommon(const RegExpr &dst, const RegExpr &src, Byte prefix0, Byte prefix1, Byte opc0, Byte opc1);
	void MovssCommon(const RegExpr &dst, const RegExpr &src, Byte prefix);

	void AddssLike(const RegExpr &dst, const RegExpr &src, Int opc, Byte prefix = 0xf3);

	void Addss(const RegExpr &dst, const RegExpr &src);
	void Addsd(const RegExpr &dst, const RegExpr &src);
	void Subss(const RegExpr &dst, const RegExpr &src);
	void Subsd(const RegExpr &dst, const RegExpr &src);
	void Mulss(const RegExpr &dst, const RegExpr &src);
	void Mulsd(const RegExpr &dst, const RegExpr &src);
	void Divss(const RegExpr &dst, const RegExpr &src);
	void Divsd(const RegExpr &dst, const RegExpr &src);
	void UComiss(const RegExpr &dst, const RegExpr &src);
	void UComisd(const RegExpr &dst, const RegExpr &src);
	void Comiss(const RegExpr &dst, const RegExpr &src);
	void Comisd(const RegExpr &dst, const RegExpr &src);
	void Sqrtss(const RegExpr &dst, const RegExpr &src);
	void Sqrtsd(const RegExpr &dst, const RegExpr &src);
	void Cvtsi2ss(const RegExpr &dst, const RegExpr &src);
	void Cvtsi2sd(const RegExpr &dst, const RegExpr &src);
	void Cvttss2si(const RegExpr &dst, const RegExpr &src);
	void Cvttsd2si(const RegExpr &dst, const RegExpr &src);
	void Cvtss2sd(const RegExpr &dst, const RegExpr &src);
	void Cvtsd2ss(const RegExpr &dst, const RegExpr &src);
	void Pxor(const RegExpr &dst, const RegExpr &src);

	void Xorps(const RegExpr &dst, const RegExpr &src);

	void AddLike(const RegExpr &dst, const RegExpr &src, const Short *tbl);
	void Add(const RegExpr &dst, const RegExpr &src);
	void Sub(const RegExpr &dst, const RegExpr &src);
	void And(const RegExpr &dst, const RegExpr &src);
	void Or(const RegExpr &dst, const RegExpr &src);
	void Xor(const RegExpr &dst, const RegExpr &src);
	void Cmp(const RegExpr &dst, const RegExpr &src);
	void Test(const RegExpr &dst, const RegExpr &src);

	void Inc(const RegExpr &dst);
	void Dec(const RegExpr &dst);
	void Neg(const RegExpr &dst);
	void Not(const RegExpr &dst);

	void ShlLike(const RegExpr &dst, const RegExpr &src, Int val);
	void Shl(const RegExpr &dst, const RegExpr &src);
	void Shr(const RegExpr &dst, const RegExpr &src);
	void Sar(const RegExpr &dst, const RegExpr &src);

	void DivLike(const RegExpr &src, Int val);
	void IDiv(const RegExpr &src);
	void Div(const RegExpr &src);
	void IMul(const RegExpr &src);
	void Mul(const RegExpr &src);

	// special variants of imul
	void IMul(const RegExpr &dst, const RegExpr &src);
	void IMul(const RegExpr &dst, const RegExpr &src, Int immed);

	void Cdq();
	void Lahf();
	void Int3();
	void Nop();
	void Retn();
	void Retn(Int pop);

	void Setxx(Cond cond, const RegExpr &dst);

	void RPush(const RegExpr &src);
	void RPop(const RegExpr &dst);

	void Xchg(RegExpr dst, RegExpr src);

	void XAdd(const RegExpr &dst, const RegExpr &src);

	void Bsf(const RegExpr &dst, const RegExpr &src);
	void Bsr(const RegExpr &dst, const RegExpr &src);
	void Bswap(const RegExpr &dst);
	// note: SSE4.1+
	void PopCnt(const RegExpr &dst, const RegExpr &src);

	// 64-bit, 32-bit, 64-bit mode only
	void Movsxd(const RegExpr &dst, const RegExpr &src);

	virtual void FlushStackOpt(bool soft = false)
	{
		(void)soft;
	}

	#define VX86_DEF_REG(x, y) \
		struct x##_ : public RegExpr \
			{ \
			x##_() : RegExpr(y) {} \
		}; \
		static const x##_ x;

	VX86_DEF_REG(Al, AL)
	VX86_DEF_REG(Cl, CL)
	VX86_DEF_REG(Dl, DL)
	VX86_DEF_REG(Bl, BL)
	VX86_DEF_REG(Ah, AH)
	VX86_DEF_REG(Ch, CH)
	VX86_DEF_REG(Dh, DH)
	VX86_DEF_REG(Bh, BH)

	VX86_DEF_REG(R8l, R8L)
	VX86_DEF_REG(R9l, R9L)
	VX86_DEF_REG(R10l, R10L)
	VX86_DEF_REG(R11l, R11L)
	VX86_DEF_REG(R12l, R12L)
	VX86_DEF_REG(R13l, R13L)
	VX86_DEF_REG(R14l, R14L)
	VX86_DEF_REG(R15l, R15L)

	VX86_DEF_REG(Ax, AX)
	VX86_DEF_REG(Cx, CX)
	VX86_DEF_REG(Dx, DX)
	VX86_DEF_REG(Bx, BX)
	VX86_DEF_REG(Sp, SP)
	VX86_DEF_REG(Bp, BP)
	VX86_DEF_REG(Si, SI)
	VX86_DEF_REG(Di, DI)

	VX86_DEF_REG(R8w, R8W)
	VX86_DEF_REG(R9w, R9W)
	VX86_DEF_REG(R10w, R10W)
	VX86_DEF_REG(R11w, R11W)
	VX86_DEF_REG(R12w, R12W)
	VX86_DEF_REG(R13w, R13W)
	VX86_DEF_REG(R14w, R14W)
	VX86_DEF_REG(R15w, R15W)

	VX86_DEF_REG(Eax, EAX)
	VX86_DEF_REG(Ecx, ECX)
	VX86_DEF_REG(Edx, EDX)
	VX86_DEF_REG(Ebx, EBX)
	VX86_DEF_REG(Esp, ESP)
	VX86_DEF_REG(Ebp, EBP)
	VX86_DEF_REG(Esi, ESI)
	VX86_DEF_REG(Edi, EDI)

	VX86_DEF_REG(R8d, R8D)
	VX86_DEF_REG(R9d, R9D)
	VX86_DEF_REG(R10d, R10D)
	VX86_DEF_REG(R11d, R11D)
	VX86_DEF_REG(R12d, R12D)
	VX86_DEF_REG(R13d, R13D)
	VX86_DEF_REG(R14d, R14D)
	VX86_DEF_REG(R15d, R15D)

	VX86_DEF_REG(Rax, RAX)
	VX86_DEF_REG(Rcx, RCX)
	VX86_DEF_REG(Rdx, RDX)
	VX86_DEF_REG(Rbx, RBX)
	VX86_DEF_REG(Rsp, RSP)
	VX86_DEF_REG(Rbp, RBP)
	VX86_DEF_REG(Rsi, RSI)
	VX86_DEF_REG(Rdi, RDI)

	VX86_DEF_REG(R8q, R8)
	VX86_DEF_REG(R9q, R9)
	VX86_DEF_REG(R10q, R10)
	VX86_DEF_REG(R11q, R11)
	VX86_DEF_REG(R12q, R12)
	VX86_DEF_REG(R13q, R13)
	VX86_DEF_REG(R14q, R14)
	VX86_DEF_REG(R15q, R15)

	VX86_DEF_REG(Xmm0, XMM0)
	VX86_DEF_REG(Xmm1, XMM1)
	VX86_DEF_REG(Xmm2, XMM2)
	VX86_DEF_REG(Xmm3, XMM3)
	VX86_DEF_REG(Xmm4, XMM4)
	VX86_DEF_REG(Xmm5, XMM5)
	VX86_DEF_REG(Xmm6, XMM6)
	VX86_DEF_REG(Xmm7, XMM7)

	VX86_DEF_REG(Xmm8, XMM8)
	VX86_DEF_REG(Xmm9, XMM9)
	VX86_DEF_REG(Xmm10, XMM10)
	VX86_DEF_REG(Xmm11, XMM11)
	VX86_DEF_REG(Xmm12, XMM12)
	VX86_DEF_REG(Xmm13, XMM13)
	VX86_DEF_REG(Xmm14, XMM14)
	VX86_DEF_REG(Xmm15, XMM15)

	#undef VX86_DEF_REG

#if LETHE_32BIT
	static constexpr bool IsX64 = false;
#else
	static constexpr bool IsX64 = true;
#endif

	inline void ForceMovsxd(bool mask)
	{
		if (IsX64)
			forceMovsxd = mask;
	}

protected:
	Int lastIns = -1;
	// last REX prefix position (x64 only)
	Int lastRex = -1;
	// force movsxd for next mov
	bool forceMovsxd = false;
};

LETHE_API_END

}
