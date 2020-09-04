#include "AsmX86.h"

namespace lethe
{

// RegExpr

RegExpr RegExpr::ToRegPtr() const
{
#if LETHE_32BIT
	return ToReg32();
#else
	return ToReg64();
#endif
}

RegExpr RegExpr::ToReg64() const
{
	RegExpr res(*this);

	if (base < XMM0)
		res.base = (GprEnum)((base & 15) + 48);

	return res;
}

RegExpr RegExpr::ToReg32() const
{
	RegExpr res(*this);

	if (base < XMM0)
		res.base = (GprEnum)(base & 15);

	return res;
}

RegExpr RegExpr::ToReg8() const
{
	RegExpr res(*this);
	LETHE_ASSERT(base < XMM0);

	if (base >= RAX)
		res.base = (GprEnum)(base - 16);
	else
		res.base = (GprEnum)(base + 32);

	return res;
}

RegExpr RegExpr::ToReg16() const
{
	RegExpr res(*this);
	LETHE_ASSERT(base < XMM0);

	if (base >= RAX)
		res.base = (GprEnum)(base - 32);
	else
		res.base = (GprEnum)(base + 16);

	return res;
}

Int RegExpr::GetRegMask() const
{
	Int res = 0;

	if (base != NoRegister)
		res |= 1 << (base & 15);

	if (index != NoRegister)
		res |= 1 << (index & 15);

	return res;
}

bool RegExpr::Add(const RegExpr &o)
{
	if (o.index == NoRegister)
	{
		if (index == NoRegister)
		{
			index = o.base;
			offset += o.offset;
			return true;
		}

		LETHE_RET_FALSE(base == NoRegister);
		base = o.base;
		offset += o.offset;
		return true;
	}

	RegExpr tmp = o;
	LETHE_RET_FALSE(tmp.Add(*this));
	*this = tmp;
	return true;
}

// Reg defs

#define VX86_DEF_REG(x, y) \
	const AsmX86::x##_ AsmX86::x;

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

// instructions

void AsmX86::EmitModRm(const RegExpr &dst, const RegExpr &src, Int modshift)
{
	EmitModRmDirect(dst.base, src, modshift);
}

Int AsmX86::GenSIB(const RegExpr &re, bool fulldisp)
{
	GprEnum b = re.base, i = re.index;
	Int s = re.scale;

	// optimize i*2 => i+i
	if (!fulldisp && s == 2 && b == NoRegister)
	{
		b = i;
		s = 1;
	}

	Int res = 0;

	switch (s)
	{
	case 2:
		res = 1 << 6;
		break;

	case 4:
		res = 2 << 6;
		break;

	case 8:
		res = 3 << 6;
	}

	if (i == ESP)
	{
		LETHE_ASSERT(0 && "index cannot be esp");
		return -1;
	}

	if (IsX64 && b != NoRegister && (b & 8))
		code[lastRex] |= 1;

	if (IsX64 && i != NoRegister && (i & 8))
		code[lastRex] |= 2;

	// beware of EBP base, depends on MOD!
	if (b == NoRegister)
		b = EBP;
	else
		b = (GprEnum)(b & 7);

	if (i == NoRegister)
		i = ESP;
	else
		i = (GprEnum)(i & 7);

	return res + (i << 3) + b;
}

void AsmX86::EmitModRmDirect(Int val, const RegExpr &src, Int modshift)
{
	// mod: 00 = no disp, 01 = byte disp, 10 = dword disp, 11 = reg
	// prepare mod r/m
	Int modrm = 0;
	Int dshift = modshift;
	bool fulldisp = 0;
	bool useSib = src.index != NoRegister || src.base == ESP;

	if (IsX64 && src.mem != MEM_NONE)
	{
		// force SIB for extended addressing
		useSib |= src.index != NoRegister && !!(src.index & 8);
		useSib |= src.base != NoRegister && !!(src.base & 8);
	}

	if (!src.IsRegister())
	{
		if (src.offset || src.base == EBP)
		{
			if (src.offset >= -128 && src.offset < 128)
				modrm |= 1 << 6;
			else
				modrm |= 2 << 6;
		}

		if (useSib)
		{
			modrm |= ESP;

			if (src.base == NoRegister)
			{
				modrm &= 0x3f;
				fulldisp = 1;
			}
		}
		else
		{
			if (src.base == NoRegister)
			{
				modrm = EBP;
				fulldisp = 1;
			}
			else
			{
				if (IsX64 && (src.base & 8))
					code[lastRex] |= 1;

				modrm |= src.base & 7;
			}
		}

		dshift = 3;
	}
	else
	{
		if (IsX64 && (src.base & 8))
			code[lastRex] |= modshift ? 1 : 4;

		modrm |= 0xc0;
		modrm |= (src.base & 7) << (3 ^ modshift);
	}

	if (IsX64 && (val & 8))
		code[lastRex] |= dshift ? 4 : 1;

	modrm |= (val & 7) << dshift;
	Emit(modrm);

	if (useSib)
	{
		// now if SIB
		Emit(GenSIB(src, fulldisp));
	}

	// emit displacement
	switch(modrm >> 6)
	{
	case 1:
		Emit((Int)src.offset);
		break;

	case 0:
		if (!fulldisp)
			break;
		// fall through
	case 2:
		Emit32((UInt)src.offset);
		break;
	}

	lastRex = -1;
}

void AsmX86::Lea(const RegExpr &dst, const RegExpr &src)
{
	RegExpr tmp = src;
	tmp.mem = MEM_NONE;

	if (tmp.IsRegister())
		Mov(dst.ToRegPtr(), tmp.ToRegPtr());
	else if (tmp.IsImmediate())
		Mov(dst.ToRegPtr(), tmp);
	else
	{
		EmitNew();
		EmitRex(dst.ToRegPtr(), src);
		Emit(0x8d);
		EmitModRm(dst.ToRegPtr(), src);
	}
}

void AsmX86::Movss(const RegExpr &dst, const RegExpr &src)
{
	MovssCommon(dst, src, 0xf3);
}

void AsmX86::Movsd(const RegExpr &dst, const RegExpr &src)
{
	MovssCommon(dst, src, 0xf2);
}

void AsmX86::MovssCommon(const RegExpr &dst, const RegExpr &src, Byte prefix)
{
	EmitNew();
	Emit(prefix);
	EmitRex(dst, src);
	Emit(0x0f);

	if (src.IsRegister())
	{
		Emit(0x11 - dst.IsRegister());
		EmitModRm(src, dst);
	}
	else
	{
		Emit(0x10);
		EmitModRm(dst, src);
	}
}

void AsmX86::AddssLike(const RegExpr &dst, const RegExpr &src, Int opc, Byte prefix)
{
	EmitNew();

	if (prefix)
		Emit(prefix);

	EmitRex(dst, src);

	Emit(0x0f);
	Emit(opc);

	if (src.IsRegister())
		EmitModRm(src, dst);
	else
		EmitModRm(dst, src);
}

void AsmX86::Pxor(const RegExpr &dst, const RegExpr &src)
{
	EmitNew();
	Emit(0x66);
	EmitRex(dst, src);

	Emit(0x0f);
	Emit(0xef);

	if (src.IsRegister())
		EmitModRm(src, dst);
	else
		EmitModRm(dst, src);
}

void AsmX86::Xorps(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x57, 0);
}

void AsmX86::Addsd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x58, 0xf2);
}

void AsmX86::Addss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x58);
}

void AsmX86::Subss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5c);
}

void AsmX86::Subsd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5c, 0xf2);
}

void AsmX86::Mulss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x59);
}

void AsmX86::Mulsd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x59, 0xf2);
}

void AsmX86::Divss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5e);
}

void AsmX86::Divsd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5e, 0xf2);
}

void AsmX86::Cvtsi2ss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2a);
}

void AsmX86::Cvtsi2sd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2a, 0xf2);
}

void AsmX86::Cvttss2si(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2c);
}

void AsmX86::Cvttsd2si(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2c, 0xf2);
}

void AsmX86::Cvtss2sd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5a);
}

void AsmX86::Cvtsd2ss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x5a, 0xf2);
}

void AsmX86::UComiss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2e, 0);
}

void AsmX86::UComisd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2e, 0x66);
}

void AsmX86::Comiss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2f, 0);
}

void AsmX86::Comisd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x2f, 0x66);
}

void AsmX86::Sqrtss(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x51);
}

void AsmX86::Sqrtsd(const RegExpr &dst, const RegExpr &src)
{
	AddssLike(dst, src, 0x51, 0xf2);
}

void AsmX86::Movd(const RegExpr &dst, const RegExpr &src)
{
	MovxCommon(dst, src, 0x66, 0x66, 0x6e, 0x7e);
}

void AsmX86::Movq(const RegExpr &dst, const RegExpr &src)
{
	MovxCommon(dst, src, 0xf3, 0x66, 0x7e, 0xd6);
}

void AsmX86::MovxCommon(const RegExpr &dst, const RegExpr &src, Byte prefix0, Byte prefix1, Byte opc0, Byte opc1)
{
	EmitNew(dst.IsRegister() ? prefix0 : prefix1);

	if (dst.GetSize() == MEM_XMM)
		EmitRex(src, src);
	else
		EmitRex(dst, dst);

	Emit(0xf);

	if (dst.IsRegister())
	{
		if (src.IsRegister())
		{
			if (dst.GetSize() == MEM_XMM)
			{
				Emit(opc0);
				EmitModRm(src, dst);
			}
			else
			{
				Emit(opc1);
				EmitModRm(dst, src);
			}

			return;
		}

		Emit(opc0);
		EmitModRm(dst, src);
	}
	else
	{
		Emit(opc1);
		EmitModRm(src, dst);
	}
}

void AsmX86::Movzx(const RegExpr &dst, const RegExpr &src)
{
	MemSize dregSize = dst.GetSize();
	MemSize sregSize = src.GetSize();

	EmitNew();

	if (dregSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xb6 + (sregSize != MEM_BYTE));

	if (dst.IsRegister() && src.IsRegister())
		EmitModRm(src, dst);
	else
		EmitModRm(dst, src);
}

void AsmX86::Movsxd(const RegExpr &dst, const RegExpr &src)
{
	LETHE_ASSERT(IsX64 && dst.IsRegister() && dst.GetSize() == MEM_QWORD && src.GetSize() == MEM_DWORD);
	EmitNew();
	EmitRex(dst, src);
	Emit(0x63);

	if (dst.IsRegister() && src.IsRegister())
		EmitModRm(src, dst);
	else
		EmitModRm(dst, src);
}

void AsmX86::Movsx(const RegExpr &dst, const RegExpr &src)
{
	MemSize dregSize = dst.GetSize();
	MemSize sregSize = src.GetSize();

	EmitNew();

	if (dregSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xbe + (sregSize != MEM_BYTE));

	if (dst.IsRegister() && src.IsRegister())
		EmitModRm(src, dst);
	else
		EmitModRm(dst, src);
}

namespace
{

const Short Add_Tbl[] =
{
	0x01, 0
};

const Short Sub_Tbl[] =
{
	0x29, 5
};

const Short Or_Tbl[] =
{
	0x09, 1
};

const Short And_Tbl[] =
{
	0x21, 4
};

const Short Xor_Tbl[] =
{
	0x31, 6
};

const Short Cmp_Tbl[] =
{
	0x39, 7
};

}

void AsmX86::AddLike(const RegExpr &dst, const RegExpr &src, const Short *tbl)
{
	MemSize regSize = dst.GetSize();
	Int delta = regSize == MEM_BYTE;
	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	if (dst.IsRegister())
	{
		if (src.IsImmediate())
		{
			Int val = tbl[1];
			bool byteOfs = src.offset >= -128 && src.offset < 128;

			if (!byteOfs && (dst.base == EAX || dst.base == AX))
			{
				Emit(tbl[0]+4);
				EmitImm((UInt)src.offset, regSize);
				return;
			}

			if (dst.base == AL)
			{
				Emit(tbl[0]+3);
				Emit((Int)src.offset);
				return;
			}

			if (IsX64)
			{
				Byte rex = 0;

				if (dst.base & 8)
					rex |= 0x41;

				if (dst.GetSize() == MEM_QWORD)
					rex |= 0x48;

				if (rex)
					Emit(rex);
			}

			switch(regSize)
			{
			case MEM_BYTE:
				Emit(0x80);
				Emit(0xc0 + (val << 3) + (dst.base & 7));
				Emit((Int)src.offset);
				return;

			case MEM_WORD:
				Emit(byteOfs ? 0x83 : 0x81);
				Emit(0xc0 + (val << 3) + (dst.base & 7));

				if (byteOfs)
					Emit((Int)src.offset);
				else
					Emit16((UInt)src.offset);

				return;

			default:
				;
			}

			Emit(byteOfs ? 0x83 : 0x81);
			Emit(0xc0 + (val << 3) + (dst.base & 7));

			if (byteOfs)
				Emit((Int)src.offset);
			else
				Emit32((UInt)src.offset);

			return;
		}

		EmitRex(dst, src);
		Emit((src.IsRegister() ? tbl[0] : tbl[0]+2) - delta);
		EmitModRm(dst, src);
		return;
	}

	EmitRex(dst, src);

	if (src.IsImmediate())
	{
		bool byteOfs = src.offset >= -128 && src.offset < 128;
		Emit((!delta && byteOfs ? 0x83 : 0x81) - delta);
		EmitModRmDirect(tbl[1], dst);

		if (byteOfs)
			regSize = MEM_BYTE;

		EmitImm((UInt)src.offset, regSize);
	}
	else
	{
		// add ?, reg
		Emit(tbl[0] - delta);
		EmitModRm(src, dst);
	}
}

void AsmX86::Add(const RegExpr &dst, const RegExpr &src)
{
	if (src.IsImmediate() && Abs(src.offset) == 1)
	{
		// optimize to inc/dec dst
		if (src.offset < 0)
			Dec(dst);
		else
			Inc(dst);

		return;
	}

	AddLike(dst, src, Add_Tbl);
}

void AsmX86::Sub(const RegExpr &dst, const RegExpr &src)
{
	if (src.IsImmediate() && Abs(src.offset) == 1)
	{
		// optimize to inc/dec dst
		if (src.offset > 0)
			Dec(dst);
		else
			Inc(dst);

		return;
	}

	AddLike(dst, src, Sub_Tbl);
}

void AsmX86::And(const RegExpr &dst, const RegExpr &src)
{
	AddLike(dst, src, And_Tbl);
}

void AsmX86::Or(const RegExpr &dst, const RegExpr &src)
{
	AddLike(dst, src, Or_Tbl);
}

void AsmX86::Xor(const RegExpr &dst, const RegExpr &src)
{
	AddLike(dst, src, Xor_Tbl);
}

void AsmX86::Cmp(const RegExpr &dst, const RegExpr &src)
{
	AddLike(dst, src, Cmp_Tbl);
}

void AsmX86::Xchg(RegExpr dst, RegExpr src)
{
	MemSize regSize = dst.GetSize();
	Int delta = regSize == MEM_BYTE;
	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);

	if (!src.IsRegister())
		Swap(src, dst);

	if (src.IsRegister() && dst.IsRegister())
	{
		if (src.base == EAX)
		{
			if (IsX64 && (dst.base & 8))
				code[lastRex] |= 0x1;

			Emit(0x90 + (dst.base & 7));
			return;
		}

		if (dst.base == EAX)
		{
			if (IsX64 && (src.base & 8))
				code[lastRex] |= 0x1;

			Emit(0x90 + (src.base & 7));
			return;
		}
	}

	Emit(0x87 - delta);
	EmitModRm(src, dst);
}

void AsmX86::Test(const RegExpr &dst, const RegExpr &src)
{
	MemSize regSize = dst.GetSize();
	Int delta = regSize == MEM_BYTE;
	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);

	if (src.IsRegister())
	{
		Emit(0x85 - delta);

		if (dst.IsRegister())
			EmitModRm(dst, src);
		else
			EmitModRm(src, dst);

		return;
	}

	LETHE_ASSERT(src.IsImmediate());

	if (dst.IsRegister())
	{
		if (dst.base == EAX || dst.base == AX)
			Emit(0xa9);
		else if (dst.base == AL)
			Emit(0xa8);
		else
		{
			if (IsX64 && (dst.base & 8))
				code[lastRex] |= 1;

			Emit(0xf7 - delta);
			Emit(0xc0 + (dst.base & 7));
		}

		EmitImm((UInt)src.offset, regSize);
		return;
	}

	Emit(0xf7 - delta);
	EmitModRmDirect(0, dst);
	EmitImm((UInt)src.offset, regSize);
}

static const SByte condOfs[] =
{
	-1,
	4,
	5,
	0xc,
	0xe,
	0xf,
	0xd,
	2,
	6,
	7,
	3,
	0xa,
	0xb
};

void AsmX86::Setxx(Cond cond, const RegExpr &dst)
{
	EmitNew();
	EmitRex(dst);
	Emit(0x0f);
	Emit(0x90 + condOfs[cond]);

	if (dst.IsRegister())
	{
		if (IsX64 && (dst.base & 8))
			code[lastRex] |= 1;

		Emit(0xc0 + (dst.base & 7));
	}
	else
		EmitModRmDirect(0, dst);
}

void AsmX86::RPush(const RegExpr &src)
{
	LETHE_ASSERT(src.IsRegister());
	EmitNew();

	// REX prefix if necessary
	if (src.base & 8)
		Emit(0x41);

	Emit(0x50 + (src.base & 7));
}

void AsmX86::RPop(const RegExpr &dst)
{
	LETHE_ASSERT(dst.IsRegister());
	EmitNew();

	// REX prefix if necessary
	if (dst.base & 8)
		Emit(0x41);

	Emit(0x58 + (dst.base & 7));
}

void AsmX86::Mov(const RegExpr &dst_, const RegExpr &src)
{
	if (forceMovsxd && dst_.GetSize() == MEM_DWORD && src.GetSize() == MEM_DWORD)
	{
		return Movsxd(dst_.ToRegPtr(), src);
	}

	auto dst = dst_;

	if (IsX64 && dst.base >= RAX && dst.base <= R15 && src.IsImmediate() && dst.IsRegister())
	{
		// a simple optimization: when loading an immediate with upper 32 bits clear, just do a 32-bit mov
		// (implies clearing of upper 32 bits in long mode, saves bytes)
		if ((Long)(UInt)src.offset == src.offset)
			dst = dst.ToReg32();
	}

	if ((dst.IsRegister() && dst.base >= XMM0) || (src.IsRegister() && src.base >= XMM0))
	{
		if (dst.GetSize() == MEM_XMM && src.GetSize() == MEM_XMM)
			Movss(dst, src);
		else
			Movd(dst, src);

		return;
	}

	// TODO?: al,[ofs], eax,[ofs], [ofs],al, [ofs]. eax => 0xa0..0xa3
	// => but I use reg-relative globals anyway so it won't help

	MemSize regSize = dst.IsRegister() ? dst.GetSize() : src.GetSize();
	Int delta = regSize == MEM_BYTE;
	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(src, dst);

	if (dst.IsRegister())
	{
		if (src.IsImmediate())
		{
			if (IsX64 && (dst.base & 8))
				code[lastRex] |= 1;

			switch(regSize)
			{
			case MEM_BYTE:
				Emit(0xb0 + (dst.base & 7));
				Emit((Int)src.offset);
				return;

			case MEM_WORD:
				Emit(0xb8 + (dst.base & 7));
				Emit16((UInt)src.offset);
				return;

			case MEM_QWORD:
				Emit(0xb8 + (dst.base & 7));
				Emit64(src.offset);
				return;

			default:
				;
			}

			Emit(0xb8 + (dst.base & 7));
			Emit32((UInt)src.offset);
			return;
		}

		Emit((src.IsRegister() ? 0x89 : 0x8b) - delta);
		EmitModRm(dst, src);
		return;
	}

	if (src.IsImmediate())
	{
		Emit(0xc7 - delta);
		EmitModRmDirect(0, dst);
		EmitImm((UInt)src.offset, regSize);
	}
	else
	{
		// mov ?, reg
		Emit(0x89 - delta);
		EmitModRm(src, dst);
	}
}

void AsmX86::Inc(const RegExpr &dst)
{
	MemSize regSize = dst.GetSize();

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst);

	if (!IsX64 && (regSize == MEM_DWORD || regSize == MEM_WORD) && dst.IsRegister())
	{
		Emit(0x40 + (dst.base & 7));
		return;
	}

	if (regSize == MEM_BYTE && dst.IsRegister())
	{
		Emit(0xfe);
		EmitModRmDirect(0, dst, 3);
		return;
	}

	Emit(0xff - (regSize == MEM_BYTE));
	EmitModRmDirect(0, dst, 3*dst.IsRegister());
}

void AsmX86::Dec(const RegExpr &dst)
{
	MemSize regSize = dst.GetSize();

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst);

	if (!IsX64 && (regSize == MEM_DWORD || regSize == MEM_WORD) && dst.IsRegister())
	{
		Emit(0x48 + (dst.base & 7));
		return;
	}

	if (regSize == MEM_BYTE && dst.IsRegister())
	{
		Emit(0xfe);
		EmitModRmDirect(1, dst, 3);
		return;
	}

	Emit(0xff - (regSize == MEM_BYTE));
	EmitModRmDirect(1, dst, 3*dst.IsRegister());
}

void AsmX86::Neg(const RegExpr &dst)
{
	MemSize regSize = dst.GetSize();

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst);

	Emit(0xf7 - (regSize == MEM_BYTE));
	EmitModRmDirect(3, dst, 3);
}

void AsmX86::Not(const RegExpr &dst)
{
	MemSize regSize = dst.GetSize();

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst);

	Emit(0xf7 - (regSize == MEM_BYTE));
	EmitModRmDirect(2, dst, 3);
}

void AsmX86::ShlLike(const RegExpr &dst, const RegExpr &src, Int val)
{
	MemSize regSize = dst.GetSize();
	Int delta = regSize == MEM_BYTE;

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);

	if (src.IsImmediate())
	{
		if (src.offset == 1)
		{
			Emit(0xd1 - delta);
			EmitModRmDirect(val, dst, 3);
		}
		else
		{
			Emit(0xc1 - delta);
			EmitModRmDirect(val, dst, 3);
			Emit((Int)src.offset);
		}

		return;
	}

	LETHE_ASSERT(src.IsRegister() && src.base == CL);
	Emit(0xd3 - delta);
	EmitModRmDirect(val, dst, 3);
}

void AsmX86::Shl(const RegExpr &dst, const RegExpr &src)
{
	if (src.IsImmediate() && src.offset == 1)
		Add(dst, dst);
	else
		ShlLike(dst, src, 4);
}

void AsmX86::Shr(const RegExpr &dst, const RegExpr &src)
{
	ShlLike(dst, src, 5);
}

void AsmX86::Sar(const RegExpr &dst, const RegExpr &src)
{
	ShlLike(dst, src, 7);
}

void AsmX86::DivLike(const RegExpr &src, Int val)
{
	MemSize regSize = src.GetSize();
	Int delta = regSize == MEM_BYTE;

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(src);

	LETHE_ASSERT(!src.IsImmediate());
	Emit(0xf7 - delta);
	EmitModRmDirect(val, src, 3);
}

void AsmX86::IDiv(const RegExpr &src)
{
	DivLike(src, 7);
}

void AsmX86::Div(const RegExpr &src)
{
	DivLike(src, 6);
}

void AsmX86::IMul(const RegExpr &src)
{
	DivLike(src, 5);
}

void AsmX86::Mul(const RegExpr &src)
{
	DivLike(src, 4);
}

void AsmX86::IMul(const RegExpr &dst, const RegExpr &src)
{
	MemSize regSize = src.GetSize();
	LETHE_ASSERT(regSize == MEM_WORD || regSize == MEM_DWORD || regSize == MEM_QWORD);

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);

	Emit(0xf);
	Emit(0xaf);
	EmitModRm(dst, src, 3);
}

void AsmX86::IMul(const RegExpr &dst, const RegExpr &src, Int immed)
{
	MemSize regSize = src.GetSize();
	LETHE_ASSERT(regSize == MEM_WORD || regSize == MEM_DWORD || regSize == MEM_QWORD);

	EmitNew();

	if (regSize == MEM_WORD)
		Emit(0x66);

	EmitRex(dst, src);

	if (immed >= -128 && immed < 128)
	{
		Emit(0x6b);
		EmitModRm(dst, src, 3);
		Emit(immed);
	}
	else
	{
		Emit(0x69);
		EmitModRm(dst, src, 3);
		Emit32(immed);
	}
}

void AsmX86::Cdq()
{
	EmitNew(0x99);
}

void AsmX86::Lahf()
{
	EmitNew(0x9f);
}

void AsmX86::Int3()
{
	EmitNew(0xcc);
}

void AsmX86::Nop()
{
	EmitNew(0x90);
}

void AsmX86::Retn()
{
	EmitNew(0xc3);
}

void AsmX86::Retn(Int pop)
{
	EmitNew(0xc2);
	Emit16(pop);
}

void AsmX86::XAdd(const RegExpr &dst, const RegExpr &src)
{
	EmitNew();
	Emit(0xf0);
	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xc1);
	EmitModRm(src, dst);
}

void AsmX86::Bsf(const RegExpr &dst, const RegExpr &src)
{
	EmitNew();
	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xbc);
	EmitModRm(src, dst);
}

void AsmX86::Bsr(const RegExpr &dst, const RegExpr &src)
{
	EmitNew();
	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xbd);
	EmitModRm(src, dst);
}

void AsmX86::Bswap(const RegExpr &dst)
{
	EmitNew();
	EmitRex(dst, dst);
	Emit(0x0f);
	Emit(0xc8 | (dst.base & 7));
}

// note: SSE 4.1+ only!
void AsmX86::PopCnt(const RegExpr &dst, const RegExpr &src)
{
	EmitNew();
	Emit(0xf3);
	EmitRex(dst, src);
	Emit(0x0f);
	Emit(0xb8);
	EmitModRm(src, dst);
}

void AsmX86::EmitRex(const RegExpr &r0, const RegExpr &r1)
{
	if (IsX64)
	{
		Int rex = 0;
		bool useRex = r0.mem == MEM_QWORD || r1.mem == MEM_QWORD;

		if (r0.mem != MEM_NONE)
			useRex |= ((r0.base | r0.index) & 8) != 0;

		if (r1.mem != MEM_NONE)
			useRex |= ((r1.base | r1.index) & 8) != 0;

		if (r0.IsRegister() && r0.GetSize() != MEM_XMM)
		{
			useRex |= (r0.base & 8) != 0;
			rex |= 8*(r0.GetSize() == MEM_QWORD);
		}

		if (r1.IsRegister() && r1.GetSize() != MEM_XMM)
		{
			useRex |= (r1.base & 8) != 0;
			rex |= 8 * (r1.GetSize() == MEM_QWORD);
		}

		if (useRex || rex)
		{
			if (!rex)
				rex |= 8;

			lastRex = code.GetSize();

			if (r0.GetSize() != MEM_QWORD && r1.GetSize() != MEM_QWORD)
				rex = 0;

			Emit(0x40 | rex);
		}
	}
}

void AsmX86::EmitNew()
{
	FlushStackOpt();
	lastIns = code.GetSize();
	lastRex = -1;
}

void AsmX86::EmitNew(Int b)
{
	EmitNew();
	Emit(b);
}

void AsmX86::EmitNewSoft()
{
	FlushStackOpt(true);
	lastIns = code.GetSize();
	lastRex = -1;
}

void AsmX86::EmitNewSoft(Int b)
{
	EmitNewSoft();
	code.Add((Byte)b);
}

void AsmX86::Emit64(ULong i)
{
	//FlushStackOpt();
	code.Add(i & 255);
	code.Add((i >> 8) & 255);
	code.Add((i >> 16) & 255);
	code.Add((i >> 24) & 255);
	code.Add((i >> 32) & 255);
	code.Add((i >> 40) & 255);
	code.Add((i >> 48) & 255);
	code.Add((i >> 56) & 255);
}

void AsmX86::Emit32(UInt i)
{
	//FlushStackOpt();
	code.Add(i & 255);
	code.Add((i >> 8) & 255);
	code.Add((i >> 16) & 255);
	code.Add((i >> 24) & 255);
}

void AsmX86::Emit16(UInt i)
{
	//FlushStackOpt();
	code.Add(i & 255);
	code.Add((i >> 8) & 255);
}

void AsmX86::EmitImm(UInt i, MemSize sz)
{
	if (sz == MEM_QWORD)
		Emit64(i);
	else if (sz == MEM_DWORD)
		Emit32(i);
	else if (sz == MEM_WORD)
		Emit16(i);
	else
		Emit(i);
}

AsmX86::Cond AsmX86::FlipCond(Cond cond)
{
	static const Cond flipCond[] =
	{
		COND_ALWAYS,
		COND_NZ,
		COND_Z,
		COND_GE,
		COND_GT,
		COND_LE,
		COND_LT,
		COND_UGE,
		COND_UGT,
		COND_ULE,
		COND_ULT,
		COND_NP,
		COND_P
	};
	return flipCond[cond];
}


}
