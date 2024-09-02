#include "CompiledProgram.h"

#include <Lethe/Script/Compiler/Warnings.h>

#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Vm/Stack.h>
#include <Lethe/Script/Vm/Builtin.h>

namespace lethe
{

bool CompiledProgram::CanEncodeI24(Int val) const
{
	Int aval = Abs(val);
	return aval >= 0 && aval <= 0x7fffff;
}

UInt CompiledProgram::GenIntConst(Int iconst)
{
	Int val = iconst;

	if (CanEncodeI24(val))
		return ((UInt)val << 8) + OPC_PUSH_ICONST;

	// must emit to constant pool
	return ((UInt)cpool.Add(val) << 8) + OPC_PUSHC_ICONST;
}

UInt CompiledProgram::GenUIntConst(UInt iconst)
{
	return GenIntConst(*reinterpret_cast<const Int *>(&iconst));
}

UInt CompiledProgram::GenFloatConst(Float fconst)
{
	auto val = fconst;

	if (Abs(val) <= Limits<Int>::Max() && val == (Int)val && CanEncodeI24((Int)val))
	{
		if (GetJitFriendly())
			cpool.Add(val);

		return ((UInt)(Int)val << 8) + OPC_PUSH_FCONST;
	}

	// must emit to constant pool
	return ((UInt)cpool.Add(val) << 8) + OPC_PUSHC_FCONST;
}

UInt CompiledProgram::GenDoubleConst(Double dconst)
{
	auto val = dconst;

	if (Abs(val) <= Limits<Int>::Max() && val == (Int)val && CanEncodeI24((Int)val))
	{
		if (GetJitFriendly())
			cpool.Add(val);

		return ((UInt)(Int)val << 8) + OPC_PUSH_DCONST;
	}

	// must emit to constant pool
	return ((UInt)cpool.Add(val) << 8) + OPC_PUSHC_DCONST;
}

void CompiledProgram::EmitIntConst(Int iconst)
{
	Emit(GenIntConst(iconst));
}

void CompiledProgram::EmitUIntConst(UInt iconst)
{
	Emit(GenUIntConst(iconst));
}

void CompiledProgram::EmitLongConst(Long iconst)
{
	if (iconst >= Limits<Int>::Min() && iconst <= Limits<Int>::Max())
	{
		EmitIntConst((Int)iconst);
		Emit(OPC_PUSH_LCONST);
		return;
	}

	EmitUIntConst(cpool.Add(iconst));
	Emit(OPC_PUSHC_LCONST);
}

void CompiledProgram::EmitULongConst(ULong iconst)
{
	// note: we must not use a faster way using PUSH_LCONST for values above 7ffffffff due to sign extension ofg OPC_PUSH_LCONST
	if (iconst <= (ULong)Limits<Int>::Max())
	{
		EmitUIntConst((UInt)iconst);
		Emit(OPC_PUSH_LCONST);
		return;
	}

	EmitUIntConst(cpool.Add(iconst));
	Emit(OPC_PUSHC_LCONST);
}

void CompiledProgram::EmitFloatConst(Float fconst)
{
	Emit(GenFloatConst(fconst));
}

void CompiledProgram::EmitDoubleConst(Double dconst)
{
	Emit(GenDoubleConst(dconst));
}

void CompiledProgram::EmitNameConst(Name n)
{
	// FIXME: this optimization is unsafe as it prevents bytecode serialization;
	// but I don't plan that anyway...
	UInt ofs = (UInt)cpool.Add(n);
	(void)ofs;
	EmitULongConst(n.GetValue());
#if 0
	UInt idx = n.GetIndex();

	if (idx <= 0x7fffffu)
	{
		EmitI24(OPC_PUSH_ICONST, idx);
	}
	else
	{
		EmitI24(OPC_PUSH_ICONST, ofs);
		Emit(OPC_BCALL + (BUILTIN_LPUSHNAME_CONST << 8));
	}
#endif
}

Int CompiledProgram::EmitForwardJump(UInt ins)
{
	Emit(ins);
	Int fixupTarget = instructions.GetSize()-1;
	// grab fixup handle
	Int handle = flist.Alloc();
	Int nsize = handle+1;

	if (handle >= fixupTargets.GetSize())
		fixupTargets.Resize(nsize);

	fixupTargets[handle] = fixupTarget;
	lastForwardJump = fixupTarget;
	return handle;
}

bool CompiledProgram::EmitBackwardJump(UInt ins, Int target)
{
	Int cur = instructions.GetSize()+1;
	target -= cur;
	LETHE_RET_FALSE(CanEncodeI24(target));
	ins |= (UInt)target << 8;
	Emit(ins);
	return 1;
}

void CompiledProgram::EmitCtor(QDataType qdt)
{
	if (qdt.IsReference())
		return;

	auto idx = qdt.GetType().funCtor;

	if (idx < 0)
		return;

	Emit(OPC_LPUSHADR);
	EmitBackwardJump(OPC_CALL, idx);
	EmitI24(OPC_POP, 1);
}

void CompiledProgram::EmitGlobalCtor(QDataType qdt, Int offset)
{
	if (qdt.IsReference())
		return;

	auto idx = qdt.GetType().funCtor;

	if (idx < 0)
		return;

	EmitI24(OPC_GLOADADR, offset);
	EmitBackwardJump(OPC_CALL, idx);
	EmitI24(OPC_POP, 1);
}

void CompiledProgram::EmitFunc(const String &fnm, AstFunc *fn, const String &typeSignature)
{
	if (!GetInline())
		returnValues.Clear();

	FuncDesc fd;
	fd.adr = instructions.GetSize();
	fd.typeSignature = typeSignature;

	if (fn)
		fn->offset = fd.adr;

	fd.node = fn;
	functions[fnm] = fd;
	funcMap[fd.adr] = functions.GetSize()-1;

	FlushOpt();
}

Int CompiledProgram::GetInsType(Int idx) const
{
	return (Int)(instructions[idx] & 255u);
}

Int CompiledProgram::GetInsImm24(Int idx) const
{
	return instructions[idx] >> 8;
}

struct Fold1
{
	Int opc0;
	Int opcFold;
};

struct Fold2
{
	Int opc0;
	Int opc1;
	Int opcFold;
};

static const Fold2 opcFold2Table[] =
{
	{ OPC_PUSH_ICONST, OPC_IADD, OPC_IADD_ICONST },
	{ OPC_PUSH_FCONST, OPC_FADD, OPC_FADD_ICONST },
	{ OPC_PUSH_ICONST, OPC_IAND, OPC_IAND_ICONST },
	{ OPC_PUSH_ICONST, OPC_IOR, OPC_IOR_ICONST },
	{ OPC_PUSH_ICONST, OPC_IXOR, OPC_IXOR_ICONST },
	{ OPC_PUSH_ICONST, OPC_ISHL, OPC_ISHL_ICONST },
	{ OPC_PUSH_ICONST, OPC_ISHR, OPC_ISHR_ICONST },
	{ OPC_PUSH_ICONST, OPC_ISAR, OPC_ISAR_ICONST },
	{ OPC_PUSH_ICONST, OPC_IMUL, OPC_IMUL_ICONST },
	{ OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold2 opcFold2TableSub[] =
{
	{ OPC_PUSH_ICONST, OPC_ISUB, OPC_IADD_ICONST },
	{ OPC_PUSH_FCONST, OPC_FSUB, OPC_FADD_ICONST },
	{ OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold2 opcFold2TableDblPush[] =
{
	{ OPC_LPUSH32, OPC_PUSH_ICONST, OPC_LPUSH32_ICONST },
	{ OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold2 opcFold2TableDblPushC[] =
{
	{ OPC_LPUSH32, OPC_PUSHC_ICONST, OPC_LPUSH32_CICONST },
	{ OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold2 opcFold2TableMov[] =
{
	{ OPC_LPUSH32, OPC_LSTORE32, OPC_LMOVE32 },
	{ OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold2 opcFold2TablePLoad[] =
{
	{ OPC_PUSH_ICONST, OPC_PLOAD8,   OPC_PLOAD8_IMM   },
	{ OPC_PUSH_ICONST, OPC_PLOAD8U,  OPC_PLOAD8U_IMM  },
	{ OPC_PUSH_ICONST, OPC_PLOAD16,  OPC_PLOAD16_IMM  },
	{ OPC_PUSH_ICONST, OPC_PLOAD16U, OPC_PLOAD16U_IMM },
	{ OPC_PUSH_ICONST, OPC_PLOAD32,  OPC_PLOAD32_IMM  },
	{ OPC_PUSH_ICONST, OPC_PLOAD32F, OPC_PLOAD32F_IMM },
	{ OPC_PUSH_ICONST, OPC_PLOAD64D, OPC_PLOAD64D_IMM },
	{ OPC_PUSH_ICONST, OPC_PLOADPTR, OPC_PLOADPTR_IMM },
	{ OPC_HALT,        OPC_HALT,     OPC_HALT         }
};

static const Fold2 opcFold2TablePXX[] =
{
	{ OPC_AADD_ICONST, OPC_AADD_ICONST,  OPC_AADD_ICONST },

	{ OPC_AADD_ICONST, OPC_PLOAD8_IMM,   OPC_PLOAD8_IMM   },
	{ OPC_AADD_ICONST, OPC_PLOAD8U_IMM,  OPC_PLOAD8U_IMM  },
	{ OPC_AADD_ICONST, OPC_PLOAD16_IMM,  OPC_PLOAD16_IMM  },
	{ OPC_AADD_ICONST, OPC_PLOAD16U_IMM, OPC_PLOAD16U_IMM },
	{ OPC_AADD_ICONST, OPC_PLOAD32_IMM,  OPC_PLOAD32_IMM  },
	{ OPC_AADD_ICONST, OPC_PLOAD32F_IMM, OPC_PLOAD32F_IMM },
	{ OPC_AADD_ICONST, OPC_PLOAD64D_IMM, OPC_PLOAD64D_IMM },
	{ OPC_AADD_ICONST, OPC_PLOADPTR_IMM, OPC_PLOADPTR_IMM },

	{ OPC_AADD_ICONST, OPC_PSTORE8_IMM,   OPC_PSTORE8_IMM   },
	{ OPC_AADD_ICONST, OPC_PSTORE16_IMM,  OPC_PSTORE16_IMM  },
	{ OPC_AADD_ICONST, OPC_PSTORE32_IMM,  OPC_PSTORE32_IMM  },
	{ OPC_AADD_ICONST, OPC_PSTORE32F_IMM, OPC_PSTORE32F_IMM  },
	{ OPC_AADD_ICONST, OPC_PSTORE64D_IMM, OPC_PSTORE64D_IMM  },
	{ OPC_AADD_ICONST, OPC_PSTOREPTR_IMM, OPC_PSTOREPTR_IMM },
	{ OPC_AADD_ICONST, OPC_PSTORE8_IMM_NP,   OPC_PSTORE8_IMM_NP },
	{ OPC_AADD_ICONST, OPC_PSTORE16_IMM_NP,  OPC_PSTORE16_IMM_NP },
	{ OPC_AADD_ICONST, OPC_PSTORE32_IMM_NP,  OPC_PSTORE32_IMM_NP },
	{ OPC_AADD_ICONST, OPC_PSTORE32F_IMM_NP,  OPC_PSTORE32F_IMM_NP },
	{ OPC_AADD_ICONST, OPC_PSTORE64D_IMM_NP,  OPC_PSTORE64D_IMM_NP },
	{ OPC_AADD_ICONST, OPC_PSTOREPTR_IMM_NP, OPC_PSTOREPTR_IMM_NP },
	{ OPC_HALT,        OPC_HALT,     OPC_HALT         }
};

struct Fold3
{
	Int opc0;
	Int opc1;
	Int opc2;
	Int opcFold;
};

static const Fold3 opcFold3Table[] =
{
	{ OPC_LPUSH32, OPC_IADD_ICONST, OPC_LSTORE32, OPC_LIADD_ICONST },
	{ OPC_LPUSH32, OPC_FADD_ICONST, OPC_LSTORE32, OPC_LFADD_ICONST },
	{ OPC_LPUSH32F, OPC_FADD_ICONST, OPC_LSTORE32F, OPC_LFADD_ICONST },
	{ OPC_HALT, OPC_HALT, OPC_HALT, OPC_HALT }
};

static const Fold3 opcFold3TableSimple[] =
{
	{ OPC_LPUSH32, OPC_IADD, OPC_LSTORE32, OPC_LIADD },
	{ OPC_LPUSH32, OPC_ISUB, OPC_LSTORE32, OPC_LISUB },
	{ OPC_LPUSH32, OPC_FADD, OPC_LSTORE32, OPC_LFADD },
	{ OPC_LPUSH32, OPC_FSUB, OPC_LSTORE32, OPC_LFSUB },
	{ OPC_LPUSH32, OPC_FMUL, OPC_LSTORE32, OPC_LFMUL },
	{ OPC_LPUSH32, OPC_FDIV, OPC_LSTORE32, OPC_LFDIV },
	{ OPC_LPUSH32F, OPC_FADD, OPC_LSTORE32F, OPC_LFADD },
	{ OPC_LPUSH32F, OPC_FSUB, OPC_LSTORE32F, OPC_LFSUB },
	{ OPC_LPUSH32F, OPC_FMUL, OPC_LSTORE32F, OPC_LFMUL },
	{ OPC_LPUSH32F, OPC_FDIV, OPC_LSTORE32F, OPC_LFDIV },
	{ OPC_HALT, OPC_HALT, OPC_HALT, OPC_HALT }
};

void CompiledProgram::EmitI24Zero(Int ins, Int offset)
{
	if (!GetUnsafe() && ((ins & 255) == OPC_PUSH_RAW))
	{
		ins &= ~255;
		ins |= OPC_PUSHZ_RAW;
	}

	EmitI24(ins, offset);
}

void CompiledProgram::EmitU24Zero(Int ins, Int offset)
{
	EmitI24Zero(ins, offset);
}

void CompiledProgram::EmitU24(Int ins, Int data)
{
	EmitI24(ins, data);
}

void CompiledProgram::EmitI24(Int ins, Int offset)
{
	if ((UInt)ins > 255 && (Byte)(ins & 255) == OPC_BCALL)
	{
		// special int64 emulation....
		EmitIntConst(offset);
		Emit(ins);
		return;
	}

	if (Abs(offset) >= (1 << 23))
	{
		// need to fixup stuff...
		Int smallOfs = (Int)((UInt)offset & ((1u << 16) - 1));
		Int hiOfs = (offset - smallOfs) >> 16;

		switch(ins)
		{
		case OPC_GLOAD8:
		case OPC_GLOAD8U:
		case OPC_GLOAD16:
		case OPC_GLOAD16U:
		case OPC_GLOAD32:
		case OPC_GLOAD32F:
		case OPC_GLOADPTR:
			Emit(OPC_GLOADADR + ((UInt)smallOfs << 8));
			Emit(OPC_AADDH_ICONST + ((UInt)hiOfs << 8));
			// note: requires same sequence
			Emit(OPC_PLOAD8_IMM + (ins - OPC_GLOAD8));
			return;

		case OPC_GSTORE8:
		case OPC_GSTORE16:
		case OPC_GSTORE32:
		case OPC_GSTORE32F:
		case OPC_GSTOREPTR:
		case OPC_GSTORE8_NP:
		case OPC_GSTORE16_NP:
		case OPC_GSTORE32_NP:
		case OPC_GSTORE32F_NP:
		case OPC_GSTOREPTR_NP:
			Emit(OPC_GLOADADR + ((UInt)smallOfs << 8));
			Emit(OPC_AADDH_ICONST + ((UInt)hiOfs << 8));
			// note: requires same sequence
			Emit(OPC_PSTORE8_IMM + (ins - OPC_GSTORE8));
			return;

		case OPC_GLOADADR:
			Emit(OPC_GLOADADR + ((UInt)smallOfs << 8));
			Emit(OPC_AADDH_ICONST + ((UInt)hiOfs << 8));
			return;
		}
	}

	Emit((UInt)ins + ((UInt)offset << 8));
}

void CompiledProgram::Emit(UInt ins)
{
	auto oldInsSize = instructions.GetSize();

	EmitIns(ins);

	auto isz = instructions.GetSize();

	if (isz > 0 && emitOptBase <= isz-2 && (instructions.Back() & 255) == OPC_AADD_ICONST && (instructions[isz-2] & 255) == OPC_LPUSHADR)
	{
		// fold lpushadr + AADD_ICONST to lpushadr if possible
		auto value = (Int)instructions.Back() >> 8;

		if (value >= 0 && !((UInt)value % (UInt)Stack::WORD_SIZE))
		{
			value /= Stack::WORD_SIZE;
			value += (Int)instructions[isz-2] >> 8;

			if (CanEncodeI24(value))
			{
				instructions[isz-2] = OPC_LPUSHADR + (value << 8);
				instructions.Pop();
				--isz;
			}
		}
	}

	if (isz <= oldInsSize && !codeToLine.IsEmpty())
	{
		// we may need to fixup codeToLine!
		auto &cl = codeToLine.Back();

		if (cl.pc >= instructions.GetSize())
		{
			Int fixpc = instructions.GetSize()-1;

			// paranoid...
			if (codeToLine.GetSize() > 1 && codeToLine[codeToLine.GetSize()-2].pc >= fixpc)
				return;

			cl.pc = fixpc;
		}
	}
}

void CompiledProgram::EmitIns(UInt ins)
{
	// optimization to speed up arrayref=>bool conversion
	if (ins == OPC_POP + 256 && emitOptBase <= instructions.GetSize()-1)
	{
		auto oins = instructions.Back();
		auto itype = oins & 255;

		if (itype == OPC_LPUSHPTR || itype == OPC_GLOADPTR)
		{
			// squash push + pop1 => nothing
			instructions.Pop();
			return;
		}

		if (itype == OPC_PLOADPTR_IMM && emitOptBase <= instructions.GetSize()-2)
		{
			auto itype2 = instructions[instructions.GetSize()-2] & 255;

			if (itype2 == OPC_PUSHTHIS_TEMP || itype2 == OPC_PUSHTHIS)
			{
				// squash pushthis_adr + ploadptr_imm => nothing
				instructions.Pop();
				instructions.Pop();
				return;
			}
		}
	}

	EmitInternal(ins);

	Int num = instructions.GetSize();

	// always fold LPUSH32 + AADD => LAADD
	// note: this must be the last optimization
	if (num > 1 && emitOptBase <= num - 2)
	{
		Int ins0 = GetInsType(num - 2);
		Int ins1 = GetInsType(num - 1);

		// get rid of nops
		switch(instructions[num-1])
		{
		case OPC_IOR_ICONST:
		case OPC_IXOR_ICONST:
		case OPC_IADD_ICONST:
		case OPC_ISAR_ICONST:
		case OPC_ISHR_ICONST:
		case OPC_ISHL_ICONST:
		case OPC_LMOVE32:
		case OPC_LMOVEPTR:
		case OPC_IMUL_ICONST + (1 << 8):
			instructions.Pop();
			return;
		}

		if (ins0 == OPC_LPUSH32 && ins1 == OPC_AADD)
		{
			Int i0 = instructions[num - 2];
			Int i1 = instructions[num - 1];
			i0 >>= 8;
			i1 >>= 8;

			if (i0 >= 0 && i0 < 65536 && i1 >= 0 && i1 < 256)
			{
				// fold!
				*reinterpret_cast<Int *>(&instructions[num - 2]) = (UInt)OPC_LAADD + ((UInt)i1 << 8) + ((UInt)i0 << 16);
				instructions.Pop();
				return;
			}
		}

		// fold double iadd_iconst
		if (ins0 == OPC_IADD_ICONST && ins1 == OPC_IADD_ICONST)
		{
			Int i0 = instructions[num - 2];
			Int i1 = instructions[num - 1];
			i0 >>= 8;
			i1 >>= 8;
			i0 += i1;

			if (CanEncodeI24(i0))
			{
				// fold!
				*reinterpret_cast<Int *>(&instructions[num - 2]) = (UInt)OPC_IADD_ICONST + ((UInt)i0 << 8);
				instructions.Pop();
				return;
			}
		}

		// fold OPC_PUSH_ICONST 0 + IBEQ(NE) => IB(N)Z_P
		if (instructions[num-2] == OPC_PUSH_ICONST)
		{
			auto &i1 = instructions[num - 2];

			// range-validate (assuming 2's completed: FIXME: better, too cryptic)
			if ((i1 >> 8) != (1 << 24)-1 && (ins1 == OPC_IBEQ || ins1 == OPC_IBNE))
			{
				i1 = instructions[num - 1];
				i1 += 256;
				i1 &= ~255;
				i1 += ins1 == OPC_IBEQ ? OPC_IBZ_P : OPC_IBNZ_P;
				instructions.Pop();
				return;
			}
		}

		// fold iadd_iconst + aadd => aadd, aadd_iconst (helps JIT)
		if (jitFriendly && ins0 == OPC_IADD_ICONST && ins1 == OPC_AADD)
		{
			auto aaddMul = instructions[num-1] >> 8;
			auto iadd = instructions[num-2];

			if (CanEncodeI24((iadd >> 8) * aaddMul))
			{
				instructions[num-2] = instructions[num-1];
				iadd &= ~255;
				iadd *= aaddMul;
				iadd += OPC_AADD_ICONST;
				instructions[num-1] = iadd;
				return;
			}
		}
	}

	// fold LPUSHADR + PLOADxx_IMM/PSTORExx_IM
	if (num > 1 && emitOptBase <= num - 2)
	{
		auto i0 = instructions[num - 2];
		auto i1 = instructions[num - 1];

		Int targetOp = OPC_HALT;

		if ((i0 & 255) == OPC_LPUSHADR)
		{
			switch(i1 & 255)
			{
			case OPC_PLOAD32_IMM:
				targetOp = OPC_LPUSH32;
				break;

			case OPC_PLOAD32F_IMM:
				targetOp = OPC_LPUSH32F;
				break;

			case OPC_PLOAD64D_IMM:
				targetOp = OPC_LPUSH64D;
				break;

			case OPC_PLOADPTR_IMM:
				targetOp = OPC_LPUSHPTR;
				break;

			case OPC_PSTORE32_IMM:
				targetOp = OPC_LSTORE32;
				break;

			case OPC_PSTORE32_IMM_NP:
				targetOp = OPC_LSTORE32_NP;
				break;

			case OPC_PSTORE32F_IMM:
				targetOp = OPC_LSTORE32F;
				break;

			case OPC_PSTORE32F_IMM_NP:
				targetOp = OPC_LSTORE32F_NP;
				break;

			case OPC_PSTORE64D_IMM:
				targetOp = OPC_LSTORE64D;
				break;

			case OPC_PSTORE64D_IMM_NP:
				targetOp = OPC_LSTORE64D_NP;
				break;

			case OPC_PSTOREPTR_IMM:
				targetOp = OPC_LSTOREPTR;
				break;

			case OPC_PSTOREPTR_IMM_NP:
				targetOp = OPC_LSTOREPTR_NP;
				break;

			default:;
			}

			if (targetOp != OPC_HALT)
			{
				auto ofs = i1 >> 8;

				if (!(ofs & (Stack::WORD_SIZE-1)))
				{
					auto newOfs = (i0 >> 8) + (ofs / Stack::WORD_SIZE);

					if (CanEncodeI24(newOfs))
					{
						instructions.Pop();
						instructions.Back() = targetOp + 256*newOfs;
					}
				}
			}
		}
	}
}

// FIXME: opt_problem: forward jumps and backward jumps?
// maybe best would be allow unoptimized emits; where emit cannot optimize across...
// solution: simply remember pc_start for emit opts (because we only care about forward jumps)
void CompiledProgram::EmitInternal(UInt ins)
{
	Int num = instructions.GetSize();
	UInt opc = ins & 255u;

	if (!jitFriendly)
	{
		Int rewrite = -1;

		switch(opc)
		{
		case OPC_LPUSH32F:
		case OPC_GLOAD32F:
		case OPC_GSTORE32F:
		case OPC_GSTORE32F_NP:
		case OPC_LSTORE32F:
		case OPC_LSTORE32F_NP:
		case OPC_PLOAD32F:
		case OPC_PLOAD32F_IMM:
		case OPC_PSTORE32F_IMM:
		case OPC_PSTORE32F_IMM_NP:
			rewrite = (Int)opc-1;
			break;
		}

		if (rewrite >= 0)
		{
			opc = rewrite;
			ins &= ~255u;
			ins += rewrite;
		}
	}

	switch(opc)
	{
	case OPC_RET:

		// optimize POP+RET => RET
		if (num > 0 && emitOptBase <= num-1)
		{
			Int &pins = instructions.Back();
			UInt popc = (UInt)pins & 255u;

			if (popc == OPC_POP)
			{
				// internally chain
				pins &= ~255u;
				pins |= opc;

				if (num > 1 && emitOptBase <= num-2)
				{
					// try to drop two returns in a row
					UInt prev = instructions[instructions.GetSize()-2];

					if ((prev & 255u) == OPC_RET)
						instructions.Pop();
				}

				return;
			}

			// remove double ret
			if (popc == OPC_RET)
				return;
		}

		break;

	case OPC_PUSH_RAW:
	case OPC_PUSHZ_RAW:
	case OPC_POP:

		// optimize sequence of PUSH_RAW/PUSHZ_RAW/POP
		if (num > 0 && emitOptBase <= num-1)
		{
			Int &pins = instructions.Back();

			if (((UInt)pins & 255u) == opc)
			{
				// internally chain
				UInt amt = ins & 0xffffff00u;
				pins += amt;
				return;
			}
		}

		break;

	case OPC_AADD:

		// optimize PUSH_ICONST + AADD => AADD_ICONST
		if (num > 0 && emitOptBase <= num-1)
		{
			Int &pins = instructions.Back();

			if (((UInt)pins & 255u) == OPC_PUSH_ICONST)
			{
				Int const0 = pins >> 8;
				Int const1 = *reinterpret_cast<const Int *>(&ins) >> 8;
				const0 *= const1;

				if (CanEncodeI24(const0))
				{
					ins = ((UInt)const0 << 8) + OPC_AADD_ICONST;
					num--;
					instructions.Pop();

					if (!const0)
						return;
				}
			}
		}

		break;

	case OPC_IBZ_P:
	case OPC_IBNZ_P:

		// fold ICMPxx + IB(N)Z_P => flip_cond
		// FIXME: universal solution
		if (num > 0 && emitOptBase <= num-1)
		{
			bool flip = opc == OPC_IBZ_P;
			Int optIns = OPC_HALT;

			switch(GetInsType(num-1))
			{
			case OPC_ICMPZ:
				optIns = flip ? OPC_IBNZ_P : OPC_IBZ_P;
				break;

			case OPC_ICMPNE:
				flip = !flip;
				// fall through
			case OPC_ICMPEQ:
				optIns = flip ? OPC_IBNE : OPC_IBEQ;
				break;

			case OPC_ICMPGE:
				flip = !flip;
				// fall through
			case OPC_ICMPLT:
				optIns = flip ? OPC_IBGE : OPC_IBLT;
				break;

			case OPC_ICMPLE:
				flip = !flip;
				// fall through
			case OPC_ICMPGT:
				optIns = flip ? OPC_IBLE : OPC_IBGT;
				break;

			case OPC_UICMPGE:
				flip = !flip;
				// fall through
			case OPC_UICMPLT:
				optIns = flip ? OPC_UIBGE : OPC_UIBLT;
				break;

			case OPC_UICMPLE:
				flip = !flip;
				// fall through
			case OPC_UICMPGT:
				optIns = flip ? OPC_UIBLE : OPC_UIBGT;
				break;

			case OPC_FCMPNE:
				optIns = flip ? OPC_FBEQ : OPC_FBNE;
				break;

			case OPC_FCMPEQ:
				optIns = flip ? OPC_FBNE : OPC_FBEQ;
				break;

			case OPC_FCMPGE:
				optIns = flip ? OPC_FBLT : OPC_FBGE;
				break;

			case OPC_FCMPLT:
				optIns = flip ? OPC_FBGE : OPC_FBLT;
				break;

			case OPC_FCMPLE:
				optIns = flip ? OPC_FBGT : OPC_FBLE;
				break;

			case OPC_FCMPGT:
				optIns = flip ? OPC_FBLE : OPC_FBGT;
				break;

			case OPC_DCMPNE:
				optIns = flip ? OPC_DBEQ : OPC_DBNE;
				break;

			case OPC_DCMPEQ:
				optIns = flip ? OPC_DBNE : OPC_DBEQ;
				break;

			case OPC_DCMPGE:
				optIns = flip ? OPC_DBLT : OPC_DBGE;
				break;

			case OPC_DCMPLT:
				optIns = flip ? OPC_DBGE : OPC_DBLT;
				break;

			case OPC_DCMPLE:
				optIns = flip ? OPC_DBGT : OPC_DBLE;
				break;

			case OPC_DCMPGT:
				optIns = flip ? OPC_DBLE : OPC_DBGT;
				break;
			}

#if !LETHE_FAST_FLOAT
			if (flip && optIns >= OPC_FBEQ)
				break;
#endif
			if (optIns != OPC_HALT)
			{
				Int delta = *reinterpret_cast< const Int * >(&ins) >> 8;

				delta++;

				Int &pins = instructions[num-1];
				pins = Int(optIns + ((UInt)delta << 8));
				return;
			}
		}

		break;

	case OPC_ICMPNZ_BZ:
	case OPC_ICMPNZ_BNZ:
		// optimize icmpz + icmpnz_bz => ibz and similar
		if (!CanOptPrevious() || !IsConvToBool(instructions.Back()))
			break;

		ins &= ~0xff;
		ins |= opc == OPC_ICMPNZ_BZ ? OPC_IBZ : OPC_IBNZ;
		break;

	case OPC_LSTORE32:
		for (const Fold2 *f2 = opcFold2TableMov; num > 0 && emitOptBase <= num-1 && f2->opc0 != OPC_HALT; f2++)
		{
			// fold LPUSH32 + LSTORE32 => LMOV32 (and similar)
			if (opc != (UInt)f2->opc1 || GetInsType(num-1) != f2->opc0)
				continue;

			Int x = GetInsImm24(num-1);
			Int y = *reinterpret_cast< const Int * >(&ins) >> 8;
			y--;
			LETHE_ASSERT(x >= 0 && y >= 0);

			if (x > 255 || y > 255)
				continue;

			instructions[num-1] = f2->opcFold + ((UInt)y << 8) + ((UInt)x << 16);
			return;
		}

		// fold ICONST + LPUSH32 + IADD + LSTORE32 OR LPUSH32 + IADD_ICONST + LSTORE32
		// FIXME: better!
		for (const Fold3 *f3 = opcFold3Table; num >= 2 && emitOptBase <= num-2 && f3->opc0 != OPC_HALT; f3++)
		{
			if (opc != (UInt)f3->opc2 || GetInsType(num-1) != f3->opc1 || GetInsType(num-2) != f3->opc0)
				continue;

			// verify than LPUSH32 is fine
			if (Abs(GetInsImm24(num-1)) > 127 ||
					GetInsImm24(num-2) > 255 ||
					(ins >> 8) > 255u)
				continue;

			// fold this!
			Int x = GetInsImm24(num-1);
			Int y = GetInsImm24(num-2);
			// LSTORE32 expects 1 arg on stack
			Int z = (Int)(ins >> 8)-1;
			instructions[num-2] =
				f3->opcFold + ((UInt)x << 24) + ((UInt)y << 16) + ((UInt)z << 8);
			instructions.Resize(num-1);
			return;
		}

		// fold LPUSH32 + IADD + LSTORE32 => LIADD
		for (const Fold3 *f3 = opcFold3TableSimple; num >= 2 && emitOptBase <= num-2 && f3->opc0 != OPC_HALT; f3++)
		{
			if (opc != (UInt)f3->opc2 || GetInsType(num-1) != f3->opc1 || GetInsType(num-2) != f3->opc0)
				continue;

			// verify than LPUSH32 is fine
			if (GetInsImm24(num-2) > 255 ||
					(ins >> 8) > 255u)
				continue;

			// fold this!
			Int y = GetInsImm24(num-2);
			// LSTORE32 expects 1 arg on stack
			Int z = (Int)(ins >> 8)-0;
			instructions[num-2] =
				f3->opcFold + ((UInt)y << 16) + ((UInt)z << 8);
			instructions.Resize(num-1);
			return;
		}

		break;

	case OPC_PLOAD8:
	case OPC_PLOAD8U:
	case OPC_PLOAD16:
	case OPC_PLOAD16U:
	case OPC_PLOAD32:
	case OPC_PLOAD32F:
	case OPC_PLOAD64D:
	case OPC_PLOADPTR:
		for (const Fold2 *f2 = opcFold2TablePLoad; num > 0 && emitOptBase <= num-1 && f2->opc0 != OPC_HALT; f2++)
		{
			// fold PUSH_ICONST + PLOAD32 => PLOAD32_IMM (and similar)
			if (opc != (UInt)f2->opc1 || GetInsType(num-1) != f2->opc0)
				continue;

			Int x = GetInsImm24(num-1);
			Int y = *reinterpret_cast< const Int * >(&ins) >> 8;
			x *= y;

			if (!CanEncodeI24(x))
				continue;

			instructions[num-1] = f2->opcFold + ((UInt)x << 8);

			if (emitOptBase <= num-2 && num-2 >= 0 && GetInsType(num-2) == OPC_AADD_ICONST)
			{
				x += GetInsImm24(num - 2);

				if (CanEncodeI24(x))
				{
					instructions[num-2] = f2->opcFold + ((UInt)x << 8);
					instructions.Pop();
				}
			}

			return;
		}

		break;

	case OPC_AADD_ICONST:

	case OPC_PLOAD8_IMM:
	case OPC_PLOAD8U_IMM:
	case OPC_PLOAD16_IMM:
	case OPC_PLOAD16U_IMM:
	case OPC_PLOAD32_IMM:
	case OPC_PLOAD32F_IMM:
	case OPC_PLOAD64D_IMM:
	case OPC_PLOADPTR_IMM:

	case OPC_PSTORE8_IMM:
	case OPC_PSTORE16_IMM:
	case OPC_PSTORE32_IMM:
	case OPC_PSTORE32F_IMM:
	case OPC_PSTOREPTR_IMM:
	case OPC_PSTORE8_IMM_NP:
	case OPC_PSTORE16_IMM_NP:
	case OPC_PSTORE32_IMM_NP:
	case OPC_PSTORE32F_IMM_NP:
	case OPC_PSTORE64D_IMM_NP:
	case OPC_PSTOREPTR_IMM_NP:
		for (const Fold2 *f2 = opcFold2TablePXX; num > 0 && emitOptBase <= num-1 && f2->opc0 != OPC_HALT; f2++)
		{
			// fold AADD_ICONST + PSTORE32_IMM => PSTORE32_IMM (and similar)
			if (opc != (UInt)f2->opc1 || GetInsType(num-1) != f2->opc0)
				continue;

			Int x = GetInsImm24(num-1);
			Int y = *reinterpret_cast< const Int * >(&ins) >> 8;
			x += y;

			if (!CanEncodeI24(x))
				continue;

			instructions[num-1] = f2->opcFold + ((UInt)x << 8);
			return;
		}

		break;

	case OPC_IADD:
	case OPC_FADD:
	case OPC_IAND:
	case OPC_IOR:
	case OPC_IXOR:
	case OPC_ISHL:
	case OPC_ISHR:
	case OPC_ISAR:
	case OPC_IMUL:
		for (const Fold2 *f2 = opcFold2Table; num > 0 && emitOptBase <= num-1 && f2->opc0 != OPC_HALT; f2++)
		{
			// fold PUSH_ICONST + IADD => IADD_ICONST (and similar)
			if (opc != (UInt)f2->opc1 || GetInsType(num-1) != f2->opc0)
				continue;

			instructions[num-1] = ((UInt)GetInsImm24(num-1) << 8) + f2->opcFold;
			return;
		}

		break;

	case OPC_ISUB:
	case OPC_FSUB:
		for (const Fold2 *f2 = opcFold2TableSub; num > 0 && emitOptBase <= num-1 && f2->opc0 != OPC_HALT; f2++)
		{
			// fold PUSH_ICONST + ISUB => IADD_ICONST (but reverse)
			if (opc != (UInt)f2->opc1 || GetInsType(num-1) != f2->opc0)
				continue;

			auto val = -GetInsImm24(num-1);

			// we have to add this otherwise we break JIT
			if (GetJitFriendly())
				cpool.Add((Float)val);

			// note: this assumes 2's complement integers
			instructions[num-1] = ((UInt)val << 8) + f2->opcFold;
			return;
		}

		break;

	case OPC_PUSH_ICONST:

		// fold LPUSH32 + PUSH_ICONST
		for (const Fold2 *f2 = opcFold2TableDblPush; num > 1 && emitOptBase <= num-2 && f2->opc0 != OPC_HALT; f2++)
		{
			if (GetInsType(num-1) != f2->opc1 || GetInsType(num-2) != f2->opc0)
				continue;

			Int i0 = instructions[num-2];
			Int i1 = instructions[num-1];

			if (Abs(i1 >> 8) > 32767 || ((UInt)i0 >> 8) > 255u)
				continue;

			instructions[num-2] = f2->opcFold + (UInt(i1 >> 8) << 16) + (i0 & 0xff00);
			instructions[num-1] = *reinterpret_cast< const Int * >(&ins);
			return;
		}

		break;

	case OPC_PUSHC_ICONST:

		// fold LPUSH32 + PUSHC_ICONST
		for (const Fold2 *f2 = opcFold2TableDblPushC; num > 1 && emitOptBase <= num-2 && f2->opc0 != OPC_HALT; f2++)
		{
			if (GetInsType(num-1) != f2->opc1 || GetInsType(num-2) != f2->opc0)
				continue;

			Int i0 = instructions[num-2];
			Int i1 = instructions[num-1];

			if (((UInt)i1 >> 8) > 65535 || ((UInt)i0 >> 8) > 255u)
				continue;

			*reinterpret_cast< Int * >(&instructions[num-2]) = (UInt)f2->opcFold + (((UInt)i1 >> 8) << 16) + ((UInt)i0 & 0xff00);
			instructions[num-1] = *reinterpret_cast< const Int * >(&ins);
			return;
		}

		break;

	case OPC_FMUL:
		// optimize float*2 => float+float
		if (num > 0 && emitOptBase <= num-1)
		{
			if (instructions[num-1] == OPC_PUSH_FCONST + 2*256)
			{
				instructions[num-1] = OPC_LPUSH32F;
				instructions.Add(OPC_FADD);
				return;
			}
		}
		break;

	case OPC_DMUL:
		// optimize double*2 => double+double
		if (num > 0 && emitOptBase <= num-1)
		{
			if (instructions[num-1] == OPC_PUSH_DCONST + 2*256)
			{
				instructions[num-1] = OPC_LPUSH64D;
				instructions.Add(OPC_DADD);
				return;
			}
		}
		break;

	case OPC_ICMPNZ:
		if (num > 0 && emitOptBase <= num-1)
		{
			switch(instructions[num-1])
			{
			case OPC_ICMPNZ:
			case OPC_FCMPNZ:
			case OPC_ICMPZ:
			case OPC_FCMPZ:
			case OPC_ICMPEQ:
			case OPC_ICMPNE:
			case OPC_ICMPLT:
			case OPC_ICMPLE:
			case OPC_ICMPGT:
			case OPC_ICMPGE:
			case OPC_UICMPLT:
			case OPC_UICMPLE:
			case OPC_UICMPGT:
			case OPC_UICMPGE:
			case OPC_FCMPEQ:
			case OPC_FCMPNE:
			case OPC_FCMPLT:
			case OPC_FCMPLE:
			case OPC_FCMPGT:
			case OPC_FCMPGE:
			case OPC_DCMPEQ:
			case OPC_DCMPNE:
			case OPC_DCMPLT:
			case OPC_DCMPLE:
			case OPC_DCMPGT:
			case OPC_DCMPGE:
				return;
			}
		}

		break;

	case OPC_UIMOD:
		if (num > 0 && emitOptBase <= num-1 && Byte(instructions[num-1]) == OPC_PUSH_ICONST)
		{
			auto imm24 = (Int)instructions[num-1] >> 8;
			// if it's a power of 2 greater than zero, we can fold into iand_iconst const-1
			if (imm24 > 0 && !(imm24 & (imm24-1)))
			{
				--imm24;
				instructions[--num] = imm24*256 + OPC_IAND_ICONST;
				return;
			}
		}
		break;

	case OPC_UIDIV:
		if (num > 0 && emitOptBase <= num-1 && Byte(instructions[num-1]) == OPC_PUSH_ICONST)
		{
			auto imm24 = (Int)instructions[num-1] >> 8;
			// if it's a power of 2 greater than zero, we can fold into ishr_iconst log2(const)
			if (imm24 > 0 && !(imm24 & (imm24-1)))
			{
				instructions[--num] = Bits::GetLsb(imm24)*256 + OPC_ISHR_ICONST;
				return;
			}
		}
		break;
	}

	instructions.Add(*reinterpret_cast< const Int * >(&ins));
}

void CompiledProgram::EmitAddRef(QDataType dt)
{
	if (dt.GetTypeEnum() != DT_RAW_PTR)
	{
		// special post handling of pointers...
		EmitI24(OPC_BCALL, dt.GetTypeEnum() == DT_STRONG_PTR ? BUILTIN_ADD_STRONG : BUILTIN_ADD_WEAK);
	}
}

void CompiledProgram::EmitDelStr(Int offset)
{
	if (offset == 0)
		EmitI24(OPC_BCALL, BUILTIN_LDELSTR0);
	else if (offset == 1)
		EmitI24(OPC_BCALL, BUILTIN_LDELSTR1);
	else
	{
		EmitIntConst(offset);
		EmitI24(OPC_BCALL, BUILTIN_LDELSTR);
	}
}

void CompiledProgram::Optimize()
{
	// note: never do this inside switch table!!!
	Int nextSwitch = -1;
	Int switchIndex = 0;

	LETHE_ASSERT(!(switchRange.GetSize() & 1));

	if (!switchRange.IsEmpty())
		nextSwitch = switchRange.Front();

	static const Int chainIns0[] =
	{
		OPC_ICMPNZ_BZ, OPC_FCMPNZ_BZ, OPC_DCMPNZ_BZ, OPC_BR
	};
	static const Int chainIns1[] =
	{
		OPC_ICMPNZ_BNZ, OPC_FCMPNZ_BNZ, OPC_DCMPNZ_BNZ, OPC_BR
	};
	static const Int chainIns2[] =
	{
		OPC_BR, OPC_BR, OPC_BR, OPC_BR
	};

	const Int *chainIns = 0;
	Int asz = (Int)ArraySize(chainIns0);
	Int isz = instructions.GetSize();

	for (Int i=0; i<isz; i++)
	{
		if (i == nextSwitch)
		{
			i = switchRange[switchIndex+1] - 1;
			switchIndex += 2;
			nextSwitch = switchIndex < switchRange.GetSize() ? switchRange[switchIndex] : -1;
			continue;
		}

		Int &i0 = instructions[i];
		// jump target chain optimization for lazy ops (&& ||)
		UInt jmpIns = i0 & 255u;
		Int j=0;

		for (; j<asz; j++)
		{
			if (jmpIns == (UInt)chainIns0[j])
			{
				chainIns = chainIns0;
				break;
			}

			if (jmpIns == (UInt)chainIns1[j])
			{
				chainIns = chainIns1;
				break;
			}

			// for switches, chain OPC_IBEQ + n*OPC_BR
			if (jmpIns == (UInt)OPC_IBEQ)
			{
				chainIns = chainIns2;
				break;
			}
		}

		if (j >= asz)
			continue;

		// we will do jump target optimization now to skip chains
		// if istart can reach this with a sequence, optimize
		Int oldTarget = i + (i0 >> 8) + 1;
		Int newTarget = oldTarget;
		Int tmp = i;

		while (tmp < isz)
		{
			Int ji = instructions[tmp];

			if (tmp != i)
			{
				for (j=0; j<asz; j++)
				{
					if ((ji & 255u) == (UInt)chainIns[j])
						break;
				}

				if (j >= asz)
					break;
			}

			Int delta = (ji >> 8) + 1;

			if (!delta)
				break;

			tmp += delta;
			newTarget = tmp;
		}

		if (oldTarget == newTarget)
			continue;

		// optimize chain!
		i0 = (Int)jmpIns + Int(UInt(newTarget - (i+1))<<8);
	}

	// global dtors
	Finalize();
}

CompiledProgram::ElemConvType CompiledProgram::ElemConvFromDataType(const DataTypeEnum dte)
{
	// TODO: more types later
	if (dte == DT_CHAR)
		return ECONV_CHAR;

	if (dte > DT_BOOL && dte <= DT_INT)
		return ECONV_INT;

	switch(dte)
	{
	case DT_BOOL:
		return ECONV_BOOL;

	case DT_UINT:
		return ECONV_UINT;

	case DT_LONG:
		return ECONV_LONG;

	case DT_ULONG:
		return ECONV_ULONG;

	case DT_FLOAT:
		return ECONV_FLOAT;

	case DT_DOUBLE:
		return ECONV_DOUBLE;

	case DT_NAME:
		return ECONV_NAME;

	case DT_STRING:
		return ECONV_STRING;

	case DT_NULL:
		return ECONV_NULL;

	default:
		;
	}

	return ECONV_MAX;
}

const Int OPC_WARN = 0x10000000;

// + OPC_WARN: emit warning
const Int CompiledProgram::elemConvTab[ECONV_MAX][ECONV_MAX] =
{
	// BOOL
	{
		OPC_NOP,	// BOOL
		OPC_ICMPNZ,	// CHAR
		OPC_ICMPNZ,	// INT
		OPC_ICMPNZ,	// UINT
		OPC_LCMPNZ,	// LONG
		OPC_LCMPNZ,	// ULONG
		OPC_FCMPNZ,	// FLOAT
		OPC_DCMPNZ,	// DOUBLE
		OPC_LCMPNZ,	// NAME
		OPC_BCALL + (BUILTIN_CONV_STOBOOL << 8),	// STRING
		OPC_HALT	// POINTER
	},
	// CHAR
	{
		OPC_NOP,	// BOOL
		OPC_NOP,	// CHAR
		OPC_NOP,	// INT
		OPC_NOP | OPC_WARN,	// UINT
		OPC_CONV_LTOI | OPC_WARN,	// LONG
		OPC_CONV_LTOI | OPC_WARN,	// ULONG
		OPC_CONV_FTOI | OPC_WARN,	// FLOAT
		OPC_CONV_DTOI | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// INT
	{
		OPC_NOP,	// BOOL
		OPC_NOP,	// CHAR
		OPC_NOP,	// INT
		OPC_NOP | OPC_WARN,	// UINT
		OPC_CONV_LTOI | OPC_WARN,	// LONG
		OPC_CONV_LTOI | OPC_WARN,	// ULONG
		OPC_CONV_FTOI | OPC_WARN,	// FLOAT
		OPC_CONV_DTOI | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// UINT
	{
		OPC_NOP,	// BOOL
		OPC_NOP,	// CHAR
		OPC_NOP,	// INT
		OPC_NOP,	// UINT
		OPC_CONV_LTOUI | OPC_WARN,	// LONG
		OPC_CONV_LTOUI | OPC_WARN,	// ULONG
		OPC_CONV_FTOUI | OPC_WARN,	// FLOAT
		OPC_CONV_DTOUI | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// LONG
	{
		OPC_CONV_ITOL,	// BOOL
		OPC_CONV_ITOL,	// CHAR
		OPC_CONV_ITOL,	// INT
		OPC_CONV_UITOL,	// UINT
		OPC_NOP,	// LONG
		OPC_NOP | OPC_WARN,	// ULONG
		OPC_CONV_FTOL | OPC_WARN,	// FLOAT
		OPC_CONV_DTOL | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// ULONG
	{
		OPC_CONV_ITOL,	// BOOL
		OPC_CONV_ITOL,	// CHAR
		OPC_CONV_ITOL,	// INT
		OPC_CONV_UITOL,	// UINT
		OPC_NOP,	// LONG
		OPC_NOP,	// ULONG
		OPC_CONV_FTOUL | OPC_WARN,	// FLOAT
		OPC_CONV_DTOUL | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// FLOAT
	{
		OPC_CONV_ITOF,	// BOOL
		OPC_CONV_ITOF | OPC_WARN,	// CHAR
		OPC_CONV_ITOF | OPC_WARN,	// INT
		OPC_CONV_UITOF | OPC_WARN,	// UINT
		OPC_CONV_LTOF | OPC_WARN,	// LONG
		OPC_CONV_ULTOF | OPC_WARN,	// ULONG
		OPC_NOP,	// FLOAT
		OPC_CONV_DTOF | OPC_WARN,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// DOUBLE
	{
		OPC_CONV_ITOD,	// BOOL
		OPC_CONV_ITOD,	// CHAR
		OPC_CONV_ITOD,	// INT
		OPC_CONV_UITOD,	// UINT
		OPC_CONV_LTOD | OPC_WARN,	// LONG
		OPC_CONV_ULTOD | OPC_WARN,	// ULONG
		OPC_CONV_FTOD,	// FLOAT
		OPC_NOP,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_HALT	// POINTER
	},
	// NAME
	{
		OPC_HALT,	// BOOL
		OPC_HALT,	// CHAR
		OPC_HALT,	// INT
		OPC_HALT,	// UINT
		OPC_HALT,	// LONG
		OPC_HALT,	// ULONG
		OPC_HALT,	// FLOAT
		OPC_HALT,	// DOUBLE
		OPC_NOP,	// NAME
		OPC_BCALL + (BUILTIN_CONV_STON << 8),	// STRING
		OPC_HALT	// POINTER
	},
	// STRING
	{
		OPC_HALT,	// BOOL
		OPC_BCALL + (BUILTIN_CONV_CTOS << 8),	// CHAR
		OPC_BCALL + (BUILTIN_CONV_ITOS << 8),	// INT
		OPC_BCALL + (BUILTIN_CONV_UITOS << 8),	// UINT
		OPC_BCALL + (BUILTIN_CONV_LTOS << 8),	// LONG
		OPC_BCALL + (BUILTIN_CONV_ULTOS << 8),	// ULONG
		OPC_BCALL + (BUILTIN_CONV_FTOS << 8),	// FLOAT
		OPC_BCALL + (BUILTIN_CONV_DTOS << 8),	// DOUBLE
		OPC_BCALL + (BUILTIN_CONV_NTOS << 8),	// NAME
		OPC_NOP,	// STRING
		OPC_HALT	// POINTER
	},
	// POINTER
	{
		OPC_HALT,	// BOOL
		OPC_HALT,	// CHAR
		OPC_HALT,	// INT
		OPC_HALT,	// UINT
		OPC_HALT,	// LONG
		OPC_HALT,	// ULONG
		OPC_HALT,	// FLOAT
		OPC_HALT,	// DOUBLE
		OPC_HALT,	// NAME
		OPC_HALT,	// STRING
		OPC_NOP		// POINTER
	}
};

bool CompiledProgram::EmitConv(AstNode *n, const QDataType &srcq, const QDataType &dstq, bool warn)
{
	const auto &src = srcq.GetType();
	const auto &dst = dstq.GetType();

	if (srcq.IsConst() && !dstq.IsConst() && srcq.IsPointer())
	{
		// force const correctness for pointers
		return Error(n, String::Printf("cannot convert from const %s to (non-const) %s", src.GetName().Ansi(), dst.GetName().Ansi()));
	}

	if (&src == &dst)
		return true;

	if (src.type == dst.type && src.type != DT_ENUM && !src.IsStruct() && !src.IsArray() && !src.IsPointer() && !src.IsFuncPtr())
	{
		// same types => nothing to do
		return true;
	}

	// handle string vs array ref of const bytes now
	if (dst.type == DT_STRING && src.type == DT_ARRAY_REF && src.elemType.GetTypeEnum() == DT_BYTE)
	{
		EmitI24(OPC_BCALL, BUILTIN_CONV_AREF_TO_STR);
		PopStackType(true);
		PushStackType(QDataType::MakeConstType(dst));
		return true;
	}

	if (dst.type == DT_ARRAY_REF && src.type == DT_STRING && dst.name == "[](const byte)")
	{
		EmitI24(OPC_BCALL, BUILTIN_CONV_STR_TO_AREF);
		PopStackType(true);
		PushStackType(QDataType::MakeConstType(dst));
		return true;
	}

	ElemConvType esrc = ElemConvFromDataType(src.GetTypeEnumUnderlying());
	ElemConvType edst = ElemConvFromDataType(dst.GetTypeEnumUnderlying());

	if (edst == ECONV_BOOL && src.IsPointer())
	{
		// special ptr to bool conversion
		auto &qdt = exprStack.Back();
		LETHE_ASSERT(qdt.IsPointer());

		bool skipDtor = qdt.GetTypeEnum() == DT_RAW_PTR || (qdt.qualifiers & AST_Q_SKIP_DTOR) != 0;

		qdt.qualifiers |= AST_Q_SKIP_DTOR;
		Emit(OPC_PCMPNZ);

		if (src.funDtor >= 0 && !skipDtor)
		{
			EmitI24(OPC_LPUSHADR, 1);
			EmitBackwardJump(OPC_CALL, src.funDtor);
			EmitI24(OPC_POP, 1);
		}

		Emit(OPC_LMOVE32 + (1 << 8) + (0 << 16));
		EmitI24(OPC_POP, 1);
		return true;
	}

	if (dst.type == DT_ENUM)
	{
		if (src.IsInteger() && src.baseType.ref == &dst)
			return true;

		if (warn || (src.type == DT_ENUM && &src != &dst))
		{
			// force to fail
			esrc = edst = ECONV_MAX;
		}
	}

	if (esrc == ECONV_NULL)
	{
		switch(dst.type)
		{
		case DT_DELEGATE:
			EmitI24(OPC_PUSHZ_RAW, 1);
			// fall through
		case DT_FUNC_PTR:
		case DT_RAW_PTR:
		case DT_STRONG_PTR:
		case DT_WEAK_PTR:
			return true;

		default:
			;
		}
	}

	if (esrc == ECONV_MAX || edst == ECONV_MAX)
	{
		if (dst.type == DT_BOOL && src.type == DT_FUNC_PTR)
		{
			Emit(OPC_CONV_PTOB);
			return true;
		}

		if (dst.type == DT_BOOL && src.type == DT_DELEGATE)
		{
			EmitI24(OPC_POP, 1);
			Emit(OPC_CONV_PTOB);
			return true;
		}

		if (dst.type == DT_FUNC_PTR && src.type == DT_FUNC_PTR && !srcq.IsMethodPtr())
		{
			if (dstq.CanAssign(srcq))
				return true;
		}

		if (dst.type == DT_DELEGATE && srcq.IsMethodPtr())
		{
			if (dstq.CanAssign(srcq))
				return true;
		}

		auto draw = dst.type == DT_RAW_PTR;
		auto dweak = dst.type == DT_WEAK_PTR;
		auto dstrong = dst.type == DT_STRONG_PTR;

		if ((draw || dweak || dstrong) && src.IsPointer())
		{
			// has refcount?
			bool hasRef = !(srcq.qualifiers & AST_Q_SKIP_DTOR) && srcq.GetTypeEnum() != DT_RAW_PTR;
			bool nofix = dst.elemType.CanAssign(srcq.GetType().elemType);

			exprStack.Back().ref = &dst;

			// this will be more complicated than this
			// especially when converting...

			bool deleted = false;

			if (hasRef && srcq.GetTypeEnum() != dst.type)
			{
				deleted = true;
				// has refcount AND is conversion to different pointer type...
				Emit(OPC_LPUSHADR);
				// note: assuming funDtor for smart pointers clears the original pointer!
				EmitBackwardJump(OPC_CALL, src.funDtor);
				EmitI24(OPC_POP, 1);
			}

			if (!nofix)
			{
				EmitNameConst(Name(dst.elemType.GetType().name));

				if (hasRef && src.funDtor >= 0)
				{
					EmitI24(OPC_BCALL, BUILTIN_ISA_NOPOP);

					auto handle = EmitForwardJump(OPC_IBNZ_P);

					// call ptr dtor, push nullptr
					Emit(OPC_LPUSHADR);
					EmitBackwardJump(OPC_CALL, src.funDtor);
					EmitI24(OPC_POP, 2);
					EmitI24(OPC_PUSHZ_RAW, 1);

					FixupForwardTarget(handle);
				}
				else
					EmitI24(OPC_BCALL, dstrong ? BUILTIN_FIX_STRONG : BUILTIN_FIX_WEAK);
			}

			if (deleted || !hasRef)
				exprStack.Back().qualifiers |= AST_Q_SKIP_DTOR;

			return true;
		}

		if (dst.type == DT_DYNAMIC_ARRAY)
		{
			switch(src.type)
			{
			case DT_ARRAY_REF:
				if (*src.elemType.ref == *dst.elemType.ref)
				{
					QDataType qtmp;
					qtmp.ref = &dst;
					DynArrayVarFix(qtmp);
					PopStackType(true);
					PushStackType(qtmp);
					return true;
				}
				break;

			case DT_STATIC_ARRAY:
				if (*src.elemType.ref == *dst.elemType.ref)
				{
					// assume reference on expr stack
					EmitU24(OPC_LPUSHPTR, 0);
					EmitIntConst(src.arrayDims);
					EmitU24(OPC_LSTORE32, 2);

					QDataType qdt;
					qdt.ref = dst.complementaryType;

					PopStackType(true);
					PushStackType(qdt);

					QDataType qtmp;
					qtmp.ref = &dst;
					DynArrayVarFix(qtmp);

					PopStackType(true);
					PushStackType(qtmp);
					return true;
				}
				break;

			default:
				;
			}
		}

		if (dst.type == DT_ARRAY_REF)
		{
			switch(src.type)
			{
			case DT_ARRAY_REF:
				if (!dst.elemType.IsConst() && src.elemType.IsConst())
					break;

				if (*src.elemType.ref == *dst.elemType.ref)
					return true;

				break;

			case DT_STATIC_ARRAY:
				if (!dst.elemType.IsConst() && src.elemType.IsConst())
					break;

				if (*src.elemType.ref == *dst.elemType.ref)
				{
					// assume reference on expr stack
					EmitU24(OPC_LPUSHPTR, 0);
					EmitIntConst(src.arrayDims);
					EmitU24(OPC_LSTORE32, 2);

					PopStackType(true);
					auto qtmp = dstq;
					qtmp.RemoveReference();
					PushStackType(qtmp);
					return true;
				}

				break;

			default:
				;
			}
		}

		if (edst == ECONV_BOOL && src.type == DT_ARRAY_REF)
		{
			// we have: pointer, size, just drop pointer and convert to nonzero
			EmitI24(OPC_POP, 1);
			Emit(OPC_ICMPNZ);
			PopStackType(true);
			PushStackType(QDataType::MakeConstType(elemTypes[DT_BOOL]));
			return true;
		}

		return Error(n, String::Printf("cannot convert from %s to %s", src.GetName().Ansi(), dst.GetName().Ansi()));
	}

	// bit 8: 1 = emit precision warning
	Int conv = elemConvTab[edst][esrc];
	Int opc = conv & (OPC_WARN-1);

	if (warn && (conv & OPC_WARN))
		Warning(n, String::Printf("conversion may lose precision (%s to %s)", src.GetName().Ansi(), dst.GetName().Ansi()), WARN_CONV_PRECISION);

	if (opc == OPC_HALT)
		return Error(n, String::Printf("cannot convert from %s to %s", src.GetName().Ansi(), dst.GetName().Ansi()));

	if (opc != OPC_NOP)
		Emit(opc);

	PopStackType(true);
	PushStackType(QDataType::MakeConstType(dst));

	return true;
}


}
