#include "Vm.h"

#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/TypeInfo/BaseObject.h>

#include <stdio.h>

namespace lethe
{

String Vm::GetFuncName(Int pc) const
{
	String res;

	if (prog)
	{
		HashMap< Int, Int >::ConstIterator ci = prog->funcMap.Find(pc);

		if (ci != prog->funcMap.End())
			res = prog->functions.GetKey(ci->value).key;
	}

	return res;
}

String Vm::Disassemble(Int pc, Int ins) const
{
	String res;

	String fname = GetFuncName(pc);

	if (!fname.IsEmpty())
	{
		res = "//-----------------------------------------------------------------------------\n";
		res += "// func ";
		res += fname.Ansi();

		Int fidx = prog->functions.FindIndex(fname);

		if (fidx >= 0)
		{
			auto typeSig = prog->functions.GetValue(fidx).typeSignature;

			if (!typeSig.IsEmpty())
			{
				res += " // ";
				res += typeSig;
			}
		}

		res += '\n';
	}

	CompiledProgram::CodeToLine lookup;
	lookup.pc = pc;

	auto it = LowerBound(prog->codeToLine.Begin(), prog->codeToLine.End(), lookup);

	if (it != prog->codeToLine.End() && it->pc == pc)
		res += String::Printf("#%d %s\n", it->line, it->file.Ansi());

	return res + DisassembleInternal(pc, ins);
}

String Vm::DisassembleInternal(Int pc, Int ins) const
{
	LETHE_COMPILE_ASSERT(OPC_MAX <= 256);

	static const Disasm disasm[OPC_MAX+1] =
	{
		{ "push_iconst",	OPC_PUSH_ICONST,	OPN_I24 },
		{ "pushc_iconst",	OPC_PUSHC_ICONST,	OPN_U24 },
		{ "push_fconst",	OPC_PUSH_FCONST,	OPN_I24 },
		{ "pushc_fconst",	OPC_PUSHC_FCONST,	OPN_U24 },
		{ "push_dconst",	OPC_PUSH_DCONST,	OPN_I24 },
		{ "pushc_dconst",	OPC_PUSHC_DCONST,	OPN_U24 },
		{ "lpush32",		OPC_LPUSH32,		OPN_U24 },
		{ "lpush32f",		OPC_LPUSH32F,		OPN_U24 },
		{ "lpush64d",		OPC_LPUSH64D,		OPN_U24 },
		{ "lpushadr",		OPC_LPUSHADR,		OPN_U24 },
		{ "lpushptr",		OPC_LPUSHPTR,		OPN_U24 },
		{ "lpush32_iconst",	OPC_LPUSH32_ICONST,	OPN_I16_U8 },
		{ "lpush32_ciconst",OPC_LPUSH32_CICONST,OPN_U16_U8 },
		{ "push_func",      OPC_PUSH_FUNC,      OPN_I24_BR },
		{ "pop",			OPC_POP,			OPN_U24 },
		{ "gload8",			OPC_GLOAD8,			OPN_U24 },
		{ "gload8u",		OPC_GLOAD8U,		OPN_U24 },
		{ "gload16",		OPC_GLOAD16,		OPN_U24 },
		{ "gload16u",		OPC_GLOAD16U,		OPN_U24 },
		{ "gload32",		OPC_GLOAD32,		OPN_U24 },
		{ "gload32f",		OPC_GLOAD32F,		OPN_U24 },
		{ "gload64d",		OPC_GLOAD64D,		OPN_U24 },
		{ "gloadptr",		OPC_GLOADPTR,		OPN_U24 },
		{ "gloadadr",		OPC_GLOADADR,		OPN_U24 },
		{ "gstore8",		OPC_GSTORE8,		OPN_U24 },
		{ "gstore16",		OPC_GSTORE16,		OPN_U24 },
		{ "gstore32",		OPC_GSTORE32,		OPN_U24 },
		{ "gstore32f",		OPC_GSTORE32F,		OPN_U24 },
		{ "gstore64d",		OPC_GSTORE64D,		OPN_U24 },
		{ "gstoreptr",		OPC_GSTOREPTR,		OPN_U24 },
		{ "gstore8_np",		OPC_GSTORE8_NP,		OPN_U24 },
		{ "gstore16_np",	OPC_GSTORE16_NP,	OPN_U24 },
		{ "gstore32_np",	OPC_GSTORE32_NP,	OPN_U24 },
		{ "gstore32f_np",	OPC_GSTORE32F_NP,	OPN_U24 },
		{ "gstore64d_np",	OPC_GSTORE64D_NP,	OPN_U24 },
		{ "gstoreptr_np",	OPC_GSTOREPTR_NP,	OPN_U24 },
		{ "lpush8",			OPC_LPUSH8,			OPN_U24 },
		{ "lpush8u",		OPC_LPUSH8U,		OPN_U24 },
		{ "lpush16",		OPC_LPUSH16,		OPN_U24 },
		{ "lpush16u",		OPC_LPUSH16U,		OPN_U24 },
		{ "lstore8",		OPC_LSTORE8,		OPN_U24 },
		{ "lstore16",		OPC_LSTORE16,		OPN_U24 },
		{ "lstore32",		OPC_LSTORE32,		OPN_U24 },
		{ "lstore32f",		OPC_LSTORE32F,		OPN_U24 },
		{ "lstore64d",		OPC_LSTORE64D,		OPN_U24 },
		{ "lstoreptr",		OPC_LSTOREPTR,		OPN_U24 },
		{ "lstore8_np",		OPC_LSTORE8_NP,		OPN_U24 },
		{ "lstore16_np",	OPC_LSTORE16_NP,	OPN_U24 },
		{ "lstore32_np",	OPC_LSTORE32_NP,	OPN_U24 },
		{ "lstore32f_np",	OPC_LSTORE32F_NP,	OPN_U24 },
		{ "lstore64d_np",	OPC_LSTORE64D_NP,	OPN_U24 },
		{ "lstoreptr_np",	OPC_LSTOREPTR_NP,	OPN_U24 },
		{ "lmove32",		OPC_LMOVE32,		OPN_U8_U8 },
		{ "lmoveptr",		OPC_LMOVEPTR,		OPN_U8_U8 },
		{ "lswapptr",		OPC_LSWAPPTR,		OPN_NONE },
		{ "range_iconst",	OPC_RANGE_ICONST,	OPN_U24 },
		{ "range_ciconst",	OPC_RANGE_CICONST,	OPN_U24 },
		{ "range",			OPC_RANGE,			OPN_NONE },
		{ "pload8",			OPC_PLOAD8,			OPN_U24 },
		{ "pload8u",		OPC_PLOAD8U,		OPN_U24 },
		{ "pload16",		OPC_PLOAD16,		OPN_U24 },
		{ "pload16u",		OPC_PLOAD16U,		OPN_U24 },
		{ "pload32",		OPC_PLOAD32,		OPN_U24 },
		{ "pload32f",		OPC_PLOAD32F,		OPN_U24 },
		{ "pload64d",		OPC_PLOAD64D,		OPN_U24 },
		{ "ploadptr",		OPC_PLOADPTR,		OPN_U24 },
		{ "pload8_imm",		OPC_PLOAD8_IMM,		OPN_I24 },
		{ "pload8u_imm",	OPC_PLOAD8U_IMM,	OPN_I24 },
		{ "pload16_imm",	OPC_PLOAD16_IMM,	OPN_I24 },
		{ "pload16u_imm",	OPC_PLOAD16U_IMM,	OPN_I24 },
		{ "pload32_imm",	OPC_PLOAD32_IMM,	OPN_I24 },
		{ "pload32f_imm",	OPC_PLOAD32F_IMM,	OPN_I24 },
		{ "pload64d_imm",	OPC_PLOAD64D_IMM,	OPN_I24 },
		{ "ploadptr_imm",	OPC_PLOADPTR_IMM,	OPN_I24 },
		{ "pstore8_imm",	OPC_PSTORE8_IMM,	OPN_I24 },
		{ "pstore8_imm_np",	OPC_PSTORE8_IMM_NP,OPN_I24 },
		{ "pstore16_imm",	OPC_PSTORE16_IMM,	OPN_I24 },
		{ "pstore16_imm_np",OPC_PSTORE16_IMM_NP,OPN_I24 },
		{ "pstore32_imm",	OPC_PSTORE32_IMM,	OPN_I24 },
		{ "pstore32_imm_np",OPC_PSTORE32_IMM_NP,OPN_I24 },
		{ "pstore32f_imm",	OPC_PSTORE32F_IMM,	OPN_I24 },
		{ "pstore32f_imm_np",OPC_PSTORE32F_IMM_NP,OPN_I24 },
		{ "pstore64d_imm",	OPC_PSTORE64D_IMM,	OPN_I24 },
		{ "pstore64d_imm_np",OPC_PSTORE64D_IMM_NP,OPN_I24 },
		{ "pstoreptr_imm",  OPC_PSTOREPTR_IMM,  OPN_I24 },
		{ "pstoreptr_imm_np",  OPC_PSTOREPTR_IMM_NP,  OPN_I24 },
		{ "pinc8",			OPC_PINC8,			OPN_I24 },
		{ "pinc8u",			OPC_PINC8U,			OPN_I24 },
		{ "pinc16",			OPC_PINC16,			OPN_I24 },
		{ "pinc16u",		OPC_PINC16U,		OPN_I24 },
		{ "pinc32",			OPC_PINC32,			OPN_I24 },
		{ "pinc32f",		OPC_PINC32F,		OPN_I24 },
		{ "pinc64d",		OPC_PINC64D,		OPN_I24 },
		{ "pinc8_post",		OPC_PINC8_POST,		OPN_I24 },
		{ "pinc8u_post",	OPC_PINC8U_POST,	OPN_I24 },
		{ "pinc16_post",	OPC_PINC16_POST,	OPN_I24 },
		{ "pinc16u_post",	OPC_PINC16U_POST,	OPN_I24 },
		{ "pinc32_post",	OPC_PINC32_POST,	OPN_I24 },
		{ "pinc32f_post",	OPC_PINC32F_POST,	OPN_I24 },
		{ "pinc64d_post",	OPC_PINC64D_POST,	OPN_I24 },
		{ "pcopy",			OPC_PCOPY,			OPN_U24 },
		{ "pcopy_rev",		OPC_PCOPY_REV,		OPN_U24 },
		{ "pcopy_np",		OPC_PCOPY_NP,		OPN_U24 },
		{ "pswap",			OPC_PSWAP,			OPN_U24 },
		{ "push_raw",		OPC_PUSH_RAW,		OPN_U24 },
		{ "pushz_raw",		OPC_PUSHZ_RAW,		OPN_U24 },
		{ "conv_itof",		OPC_CONV_ITOF,		OPN_NONE },
		{ "conv_uitof",		OPC_CONV_UITOF,		OPN_NONE },
		{ "conv_ftoi",		OPC_CONV_FTOI,		OPN_NONE },
		{ "conv_ftoui",		OPC_CONV_FTOUI,		OPN_NONE },
		{ "conv_ftod",		OPC_CONV_FTOD,		OPN_NONE },
		{ "conv_dtof",		OPC_CONV_DTOF,		OPN_NONE },
		{ "conv_itod",		OPC_CONV_ITOD,		OPN_NONE },
		{ "conv_uitod",		OPC_CONV_UITOD,		OPN_NONE },
		{ "conv_dtoi",		OPC_CONV_DTOI,		OPN_NONE },
		{ "conv_dtoui",		OPC_CONV_DTOUI,		OPN_NONE },
		{ "conv_itos",		OPC_CONV_ITOS,		OPN_NONE },
		{ "conv_itosb",		OPC_CONV_ITOSB,		OPN_NONE },
		{ "conv_ptob",		OPC_CONV_PTOB,		OPN_NONE },
		{ "ineg",			OPC_INEG,			OPN_NONE },
		{ "inot",			OPC_INOT,			OPN_NONE },
		{ "fneg",			OPC_FNEG,			OPN_NONE },
		{ "dneg",			OPC_DNEG,			OPN_NONE },
		{ "aadd",			OPC_AADD,			OPN_U24 },
		{ "laadd",			OPC_LAADD,			OPN_U16_U8 },
		{ "aadd_iconst",	OPC_AADD_ICONST,	OPN_I24 },
		{ "aaddh_iconst",	OPC_AADDH_ICONST,	OPN_I24 },
		{ "imul_iconst",	OPC_IMUL_ICONST,	OPN_I24 },
		{ "iadd",			OPC_IADD,			OPN_NONE },
		{ "isub",			OPC_ISUB,			OPN_NONE },
		{ "imul",			OPC_IMUL,			OPN_NONE },
		{ "idiv",			OPC_IDIV,			OPN_NONE },
		{ "uidiv",			OPC_UIDIV,			OPN_NONE },
		{ "imod",			OPC_IMOD,			OPN_NONE },
		{ "uimod",			OPC_UIMOD,			OPN_NONE },
		{ "ior",			OPC_IOR,			OPN_NONE },
		{ "ior_iconst",		OPC_IOR_ICONST,		OPN_I24 },
		{ "iand",			OPC_IAND,			OPN_NONE },
		{ "iand_iconst",	OPC_IAND_ICONST,	OPN_I24 },
		{ "ixor",			OPC_IXOR,			OPN_NONE },
		{ "ixor_iconst",	OPC_IXOR_ICONST,	OPN_I24 },
		{ "ishl",			OPC_ISHL,			OPN_NONE },
		{ "ishl_iconst",	OPC_ISHL_ICONST,	OPN_I24 },
		{ "ishr",			OPC_ISHR,			OPN_NONE },
		{ "ishr_iconst",	OPC_ISHR_ICONST,	OPN_I24 },
		{ "isar",			OPC_ISAR,			OPN_NONE },
		{ "isar_iconst",	OPC_ISAR_ICONST,	OPN_I24 },
		{ "icmpz",			OPC_ICMPZ,			OPN_NONE },
		{ "icmpnz",			OPC_ICMPNZ,			OPN_NONE },
		{ "fcmpz",			OPC_FCMPZ,			OPN_NONE },
		{ "fcmpnz",			OPC_FCMPNZ,			OPN_NONE },
		{ "dcmpz",			OPC_DCMPZ,			OPN_NONE },
		{ "dcmpnz",			OPC_DCMPNZ,			OPN_NONE },
		{ "icmpnz_bz",		OPC_ICMPNZ_BZ,		OPN_I24_BR },
		{ "icmpnz_bnz",		OPC_ICMPNZ_BNZ,		OPN_I24_BR },
		{ "fcmpnz_bz",		OPC_FCMPNZ_BZ,		OPN_I24_BR },
		{ "fcmpnz_bnz",		OPC_FCMPNZ_BNZ,		OPN_I24_BR },
		{ "dcmpnz_bz",		OPC_DCMPNZ_BZ,		OPN_I24_BR },
		{ "dcmpnz_bnz",		OPC_DCMPNZ_BNZ,		OPN_I24_BR },
		{ "icmpeq",			OPC_ICMPEQ,			OPN_NONE },
		{ "icmpne",			OPC_ICMPNE,			OPN_NONE },
		{ "icmplt",			OPC_ICMPLT,			OPN_NONE },
		{ "icmple",			OPC_ICMPLE,			OPN_NONE },
		{ "icmpgt",			OPC_ICMPGT,			OPN_NONE },
		{ "icmpge",			OPC_ICMPGE,			OPN_NONE },
		{ "uicmplt",		OPC_UICMPLT,		OPN_NONE },
		{ "uicmple",		OPC_UICMPLE,		OPN_NONE },
		{ "uicmpgt",		OPC_UICMPGT,		OPN_NONE },
		{ "uicmpge",		OPC_UICMPGE,		OPN_NONE },
		{ "fcmpeq",			OPC_FCMPEQ,			OPN_NONE },
		{ "fcmpne",			OPC_FCMPNE,			OPN_NONE },
		{ "fcmplt",			OPC_FCMPLT,			OPN_NONE },
		{ "fcmple",			OPC_FCMPLE,			OPN_NONE },
		{ "fcmpgt",			OPC_FCMPGT,			OPN_NONE },
		{ "fcmpge",			OPC_FCMPGE,			OPN_NONE },
		{ "dcmpeq",			OPC_DCMPEQ,			OPN_NONE },
		{ "dcmpne",			OPC_DCMPNE,			OPN_NONE },
		{ "dcmplt",			OPC_DCMPLT,			OPN_NONE },
		{ "dcmple",			OPC_DCMPLE,			OPN_NONE },
		{ "dcmpgt",			OPC_DCMPGT,			OPN_NONE },
		{ "dcmpge",			OPC_DCMPGE,			OPN_NONE },
		{ "br",				OPC_BR,				OPN_I24_BR },
		{ "ibz_p",			OPC_IBZ_P,			OPN_I24_BR },
		{ "ibnz_p",			OPC_IBNZ_P,			OPN_I24_BR },
		{ "fbz_p",			OPC_FBZ_P,			OPN_I24_BR },
		{ "fbnz_p",			OPC_FBNZ_P,			OPN_I24_BR },
		{ "dbz_p",			OPC_DBZ_P,			OPN_I24_BR },
		{ "dbnz_p",			OPC_DBNZ_P,			OPN_I24_BR },
		{ "ibz",			OPC_IBZ,			OPN_I24_BR },
		{ "ibnz",			OPC_IBNZ,			OPN_I24_BR },
		{ "ibeq",			OPC_IBEQ,			OPN_I24_BR },
		{ "ibne",			OPC_IBNE,			OPN_I24_BR },
		{ "iblt",			OPC_IBLT,			OPN_I24_BR },
		{ "ible",			OPC_IBLE,			OPN_I24_BR },
		{ "ibgt",			OPC_IBGT,			OPN_I24_BR },
		{ "ibge",			OPC_IBGE,			OPN_I24_BR },
		{ "uiblt",			OPC_UIBLT,			OPN_I24_BR },
		{ "uible",			OPC_UIBLE,			OPN_I24_BR },
		{ "uibgt",			OPC_UIBGT,			OPN_I24_BR },
		{ "uibge",			OPC_UIBGE,			OPN_I24_BR },
		{ "fbeq",			OPC_FBEQ,			OPN_I24_BR },
		{ "fbne",			OPC_FBNE,			OPN_I24_BR },
		{ "fblt",			OPC_FBLT,			OPN_I24_BR },
		{ "fble",			OPC_FBLE,			OPN_I24_BR },
		{ "fbgt",			OPC_FBGT,			OPN_I24_BR },
		{ "fbge",			OPC_FBGE,			OPN_I24_BR },
		{ "dbeq",			OPC_DBEQ,			OPN_I24_BR },
		{ "dbne",			OPC_DBNE,			OPN_I24_BR },
		{ "dblt",			OPC_DBLT,			OPN_I24_BR },
		{ "dble",			OPC_DBLE,			OPN_I24_BR },
		{ "dbgt",			OPC_DBGT,			OPN_I24_BR },
		{ "dbge",			OPC_DBGE,			OPN_I24_BR },
		{ "pcmpeq",			OPC_PCMPEQ,			OPN_NONE },
		{ "pcmpne",			OPC_PCMPNE,			OPN_NONE },
		{ "pcmpz",			OPC_PCMPZ,			OPN_NONE },
		{ "pcmpnz",			OPC_PCMPNZ,			OPN_NONE },
		{ "fadd",			OPC_FADD,			OPN_NONE },
		{ "fsub",			OPC_FSUB,			OPN_NONE },
		{ "fmul",			OPC_FMUL,			OPN_NONE },
		{ "fdiv",			OPC_FDIV,			OPN_NONE },
		{ "dadd",			OPC_DADD,			OPN_NONE },
		{ "dsub",			OPC_DSUB,			OPN_NONE },
		{ "dmul",			OPC_DMUL,			OPN_NONE },
		{ "ddiv",			OPC_DDIV,			OPN_NONE },
		{ "fadd_iconst",	OPC_FADD_ICONST,	OPN_I24 },
		{ "lfadd_iconst",	OPC_LFADD_ICONST,	OPN_I8_U8_U8 },
		{ "lfadd",			OPC_LFADD,			OPN_U8_U8 },
		{ "lfsub",			OPC_LFSUB,			OPN_U8_U8 },
		{ "lfmul",			OPC_LFMUL,			OPN_U8_U8 },
		{ "lfdiv",			OPC_LFDIV,			OPN_U8_U8 },
		{ "iadd_iconst",	OPC_IADD_ICONST,	OPN_I24 },
		{ "liadd_iconst",	OPC_LIADD_ICONST,	OPN_I8_U8_U8 },
		{ "liadd",			OPC_LIADD,			OPN_U8_U8 },
		{ "lisub",			OPC_LISUB,			OPN_U8_U8 },
		{ "call",			OPC_CALL,			OPN_I24_BR },
		{ "fcall",			OPC_FCALL,			OPN_NONE },
		{ "fcall_dg",		OPC_FCALL_DG,		OPN_NONE },
		{ "vcall",			OPC_VCALL,			OPN_I24 },
		{ "ncall",			OPC_NCALL,			OPN_I24_NC },
		{ "nmcall",			OPC_NMCALL,			OPN_I24_NC },
		{ "bcall",			OPC_BCALL,			OPN_I24_NC },
		{ "bmcall",			OPC_BMCALL,			OPN_I24_NC },
		{ "bcall_trap",		OPC_BCALL_TRAP,		OPN_I24_NC },
		{ "nvcall",			OPC_NVCALL,			OPN_I24_NC },
		{ "ret",			OPC_RET,			OPN_U24 },
		{ "loadthis",		OPC_LOADTHIS,		OPN_NONE },
		{ "loadthis_imm",	OPC_LOADTHIS_IMM,	OPN_I24 },
		{ "pushthis",		OPC_PUSHTHIS,		OPN_NONE },
		{ "pushthis_adr",	OPC_PUSHTHIS_TEMP,	OPN_NONE },
		{ "popthis",		OPC_POPTHIS,		OPN_NONE },
		{ "switch",			OPC_SWITCH,			OPN_U24 },
		{ "halt",			OPC_HALT,			OPN_NONE },
		{ "break",			OPC_BREAK,			OPN_NONE },
		{ "chkstk",			OPC_CHKSTK,			OPN_U24 },
		{ "fsqrt",			OPC_FSQRT,			OPN_NONE },
		{ "dsqrt",			OPC_DSQRT,			OPN_NONE },

		{ 0,				OPC_HALT,			OPN_NONE }
	};

	UInt ui = (UInt)ins;
	ui &= 255u;

	bool barrier = BinarySearch(prog->barriers.Begin(), prog->barriers.End(), pc);

	bool isSwitchTable = prog->IsSwitchTable(pc);

	String adr;
	adr.Format("%08x  %08x %c", pc, ins, barrier ? '*' : ' ');

	if (isSwitchTable)
		return String::Printf("%stable %08x", adr.Ansi(), ins);

	// explicitly decode push_raw 0 as nop
	if (ins == OPC_PUSH_RAW)
		return adr + "nop";

	const Disasm *d = disasm;

	while (d->name)
	{
		if ((UInt)d->type == ui)
			break;

		d++;
	}

	if (!d->name)
		return adr + "<invalid>";

	if (d->operands == OPN_NONE)
		return adr + d->name;

	if (d->operands == OPN_I24)
		return adr + String::Printf("%s %d", d->name, ins >> 8);

	if (d->operands == OPN_I24_NC)
	{
		// translate pushstring
		auto nativeName = prog->cpool.GetNativeFuncName(ins >> 8);

		if (pc > 0 && nativeName == "*LPUSHSTR_CONST" && (prog->instructions[pc-1] & 255) == OPC_PUSH_ICONST)
		{
			Int sidx = Int((UInt)prog->instructions[pc-1] >> 8);
			String sval = prog->cpool.sPool[sidx].Escape();
			return adr + String::Printf("%s %d (%s \"%s\")", d->name, ins >> 8, nativeName.Ansi(), sval.Ansi());
		}

		if (pc > 0 && nativeName == "*NEW" && (prog->instructions[pc-1] & 255) == OPC_PUSH_ICONST)
		{
			Int sidx = Int((UInt)prog->instructions[pc-1] >> 8);
			Name n;
			n.SetIndex(sidx);
			return adr + String::Printf("%s %d (%s %s)", d->name, ins >> 8, nativeName.Ansi(), n.ToString().Ansi());
		}

		return adr + String::Printf("%s %d (%s)", d->name, ins >> 8, nativeName.Ansi());
	}

	if (d->operands == OPN_I24_BR)
	{
		Int target = pc + 1 + (ins >> 8);
		String fname = GetFuncName(target);

		if (!fname.IsEmpty())
			return adr + String::Printf("%s %08x (%s)", d->name, target, fname.Ansi());

		return adr + String::Printf("%s %08x (%d)", d->name, target, ins >> 8);
	}

	if (d->operands == OPN_U24)
		return adr + String::Printf("%s %u", d->name, (UInt)ins >> 8);

	if (d->operands == OPN_U8_U8)
	{
		return adr + String::Printf("%s %d, %d",
									d->name,
									(Int)((UInt)ins >> 8) & 255,
									(Int)((UInt)ins >> 16) & 255
								   );
	}

	if (d->operands == OPN_I8_U8_U8)
	{
		return adr + String::Printf("%s %d, %d, %d",
									d->name,
									(Int)((UInt)ins >> 8) & 255,
									(Int)((UInt)ins >> 16) & 255,
									(ins >> 24)
								   );
	}

	if (d->operands == OPN_U16_U8)
	{
		return adr + String::Printf("%s %d, %d",
									d->name,
									(Int)((UInt)ins >> 8) & 255,
									(Int)((UInt)ins >> 16)
								   );
	}

	if (d->operands == OPN_I16_U8)
	{
		return adr + String::Printf("%s %d, %d",
									d->name,
									(Int)((UInt)ins >> 8) & 255,
									(Int)(ins >> 16)
								   );
	}

	return adr + "<bad_opnd>";
}

Int Vm::FindFunc(const StringRef &fname) const
{
	if (!prog)
		return -1;

	Int fidx = prog->functions.FindIndex(fname);

	if (fidx < 0)
		return -1;

	return prog->functions.GetValue(fidx).adr;
}

ExecResult Vm::CallFunc(const StringRef &fname)
{
	if (!stack || !prog)
		return EXEC_NO_PROG;

	Int fidx = prog->functions.FindIndex(fname);
	if (fidx < 0)
		return EXEC_FUNC_NOT_FOUND;

	// make sure we push retadr to instruction #0 (halt)
	stack->PushPtr(prog->instructions.GetData());
	return Execute(prog->functions.GetValue(fidx).adr);
}

ExecResult Vm::CallGlobalConstructors()
{
	if (!stack || !prog)
		return EXEC_NO_PROG;

	Int pc = prog->globalConstIndex;

	if (pc < 0)
		return EXEC_OK;

	// make sure we push retadr to instruction #0 (halt)
	stack->PushPtr(prog->instructions.GetData());
	return Execute(pc);
}

ExecResult Vm::CallGlobalDestructors()
{
	if (!stack || !prog)
		return EXEC_NO_PROG;

	Int pc = prog->globalDestIndex;

	if (pc < 0)
		return EXEC_OK;

	// make sure we push retadr to instruction #0 (halt)
	stack->PushPtr(prog->instructions.GetData());
	return Execute(pc);
}

ExecResult Vm::RuntimeException(const Instruction *iptr, const char *msg)
{
	auto cstk = GetCallStack(iptr);
	String fullmsg = msg;

	for (Int i=0; i<cstk.GetSize(); i++)
	{
		fullmsg += '\n';
		fullmsg += cstk[i];
	}

	onRuntimeError(fullmsg.Ansi());
	printf("VM Error: %s\n", fullmsg.Ansi());
	//__debugbreak();
	return EXEC_EXCEPTION;
}

const Stack::StackWord *Vm::FindStackFrame(const Stack::StackWord *ptr) const
{
	Stack &stk = *stack;
	// tracing up...
	auto max = stk.GetBottom();

	for (; ptr < max; ptr++)
	{
		auto adr = *ptr;

		// instruction pointers must have 3 lsbit clear
		if (adr & 3)
			continue;

		auto iptr = reinterpret_cast<const Instruction *>(adr) - 1;

		// very fist instruction is halt, so check here for this as well
		if (iptr && iptr+1 == prog->instructions.GetData())
			++iptr;

		// is it a valid code ptr?
		if (!prog->IsValidCodePtr(iptr))
			continue;

		LETHE_ASSERT(iptr);

		// only valid if it's a call or halt
		switch(*iptr & 255u)
		{
		case OPC_HALT:
		case OPC_CALL:
		case OPC_FCALL:
		case OPC_FCALL_DG:
		case OPC_VCALL:
			break;

		default:
			continue;
		}

		// found it!
		return ptr;
	}

	return nullptr;
}

Int Vm::FindFuncStart(Int pc) const
{
	while (pc >= 0)
	{
		auto ci = prog->funcMap.Find(pc);

		if (ci != prog->funcMap.End())
			break;

		pc--;
	}

	return pc;
}

String Vm::GetFullCallStack(Int pc, Int opc) const
{
	CompiledProgram::CodeToLine cl;
	cl.pc = opc;
	cl.line = 0;

	auto ci = LowerBound(prog->codeToLine.Begin(), prog->codeToLine.End(), cl);
	String extra;

	if (ci == prog->codeToLine.End() && !prog->codeToLine.IsEmpty())
	{
		--ci;
		opc = ci->pc;
	}

	if (ci != prog->codeToLine.End())
	{
		if (ci->pc != opc && ci > prog->codeToLine.Begin())
			--ci;

		extra = " ";
		extra += String::Printf("[%d: %s]",
								ci->line, ci->file.Ansi());
	}

	return GetFuncName(pc) + extra;
}

Int Vm::GetFullCallStackDepth(Int pc, Int opc) const
{
	(void)pc;
	Int res = 0;

	CompiledProgram::CodeToLine cl;
	cl.pc = opc;
	cl.line = 0;

	auto ci = LowerBound(prog->codeToLine.Begin(), prog->codeToLine.End(), cl);

	if (ci == prog->codeToLine.End() && !prog->codeToLine.IsEmpty())
	{
		--ci;
		opc = ci->pc;
	}

	if (ci != prog->codeToLine.End())
	{
		if (ci->pc != opc && ci > prog->codeToLine.Begin())
			--ci;

		++res;
	}

	return res;
}

Array<String> Vm::GetThis(Int pc, Int startpc) const
{
	Array<String> res;

	auto &stk = *stack;
	auto fn = GetFuncName(startpc);

	if (fn.IsEmpty() || !stk.GetThis())
		return res;

	// try to extract this...
	auto fidx = fn.ReverseFind("::", Limits<Int>::Max());

	if (fidx < 0)
		return res;

	const auto &p = stk.GetProgram();

	// verify that this is valid for this function
	// problem is that pushthis doesn't have to be paired with popthis at all, but loadthis/popthis should always come in pairs
	// so I modified the opcodes, marking pushthis that just loads this ptr vs pushthis that actually saves stuff
	// SO: loadthis = pushthis + loadthis_imm, basically

	// analyze
	Int pushThisCount = 0;
	Int loadThisCount = -1;

	// note: not including pc; this is very important
	for (Int i=startpc; i<pc; i++)
	{
		if (p.IsSwitchTable(i))
			continue;

		auto opc = p.instructions[i] & 255;

		if (opc == OPC_LOADTHIS || opc == OPC_LOADTHIS_IMM)
		{
			if (loadThisCount < 0)
				loadThisCount = pushThisCount;
		}

		if (opc == OPC_LOADTHIS || opc == OPC_PUSHTHIS)
			++pushThisCount;
		else if (opc == OPC_POPTHIS)
		{
			--pushThisCount;
			// if this fails, this analysis doesn't work
			LETHE_ASSERT(pushThisCount >= 0);

			if (loadThisCount >= 0 && pushThisCount == loadThisCount)
				loadThisCount = -1;
		}
	}

	// if we're inside an open loadthis, we can't reliably extract this so we bail out
	if (loadThisCount >= 0)
		return res;

	fn.Erase(fidx);

	for (auto &&itx : prog->types)
	{
		if (itx->name != fn)
			continue;

		// found this but ... could be wrong and potentially crash!
		// due to: calling other functions or inlining => should analyze pushthis, loadthis, loadthis_imm, popthis
		const Byte *thisptr = static_cast<const Byte *>(stk.GetThis());

		auto addMembers = [&](const DataType *dt)
		{
			for (auto &&m : dt->members)
			{
				String tmp;
				tmp.Format("%s = ", m.name.Ansi());

				StringBuilder sb;
				m.type.ref->GetVariableText(sb, thisptr + m.offset);
				tmp += sb.Get();

				res.Add(tmp);
			}
		};

		StackArray<const DataType *, 16> baseTypes;

		const auto *tmp = itx.Get();

		while (tmp && tmp->type != DT_NONE)
		{
			baseTypes.Add(tmp);
			tmp = &tmp->baseType.GetType();
		}

		baseTypes.Reverse();

		for (auto &&it : baseTypes)
			addMembers(it);

		break;
	}

	return res;
}

Int Vm::GetCallStackDepth(const Instruction *iptr) const
{
	Int res = 0;

	if (!iptr)
		return res;

	Stack &stk = *stack;
	Int pc = Int(iptr - prog->instructions.GetData());
	Int opc;

	const Stack::StackWord *stkptr = stk.GetTop();

	for (Int idx=0;;idx++)
	{
		auto frame = FindStackFrame(stkptr);

		if (!frame)
			break;

		stkptr = frame+1;

		opc = pc;
		pc = FindFuncStart(pc);

		if (pc < 0)
			break;

		res += GetFullCallStackDepth(pc, opc);

		pc = (Int)IntPtr(reinterpret_cast<const Int *>(frame[0]) - prog->instructions.GetData());
	}

	opc = pc;
	pc = FindFuncStart(pc);

	if (pc >= 0)
		res += GetFullCallStackDepth(pc, opc);

	return res;
}

Array<String> Vm::GetCallStack(const Instruction *iptr) const
{
	Array<String> res;

	if (!iptr)
		return res;

	Stack &stk = *stack;
	Int pc = Int(iptr - prog->instructions.GetData());
	Int opc;

	const Stack::StackWord *stkptr = stk.GetTop();

	auto locals = GetLocals(iptr, stkptr);
	res.Add("=== locals: ===");
	res.Append(locals);
	res.Add("");

	for (Int idx=0;;idx++)
	{
		auto frame = FindStackFrame(stkptr);

		if (!frame)
			break;

		stkptr = frame+1;

		opc = pc;
		pc = FindFuncStart(pc);

		if (pc < 0)
			break;

		if (idx == 0)
		{
			// try to extract "this"
			auto tmp = GetThis(opc, pc);

			if (!tmp.IsEmpty())
			{
				res.Add("=== this: ===");
				res.Append(tmp);
			}

			res.Add("=== call stack: ===");
		}

		res.Add(GetFullCallStack(pc, opc));

		if (res.GetSize() >= 100)
		{
			res.Add("... snip ...");
			return res;
		}

		pc = (Int)IntPtr(reinterpret_cast<const Int *>(frame[0]) - prog->instructions.GetData());
	}

	opc = pc;
	pc = FindFuncStart(pc);

	if (pc >= 0)
		res.Add(GetFullCallStack(pc, opc));

	return res;
}

Array<String> Vm::GetLocals(const Instruction *iptr, const Stack::StackWord *sptr, bool withType) const
{
	Array<String> res;

	if (!iptr || !sptr)
		return res;

	Int pc = Int(iptr - prog->instructions.GetData());
	auto frame = FindStackFrame(sptr);

	if (!frame)
		return res;

	for (const auto &it : prog->localVars)
	{
		const auto &lv = it.value;

		if (pc < lv.startPC || pc >= lv.endPC)
			continue;

		String str;

		if (withType)
		{
			str += lv.type.GetName();
			str += ' ';
		}

		str += lv.name;

		if (!lv.type.IsReference())
		{
			str += " = ";
			StringBuilder sb;
			lv.type.GetType().GetVariableText(sb, frame + lv.offset/Stack::WORD_SIZE);
			str += sb.Get();
			res.Add(str);
		}
		else
		{
			str += " = ref ";
			auto tmp = *(const void **)(frame + lv.offset / Stack::WORD_SIZE);

			if (!tmp)
				str += "null";
			else
			{
				StringBuilder sb;
				lv.type.GetType().GetVariableText(sb, tmp);
				str += sb.Get();
			}

			res.Add(str);
		}
	}

	return res;
}


}
