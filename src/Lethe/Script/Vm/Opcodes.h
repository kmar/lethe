#pragma once

#include "Builtin.h"

namespace lethe
{

typedef lethe::Int Instruction;

// WARNING!!! all 256 opcodes taken!!!
enum VmOpCode
{
	// push as int constant (imm24)
	OPC_PUSH_ICONST,
	// push const pool relative (uimm24)
	OPC_PUSHC_ICONST,
	// push as float constant (imm24)
	OPC_PUSH_FCONST,
	// push const pool relative (uimm24)
	OPC_PUSHC_FCONST,
	// push as double constant (imm24)
	OPC_PUSH_DCONST,
	// push const pool relative (uimm24)
	OPC_PUSHC_DCONST,
	// push 32-bit stk-relative uimm24
	OPC_LPUSH32,
	OPC_LPUSH32F,
	OPC_LPUSH64D,
	// push pointer (address of 32-bit stk-relative uimm24)
	OPC_LPUSHADR,
	// push pointer (stored in local variable)
	OPC_LPUSHPTR,
	// double-push: 32-bit stk-relative, then iconst
	// encoded as uimm8, imm16#0
	OPC_LPUSH32_ICONST,
	// double push: 32-bit stk relative, then const_pool iconst
	// encoded as uimm8, uimm16#0
	OPC_LPUSH32_CICONST,
	// push function ptr, imm24 function PC relative offset
	OPC_PUSH_FUNC,
	// pop uimm24 stack words to pop
	OPC_POP,
	// fetch global and push, gdata + uimm24
	OPC_GLOAD8,
	OPC_GLOAD8U,
	OPC_GLOAD16,
	OPC_GLOAD16U,
	OPC_GLOAD32,
	OPC_GLOAD32F,
	OPC_GLOAD64D,
	OPC_GLOADPTR,
	// load address of gdata (push gdata + uimm24; byte-offset)
	OPC_GLOADADR,
	// store global value (top of stack) + pop
	OPC_GSTORE8,
	OPC_GSTORE16,
	OPC_GSTORE32,
	OPC_GSTORE32F,
	OPC_GSTORE64D,
	OPC_GSTOREPTR,
	// no-pop version
	OPC_GSTORE8_NP,
	OPC_GSTORE16_NP,
	OPC_GSTORE32_NP,
	OPC_GSTORE32F_NP,
	OPC_GSTORE64D_NP,
	OPC_GSTOREPTR_NP,
	OPC_LPUSH8,
	OPC_LPUSH8U,
	OPC_LPUSH16,
	OPC_LPUSH16U,
	// store local value (top of stack) + pop; local offset = (uimm24 words/bytes/shorts/... => must think about this...)
	OPC_LSTORE8,
	OPC_LSTORE16,
	OPC_LSTORE32,
	OPC_LSTORE32F,
	OPC_LSTORE64D,
	OPC_LSTOREPTR,
	// no-pop version
	OPC_LSTORE8_NP,
	OPC_LSTORE16_NP,
	OPC_LSTORE32_NP,
	OPC_LSTORE32F_NP,
	OPC_LSTORE64D_NP,
	OPC_LSTOREPTR_NP,
	// and moves of course
	// move uimm8#1 to uimm8#0
	OPC_LMOVE32,
	OPC_LMOVEPTR,
	// swap ptr[0]<->ptr[1] (array ref range check)
	OPC_LSWAPPTR,

	// static array range check
	OPC_RANGE_ICONST,
	OPC_RANGE_CICONST,
	// dynamic array/array ref range check
	// expects: index, limit
	// out: index
	OPC_RANGE,

	// indirect (ptr-relative) store/load
	// on stack: ptr, offset (value for stores)
	OPC_PLOAD8,
	OPC_PLOAD8U,
	OPC_PLOAD16,
	OPC_PLOAD16U,
	// load + pop (adr relative in bytes)
	OPC_PLOAD32,
	OPC_PLOAD32F,
	OPC_PLOAD64D,
	OPC_PLOADPTR,
	// immediate version, top of stack = ptr, load [ptr + imm byte offset]
	OPC_PLOAD8_IMM,
	OPC_PLOAD8U_IMM,
	OPC_PLOAD16_IMM,
	OPC_PLOAD16U_IMM,
	OPC_PLOAD32_IMM,
	OPC_PLOAD32F_IMM,
	OPC_PLOAD64D_IMM,
	OPC_PLOADPTR_IMM,

	// immediate version, expects uimm24 byte offset
	OPC_PSTORE8_IMM,
	OPC_PSTORE8_IMM_NP,
	OPC_PSTORE16_IMM,
	OPC_PSTORE16_IMM_NP,
	OPC_PSTORE32_IMM,
	OPC_PSTORE32F_IMM,
	OPC_PSTORE32_IMM_NP,
	OPC_PSTORE32F_IMM_NP,
	OPC_PSTORE64D_IMM,
	OPC_PSTORE64D_IMM_NP,
	OPC_PSTOREPTR_IMM,
	OPC_PSTOREPTR_IMM_NP,

	// pre-increment (+imm24 amount)
	OPC_PINC8,
	OPC_PINC8U,
	OPC_PINC16,
	OPC_PINC16U,
	OPC_PINC32,
	OPC_PINC32F,
	OPC_PINC64D,

	OPC_PINC8_POST,
	OPC_PINC8U_POST,
	OPC_PINC16_POST,
	OPC_PINC16U_POST,
	OPC_PINC32_POST,
	OPC_PINC32F_POST,
	OPC_PINC64D_POST,

	// stack: dst_ptr, src_ptr, +uimm24 amount
	OPC_PCOPY,
	// reversed version (src_ptr, dst_ptr) => because of array refs
	OPC_PCOPY_REV,
	// don't pop src
	OPC_PCOPY_NP,
	// similar to PCOPY except that contents are swapped
	OPC_PSWAP,

	// push raw (=reserve) uimm24 words
	OPC_PUSH_RAW,
	// don't waste opcode on nop => simply push raw 0
	OPC_NOP = OPC_PUSH_RAW,
	// push raw zero-init uimm24 words
	OPC_PUSHZ_RAW,

	// conversion ops (sigh):
	// most common conversions should be fast:
	OPC_CONV_ITOF,
	OPC_CONV_UITOF,
	OPC_CONV_FTOI,
	OPC_CONV_FTOUI,

	OPC_CONV_FTOD,
	OPC_CONV_DTOF,
	OPC_CONV_ITOD,
	OPC_CONV_UITOD,
	OPC_CONV_DTOI,
	OPC_CONV_DTOUI,

	// nothing to do for BTOI
	// BTOF same sas UITOF
	// special conversions needed for casts to smaller type
	OPC_CONV_ITOS,
	OPC_CONV_ITOSB,
	// IAND_ICONST will be used to casts to small unsigned type
	OPC_CONV_PTOB,

	// basic unary ops:
	OPC_INEG,
	OPC_INOT,
	OPC_FNEG,
	OPC_DNEG,
	// address add topmost is int, [1] is ptr
	OPC_AADD,
	// address add topmost is int, [1] is ptr
	// uimm16top = local offset, uimm8#0 is multiplier
	OPC_LAADD,
	// address add topmost is ptr, add int
	OPC_AADD_ICONST,
	// address add topmost is ptr, add int<<16
	OPC_AADDH_ICONST,
	// imul by iconst
	OPC_IMUL_ICONST,
	// basic ops: on two topmost values on stack+pop
	OPC_IADD,
	OPC_ISUB,
	// note: no need for UIMUL as long as we're 2's complement
	OPC_IMUL,
	OPC_IDIV,
	OPC_UIDIV,
	OPC_IMOD,
	OPC_UIMOD,
	OPC_IOR,
	OPC_IOR_ICONST,
	OPC_IAND,
	// used for conversion to smaller unsigned types
	// might also be useful for peephole optimization
	OPC_IAND_ICONST,
	OPC_IXOR,
	OPC_IXOR_ICONST,
	OPC_ISHL,
	OPC_ISHL_ICONST,
	OPC_ISHR,
	OPC_ISHR_ICONST,
	OPC_ISAR,
	OPC_ISAR_ICONST,

	// hmm, NO => current cmp/br requires two instructions to be decoded,
	// but it would be better to just use one
	// BUT I can't optimize it directly when generating code...
	// convert topmost (u)int to bool
	OPC_ICMPZ,
	OPC_ICMPNZ,
	// the same for float
	OPC_FCMPZ,
	OPC_FCMPNZ,
	// and double
	OPC_DCMPZ,
	OPC_DCMPNZ,

	// non-zero compare + jump else pop (imm24)
	OPC_ICMPNZ_BZ,
	OPC_ICMPNZ_BNZ,
	// the same for float
	OPC_FCMPNZ_BZ,
	OPC_FCMPNZ_BNZ,
	// and double
	OPC_DCMPNZ_BZ,
	OPC_DCMPNZ_BNZ,

	// push result of int_cmp on stack, pop
	OPC_ICMPEQ,
	OPC_ICMPNE,
	OPC_ICMPLT,
	OPC_ICMPLE,
	OPC_ICMPGT,
	OPC_ICMPGE,
	OPC_UICMPLT,
	OPC_UICMPLE,
	OPC_UICMPGT,
	OPC_UICMPGE,
	OPC_FCMPEQ,
	OPC_FCMPNE,
	OPC_FCMPLT,
	OPC_FCMPLE,
	OPC_FCMPGT,
	OPC_FCMPGE,
	OPC_DCMPEQ,
	OPC_DCMPNE,
	OPC_DCMPLT,
	OPC_DCMPLE,
	OPC_DCMPGT,
	OPC_DCMPGE,

	// branches:
	OPC_BR,
	// if topmost (u)int zero, jump, always pop
	OPC_IBZ_P,
	OPC_IBNZ_P,
	OPC_FBZ_P,
	OPC_FBNZ_P,
	OPC_DBZ_P,
	OPC_DBNZ_P,
	// if topmost (u)int zero, jump else pop
	OPC_IBZ,
	OPC_IBNZ,
	// if two topmost (u)ints equal, pop (and so on)
	OPC_IBEQ,
	OPC_IBNE,
	OPC_IBLT,
	OPC_IBLE,
	OPC_IBGT,
	OPC_IBGE,
	OPC_UIBLT,
	OPC_UIBLE,
	OPC_UIBGT,
	OPC_UIBGE,

	OPC_FBEQ,
	OPC_FBNE,
	OPC_FBLT,
	OPC_FBLE,
	OPC_FBGT,
	OPC_FBGE,

	OPC_DBEQ,
	OPC_DBNE,
	OPC_DBLT,
	OPC_DBLE,
	OPC_DBGT,
	OPC_DBGE,

	// special pointer comparisons
	OPC_PCMPEQ,
	OPC_PCMPNE,
	OPC_PCMPZ,
	OPC_PCMPNZ,

	OPC_FADD,
	OPC_FSUB,
	OPC_FMUL,
	OPC_FDIV,

	OPC_DADD,
	OPC_DSUB,
	OPC_DMUL,
	OPC_DDIV,

	// float add imm24 to top of stack
	OPC_FADD_ICONST,
	// float add uimm8#1 + imm8 to uimm8#2
	OPC_LFADD_ICONST,
	// float add uimm8#1 + top to uimm8#0 + pop
	OPC_LFADD,

	OPC_LFSUB,
	OPC_LFMUL,
	OPC_LFDIV,

	// (u)int add imm24 to top of stack
	OPC_IADD_ICONST,
	// (u)int add uimm8#1 + imm8 to uimm8#2
	OPC_LIADD_ICONST,
	// (u)int add uimm8#1 + top to uimm8#0 + pop
	OPC_LIADD,
	OPC_LISUB,

	// call function/nonvirtual method (imm24 index)
	OPC_CALL,
	// call func ptr + pop
	OPC_FCALL,
	// special fcall for delegates (fptr can hold vtbl_index*2+1 instead of function fptr)
	OPC_FCALL_DG,
	// call virtual method (uimm24 vtbl index)
	OPC_VCALL,
	// call native static func (uimm24 index)
	OPC_NCALL,
	// call native method via static func (uimm24 index)
	// this is the same as NCALL but gives a hint to JIT to restore thisPtr in stack
	OPC_NMCALL,
	// call builtin native static func (uimm24 index)
	OPC_BCALL,
	// similar to NMCALL but for builtins
	OPC_BMCALL,
	// builtin call with trap (used for int64 divide ops)
	OPC_BCALL_TRAP,
	// call native virtual func (uimm24 vtbl index)
	OPC_NVCALL,
	// return from function/method; simply pop ret addr
	// think about the arm way with LR to simplify tailcalls?
	OPC_RET,

	// load this from top of stack, save old this there
	OPC_LOADTHIS,
	// immed version, just load this from top of stack
	OPC_LOADTHIS_IMM,
	// we need to differentiate pushthis for saving and pushthis for temporary load
	// so that we can analyze if this belong to a function in callstack
	OPC_PUSHTHIS,
	OPC_PUSHTHIS_TEMP,
	OPC_POPTHIS,

	// switch table
	// expects: adjusted uint value, uimm24 range, table follows this instruction
	OPC_SWITCH,

	// halt operation
	OPC_HALT,
	// reserved for breakpoint patching
	OPC_BREAK,

	// check stack (uimm24)
	OPC_CHKSTK,

	// intrinsics:
	OPC_FSQRT,
	OPC_DSQRT,

	OPC_MAX
};

// used for 64-bit integer emulation
enum VmOpCodeEmulated
{
	OPC_PLOAD64 = OPC_BCALL + 256*BUILTIN_PLOAD64,
	OPC_GLOAD64 = OPC_BCALL + 256*BUILTIN_GLOAD64,
	OPC_LPUSH64 = OPC_BCALL + 256*BUILTIN_LPUSH64,
	OPC_LSTORE64 = OPC_BCALL + 256*BUILTIN_LSTORE64,
	OPC_LSTORE64_NP = OPC_BCALL + 256*BUILTIN_LSTORE64_NP,
	OPC_GSTORE64 = OPC_BCALL + 256*BUILTIN_GSTORE64,
	OPC_GSTORE64_NP = OPC_BCALL + 256*BUILTIN_GSTORE64_NP,
	OPC_PSTORE64_IMM0 = OPC_BCALL + 256*BUILTIN_PSTORE64_IMM0,
	OPC_PSTORE64_IMM0_NP = OPC_BCALL + 256*BUILTIN_PSTORE64_IMM0_NP,
	OPC_PINC64 = OPC_BCALL + 256*BUILTIN_PINC64,
	OPC_PINC64_POST = OPC_BCALL + 256*BUILTIN_PINC64_POST,
	OPC_PUSH_LCONST = OPC_BCALL + 256*BUILTIN_PUSH_LCONST,
	OPC_PUSHC_LCONST = OPC_BCALL + 256*BUILTIN_PUSHC_LCONST,

	OPC_LADD = OPC_BCALL + 256*BUILTIN_LADD,
	OPC_LSUB = OPC_BCALL + 256*BUILTIN_LSUB,
	OPC_LMUL = OPC_BCALL + 256*BUILTIN_LMUL,
	OPC_LMOD = OPC_BCALL_TRAP + 256*BUILTIN_LMOD,
	OPC_ULMOD = OPC_BCALL_TRAP + 256*BUILTIN_ULMOD,
	OPC_LDIV = OPC_BCALL_TRAP + 256*BUILTIN_LDIV,
	OPC_ULDIV = OPC_BCALL_TRAP + 256*BUILTIN_ULDIV,
	OPC_LSAR = OPC_BCALL + 256*BUILTIN_LSAR,
	OPC_LSHR = OPC_BCALL + 256*BUILTIN_LSHR,
	OPC_LAND = OPC_BCALL + 256*BUILTIN_LAND,
	OPC_LOR = OPC_BCALL + 256*BUILTIN_LOR,
	OPC_LXOR = OPC_BCALL + 256*BUILTIN_LXOR,
	OPC_LSHL = OPC_BCALL + 256*BUILTIN_LSHL,

	OPC_LCMPEQ = OPC_BCALL + 256*BUILTIN_LCMPEQ,
	OPC_LCMPNE = OPC_BCALL + 256*BUILTIN_LCMPNE,
	OPC_LCMPLT = OPC_BCALL + 256*BUILTIN_LCMPLT,
	OPC_ULCMPLT = OPC_BCALL + 256*BUILTIN_ULCMPLT,
	OPC_LCMPLE = OPC_BCALL + 256*BUILTIN_LCMPLE,
	OPC_ULCMPLE = OPC_BCALL + 256*BUILTIN_ULCMPLE,
	OPC_LCMPGT = OPC_BCALL + 256*BUILTIN_LCMPGT,
	OPC_ULCMPGT = OPC_BCALL + 256*BUILTIN_ULCMPGT,
	OPC_LCMPGE = OPC_BCALL + 256*BUILTIN_LCMPGE,
	OPC_ULCMPGE = OPC_BCALL + 256*BUILTIN_ULCMPGE,

	OPC_LNEG = OPC_BCALL + 256*BUILTIN_LNEG,
	OPC_LNOT = OPC_BCALL + 256*BUILTIN_LNOT,
	OPC_LCMPZ = OPC_BCALL + 256*BUILTIN_LCMPZ,
	OPC_LCMPNZ = OPC_BCALL + 256*BUILTIN_LCMPNZ,

	// conversions!
	OPC_CONV_LTOI = OPC_BCALL + 256*BUILTIN_CONV_LTOI,
	OPC_CONV_LTOUI = OPC_CONV_LTOI,
	OPC_CONV_ITOL = OPC_BCALL + 256*BUILTIN_CONV_ITOL,
	OPC_CONV_UITOL = OPC_BCALL + 256*BUILTIN_CONV_UITOL,
	OPC_CONV_LTOF = OPC_BCALL + 256*BUILTIN_CONV_LTOF,
	OPC_CONV_LTOD = OPC_BCALL + 256*BUILTIN_CONV_LTOD,
	OPC_CONV_ULTOF = OPC_BCALL + 256*BUILTIN_CONV_ULTOF,
	OPC_CONV_ULTOD = OPC_BCALL + 256*BUILTIN_CONV_ULTOD,
	OPC_CONV_FTOL = OPC_BCALL + 256*BUILTIN_CONV_FTOL,
	OPC_CONV_FTOUL = OPC_BCALL + 256*BUILTIN_CONV_FTOUL,
	OPC_CONV_DTOL = OPC_BCALL + 256*BUILTIN_CONV_DTOL,
	OPC_CONV_DTOUL = OPC_BCALL + 256*BUILTIN_CONV_DTOUL,

	// new intrinsics
	OPC_INTRINSIC_BSF = OPC_BCALL + 256*BUILTIN_INTRIN_BSF,
	OPC_INTRINSIC_BSR = OPC_BCALL + 256*BUILTIN_INTRIN_BSR,
	OPC_INTRINSIC_POPCNT = OPC_BCALL + 256*BUILTIN_INTRIN_POPCNT,
	OPC_INTRINSIC_BSWAP = OPC_BCALL + 256*BUILTIN_INTRIN_BSWAP,
	OPC_INTRINSIC_BSFL = OPC_BCALL + 256*BUILTIN_INTRIN_BSFL,
	OPC_INTRINSIC_BSRL = OPC_BCALL + 256*BUILTIN_INTRIN_BSRL,
	OPC_INTRINSIC_POPCNTL = OPC_BCALL + 256*BUILTIN_INTRIN_POPCNTL,
	OPC_INTRINSIC_BSWAPL = OPC_BCALL + 256*BUILTIN_INTRIN_BSWAPL
};

enum Operands
{
	OPN_NONE,
	OPN_I24,
	// branch
	OPN_I24_BR,
	// native call
	OPN_I24_NC,
	OPN_U24,
	OPN_I8_U8_U8,
	OPN_U8_U8,
	OPN_I16_U8,
	OPN_U16_U8
};

}
