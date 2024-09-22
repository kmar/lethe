#include "Vm.h"
#include "JitX86/VmJitX86.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/TypeInfo/BaseObject.h>
#include <Lethe/Script/ScriptEngine.h>

#include <Lethe/Core/String/StringBuilder.h>
#include <Lethe/Core/Sys/Path.h>
#include <Lethe/Core/Math/Math.h>
#include <stdio.h>

namespace lethe
{

LETHE_NOINLINE static void MemSwap_NoInline(void *dst, void *src, size_t count)
{
	MemSwap(dst, src, count);
}

// Vm

Vm::Vm()
	: stack(nullptr)
	, prog(nullptr)
	, execFlags(0)
{
	LETHE_COMPILE_ASSERT(OPC_MAX <= 256);
}

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

template<bool dg>
LETHE_NOINLINE ExecResult Vm::DoFCall(const Instruction *&iptr, Stack &stk)
{
	const void *newPtr = stk.GetPtr(0);

	if (!newPtr)
		return RuntimeException(iptr, "function refptr is null");

	stk.Pop(1);
	stk.PushPtr(iptr);

	if (dg)
	{
		auto vidx = (UIntPtr)newPtr;

		// if LSBit is 1, it's not an actual pointer but rather a vtbl index
		if (vidx & 1)
		{
			vidx &= 0xffffffffu;
			vidx >>= 2;
			const void * const *vtbl = *static_cast<const void * const * const *>(&static_cast<const BaseObject *>(stk.GetThis())->scriptVtbl);
			newPtr = reinterpret_cast<const Instruction * const *>(vtbl)[vidx];
		}
		else
		{
			vidx &= ~(UIntPtr)3;
			newPtr = (const void *)vidx;
		}
	}

	iptr = static_cast<const Instruction *>(newPtr);

	return EXEC_OK;
}

void Vm::SetStack(Stack *stk)
{
	stack = stk;

	if (stack && prog)
	{
		stack->prog = prog;
		stack->cpool = &prog->cpool;
	}
}

void Vm::SetProgram(CompiledProgram *prg)
{
	prog = prg;

	if (stack && prog)
	{
		stack->prog = prog;
		stack->cpool = &prog->cpool;
	}
}

ExecResult Vm::Execute(Int pc)
{
	const Instruction *iptr = prog->instructions.GetData() + pc;
	return ExecutePtr(iptr);
}

ExecResult Vm::ExecutePtr(const void *adr)
{
	auto iptr = static_cast<const Instruction *>(adr);

	switch(execFlags & EXEC_MASK)
	{
	case 0:
	case EXEC_NO_BREAK:
		break;

	case EXEC_DEBUG:
	case EXEC_DEBUG | EXEC_NO_BREAK:
		{
			bool hitBreakpoint = false;
			bool noBreakMode = (execFlags & EXEC_NO_BREAK) != 0;

			for (;;)
			{
				Int oldBreakExecution = Atomic::Load(stack->breakExecution);
				bool skipBreakCall = false;

				if (hitBreakpoint)
				{
					// temporarily remove breakpoint
					auto pc = (Int)IntPtr(iptr - prog->instructions.GetData());

					// check if breakpoint still valid => debugger could have toggled breakpoint in the meantime
					if ((prog->instructions[pc] & 255) == OPC_BREAK)
					{
						prog->instructions[pc] &= ~255;
						prog->instructions[pc] |= prog->savedOpcodes[pc];

						Atomic::Store(stack->breakExecution, 1);
					}
					else
						hitBreakpoint = false;
				}

				auto res = !noBreakMode ? ExecuteTemplate<EXEC_DEBUG>(iptr) : ExecuteTemplate<EXEC_DEBUG|EXEC_NO_BREAK>(iptr);

				if (hitBreakpoint)
				{
					// restore breakpoint
					auto pc = (Int)IntPtr(iptr - prog->instructions.GetData());

					prog->instructions[pc] &= ~255;
					prog->instructions[pc] |= OPC_BREAK;
					Atomic::Store(stack->breakExecution, oldBreakExecution);

					// this should take care of F5 in debugger to not break twice
					if (!oldBreakExecution)
						skipBreakCall = true;
				}

				if (res < EXEC_EXCEPTION)
				{
					if (!noBreakMode)
						Atomic::Store(stack->breakExecution, 0);

					return res;
				}

				if (noBreakMode && res == EXEC_EXCEPTION)
					return res;

				hitBreakpoint = res == EXEC_BREAKPOINT || res == EXEC_EXCEPTION;

				if (hitBreakpoint)
				{
					Atomic::Store(stack->breakExecution, 1);
					noBreakMode = false;
				}

				iptr = static_cast<const Instruction *>(stack->GetProgram().instructions.GetData() + stack->programCounter);
				stack->SetInsPtr(iptr);

				// handle debug break state
				if (!skipBreakCall && onDebugBreak(*stack->context, res))
					return res;
			}
		}
	}

	return ExecuteTemplate<0>(iptr);
}

#define VM_DEBUG_CHECK_PTR(x) \
	LETHE_ASSERT(stk.GetPtr(x)); \
	if constexpr (flags & EXEC_DEBUG) if (!stk.GetPtr(x)) return RuntimeException(iptr-1, "null pointer dereference");

template<Int flags>
ExecResult Vm::ExecuteTemplate(const Instruction *iptr)
{
	ConstPool &cpool = prog->cpool;
	Stack &stk = *stack;
	stk.cpool = &cpool;

	for (;;)
	{
		const Int ins = *iptr++;

		switch((Byte)ins)
		{
		case OPC_PUSH_ICONST:
			stk.PushInt(DecodeImm24(ins));
			break;

		case OPC_PUSHC_ICONST:
			stk.PushInt(cpool.iPool[DecodeUImm24(ins)]);
			break;

		case OPC_PUSH_FCONST:
			stk.PushFloat((Float)DecodeImm24(ins));
			break;

		case OPC_PUSHC_FCONST:
			stk.PushFloat(cpool.fPool[DecodeUImm24(ins)]);
			break;

		case OPC_PUSH_DCONST:
			stk.PushDouble((Double)DecodeImm24(ins));
			break;

		case OPC_PUSHC_DCONST:
			stk.PushDouble(cpool.dPool[DecodeUImm24(ins)]);
			break;

		case OPC_LPUSH32:
		case OPC_LPUSH32F:
			stk.PushInt(stk.GetInt(DecodeUImm24(ins)));
			break;

		case OPC_LPUSH64D:
			stk.PushDouble(stk.GetDouble(DecodeUImm24(ins)));
			break;

		case OPC_LPUSHADR:
			stk.PushPtr(stk.GetTop() + DecodeUImm24(ins));
			break;

		case OPC_LPUSHPTR:
			stk.PushPtr(stk.GetPtr(DecodeUImm24(ins)));
			break;

		case OPC_LPUSH32_ICONST:
			stk.PushInt(stk.GetInt(DecodeUImm8(ins, 0)));
			stk.PushInt(DecodeImm16Top(ins));
			break;

		case OPC_LPUSH32_CICONST:
			stk.PushInt(stk.GetInt(DecodeUImm8(ins, 0)));
			stk.PushInt(cpool.iPool[DecodeUImm16Top(ins)]);
			break;

		case OPC_GLOAD32:
		case OPC_GLOAD32F:
			stk.PushInt(*reinterpret_cast<const UInt *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GLOAD64D:
			stk.PushDouble(*reinterpret_cast<const Double *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GLOADPTR:
			stk.PushPtr(*reinterpret_cast<const void **>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GSTORE32:
		case OPC_GSTORE32F:
			*reinterpret_cast<UInt *>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetInt(0);
			stk.Pop(1);
			break;

		case OPC_GSTORE64D:
			*reinterpret_cast<Double *>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetDouble(0);
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_GSTORE32_NP:
		case OPC_GSTORE32F_NP:
			*reinterpret_cast<UInt *>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetInt(0);
			break;

		case OPC_GSTORE64D_NP:
			*reinterpret_cast<Double *>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetDouble(0);
			break;

		case OPC_GSTOREPTR:
			*reinterpret_cast<const void **>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetPtr(0);
			stk.Pop(1);
			break;

		case OPC_GSTOREPTR_NP:
			*reinterpret_cast<const void **>(cpool.data.GetData() + DecodeUImm24(ins)) = stk.GetPtr(0);
			break;

		case OPC_GLOAD8:
			stk.PushInt(*reinterpret_cast<const SByte *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GLOAD8U:
			stk.PushInt(*reinterpret_cast<const Byte *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GLOAD16:
			stk.PushInt(*reinterpret_cast<const Short *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GLOAD16U:
			stk.PushInt(*reinterpret_cast<const UShort *>(cpool.data.GetData() + DecodeUImm24(ins)));
			break;

		case OPC_GSTORE8:
			*reinterpret_cast<Byte *>(cpool.data.GetData() + DecodeUImm24(ins)) = (Byte)stk.GetInt(0);
			stk.Pop(1);
			break;

		case OPC_GSTORE8_NP:
			*reinterpret_cast<Byte *>(cpool.data.GetData() + DecodeUImm24(ins)) = (Byte)stk.GetInt(0);
			break;

		case OPC_GSTORE16:
			*reinterpret_cast<UShort *>(cpool.data.GetData() + DecodeUImm24(ins)) = (UShort)stk.GetInt(0);
			stk.Pop(1);
			break;

		case OPC_GSTORE16_NP:
			*reinterpret_cast<UShort *>(cpool.data.GetData() + DecodeUImm24(ins)) = (UShort)stk.GetInt(0);
			break;

		case OPC_GLOADADR:
			stk.PushPtr(cpool.data.GetData() + DecodeUImm24(ins));
			break;

		case OPC_PUSH_FUNC:
			stk.PushPtr(iptr + DecodeImm24(ins));
			break;

		case OPC_POP:
			stk.Pop(DecodeUImm24(ins));
			break;

		case OPC_LPUSH8:
			stk.PushInt(*reinterpret_cast<const SByte *>(stk.GetTop() + DecodeUImm24(ins)));
			break;

		case OPC_LPUSH8U:
			stk.PushInt(*reinterpret_cast<const Byte *>(stk.GetTop() + DecodeUImm24(ins)));
			break;

		case OPC_LPUSH16:
			stk.PushInt(*reinterpret_cast<const Short *>(stk.GetTop() + DecodeUImm24(ins)));
			break;

		case OPC_LPUSH16U:
			stk.PushInt(*reinterpret_cast<const UShort *>(stk.GetTop() + DecodeUImm24(ins)));
			break;

		case OPC_LSTORE8:
			*reinterpret_cast<Byte *>(stk.GetTop() + DecodeUImm24(ins)) = (Byte)stk.GetInt(0);
			stk.Pop(1);
			break;

		case OPC_LSTORE8_NP:
			*reinterpret_cast<Byte *>(stk.GetTop() + DecodeUImm24(ins)) = (Byte)stk.GetInt(0);
			break;

		case OPC_LSTORE16:
			*reinterpret_cast<UShort *>(stk.GetTop() + DecodeUImm24(ins)) = (UShort)stk.GetInt(0);
			stk.Pop(1);
			break;

		case OPC_LSTORE16_NP:
			*reinterpret_cast<UShort *>(stk.GetTop() + DecodeUImm24(ins)) = (UShort)stk.GetInt(0);
			break;

		case OPC_LSTORE32:
		case OPC_LSTORE32F:
			stk.SetInt(DecodeUImm24(ins), stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_LSTORE64D:
			stk.SetDouble(DecodeUImm24(ins), stk.GetDouble(0));
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_LSTORE32_NP:
		case OPC_LSTORE32F_NP:
			stk.SetInt(DecodeUImm24(ins), stk.GetInt(0));
			break;

		case OPC_LSTORE64D_NP:
			stk.SetDouble(DecodeUImm24(ins), stk.GetDouble(0));
			break;

		case OPC_LSTOREPTR:
			stk.SetPtr(DecodeUImm24(ins), stk.GetPtr(0));
			stk.Pop(1);
			break;

		case OPC_LSTOREPTR_NP:
			stk.SetPtr(DecodeUImm24(ins), stk.GetPtr(0));
			break;

		case OPC_LMOVE32:
			stk.SetInt(DecodeUImm8(ins, 0), stk.GetInt(DecodeUImm8(ins, 1)));
			break;

		case OPC_LMOVEPTR:
			stk.SetPtr(DecodeUImm8(ins, 0), stk.GetPtr(DecodeUImm8(ins, 1)));
			break;

		case OPC_LSWAPPTR:
		{
			auto p0 = stk.GetPtr(0);
			auto p1 = stk.GetPtr(1);
			stk.SetPtr(0, p1);
			stk.SetPtr(1, p0);
		}
		break;

		case OPC_RANGE_ICONST:
			if (stk.GetInt(0) >= (UInt)DecodeUImm24(ins))
				return RuntimeException(iptr-1, "array index out of bounds");

			break;

		case OPC_RANGE_CICONST:
			if (stk.GetInt(0) >= (UInt)cpool.iPool[DecodeUImm24(ins)])
				return RuntimeException(iptr-1, "array index out of bounds");

			break;

		case OPC_RANGE:
		{
			auto idx = stk.GetInt(0);

			if (idx >= stk.GetInt(1))
				return RuntimeException(iptr-1, "array index out of bounds");

			stk.SetInt(1, idx);
			stk.Pop(1);
		}
		break;

		case OPC_PLOAD8:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetInt(1, *reinterpret_cast<const SByte *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD8U:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetInt(1, *reinterpret_cast<const Byte *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD16:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetInt(1, *reinterpret_cast<const Short *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD16U:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetInt(1, *reinterpret_cast<const UShort *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD32:
		case OPC_PLOAD32F:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetInt(1, *reinterpret_cast<const UInt *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD64D:
		{
			VM_DEBUG_CHECK_PTR(1);
			auto val = *reinterpret_cast<const Double *>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins));
			stk.Pop(2);
			stk.PushDouble(val);
			break;
		}

		case OPC_PLOADPTR:
			VM_DEBUG_CHECK_PTR(1);
			stk.SetPtr(1, *reinterpret_cast<const void **>((UIntPtr)stk.GetPtr(1) + (UIntPtr)stk.GetInt(0)*DecodeUImm24(ins)));
			stk.Pop(1);
			break;

		case OPC_PLOAD8_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, *reinterpret_cast<const SByte *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PLOAD8U_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, *reinterpret_cast<const Byte *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PLOAD16_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, *reinterpret_cast<const Short *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PLOAD16U_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, *reinterpret_cast<const UShort *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PLOAD32_IMM:
		case OPC_PLOAD32F_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, *reinterpret_cast<const UInt *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PLOAD64D_IMM:
		{
			VM_DEBUG_CHECK_PTR(0);
			auto val = *reinterpret_cast<const Double *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins));
			stk.Pop(1);
			stk.PushDouble(val);
			break;
		}

		case OPC_PLOADPTR_IMM:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetPtr(0, *reinterpret_cast<const void **>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)));
			break;

		case OPC_PSTORE8_IMM:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<Byte *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = (Byte)stk.GetInt(1);
			stk.Pop(2);
			break;

		case OPC_PSTORE8_IMM_NP:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<Byte *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = (Byte)stk.GetInt(1);
			stk.Pop(1);
			break;

		case OPC_PSTORE16_IMM:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<UShort *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = (UShort)stk.GetInt(1);
			stk.Pop(2);
			break;

		case OPC_PSTORE16_IMM_NP:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<UShort *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = (UShort)stk.GetInt(1);
			stk.Pop(1);
			break;

		case OPC_PSTORE32_IMM:
		case OPC_PSTORE32F_IMM:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<UInt *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetInt(1);
			stk.Pop(2);
			break;

		case OPC_PSTORE32_IMM_NP:
		case OPC_PSTORE32F_IMM_NP:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<UInt *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetInt(1);
			stk.Pop(1);
			break;

		case OPC_PSTORE64D_IMM:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<Double *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetDouble(1);
			stk.Pop(1+Stack::DOUBLE_WORDS);
			break;

		case OPC_PSTORE64D_IMM_NP:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<Double *>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetDouble(1);
			stk.Pop(1);
			break;

		case OPC_PSTOREPTR_IMM:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<const void **>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetPtr(1);
			stk.Pop(2);
			break;

		case OPC_PSTOREPTR_IMM_NP:
			VM_DEBUG_CHECK_PTR(0);
			*reinterpret_cast<const void **>((UIntPtr)stk.GetPtr(0) + DecodeImm24(ins)) = stk.GetPtr(1);
			stk.Pop(1);
			break;

		case OPC_PINC8:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, SByte((*reinterpret_cast<SByte *>(stk.GetPtr(0))) += (SByte)DecodeImm24(ins)));
			break;

		case OPC_PINC8U:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, Byte((*reinterpret_cast<Byte *>(stk.GetPtr(0))) += (Byte)DecodeImm24(ins)));
			break;

		case OPC_PINC16:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, Short((*reinterpret_cast<Short *>(stk.GetPtr(0))) += (Short)DecodeImm24(ins)));
			break;

		case OPC_PINC16U:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, UShort((*reinterpret_cast<UShort *>(stk.GetPtr(0))) += (UShort)DecodeImm24(ins)));
			break;

		case OPC_PINC32:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetInt(0, (*reinterpret_cast<UInt *>(stk.GetPtr(0))) += DecodeImm24(ins));
			break;

		case OPC_PINC32F:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetFloat(0, (*reinterpret_cast<Float *>(stk.GetPtr(0))) += DecodeImm24(ins));
			break;

		case OPC_PINC64D:
		{
			VM_DEBUG_CHECK_PTR(0);
			auto val = (*reinterpret_cast<Double *>(stk.GetPtr(0))) += DecodeImm24(ins);
			stk.Pop(1);
			stk.PushDouble(val);
			break;
		}

		case OPC_PINC8_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			SByte *tmp = reinterpret_cast<SByte *>(stk.GetPtr(0));
			SByte val = *tmp;
			(*tmp) += (SByte)DecodeImm24(ins);
			stk.SetInt(0, val);
		}
		break;

		case OPC_PINC8U_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			Byte *tmp = reinterpret_cast<Byte *>(stk.GetPtr(0));
			Byte val = *tmp;
			(*tmp) += (Byte)DecodeImm24(ins);
			stk.SetInt(0, val);
		}
		break;

		case OPC_PINC16_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			Short *tmp = reinterpret_cast<Short *>(stk.GetPtr(0));
			Short val = *tmp;
			(*tmp) += (Short)DecodeImm24(ins);
			stk.SetInt(0, val);
		}
		break;

		case OPC_PINC16U_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			UShort *tmp = reinterpret_cast<UShort *>(stk.GetPtr(0));
			UShort val = *tmp;
			(*tmp) += (UShort)DecodeImm24(ins);
			stk.SetInt(0, val);
		}
		break;

		case OPC_PINC32_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			UInt *tmp = reinterpret_cast<UInt *>(stk.GetPtr(0));
			UInt val = *tmp;
			(*tmp) +=  DecodeImm24(ins);
			stk.SetInt(0, val);
		}
		break;

		case OPC_PINC32F_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			Float *tmp = reinterpret_cast<Float *>(stk.GetPtr(0));
			Float val = *tmp;
			(*tmp) += DecodeImm24(ins);
			stk.SetFloat(0, val);
		}
		break;

		case OPC_PINC64D_POST:
		{
			VM_DEBUG_CHECK_PTR(0);
			auto tmp = reinterpret_cast<Double *>(stk.GetPtr(0));
			auto val = *tmp;
			(*tmp) += DecodeImm24(ins);
			stk.Pop(1);
			stk.PushDouble(val);
		}
		break;

		case OPC_PCOPY:
			VM_DEBUG_CHECK_PTR(0);
			VM_DEBUG_CHECK_PTR(1);
			MemCpy(stk.GetPtr(0), stk.GetPtr(1), DecodeUImm24(ins));
			stk.Pop(2);
			break;

		case OPC_PCOPY_REV:
			VM_DEBUG_CHECK_PTR(0);
			VM_DEBUG_CHECK_PTR(1);
			MemCpy(stk.GetPtr(1), stk.GetPtr(0), DecodeUImm24(ins));
			stk.Pop(2);
			break;

		case OPC_PCOPY_NP:
			VM_DEBUG_CHECK_PTR(0);
			VM_DEBUG_CHECK_PTR(1);
			MemCpy(stk.GetPtr(0), stk.GetPtr(1), DecodeUImm24(ins));
			stk.Pop(1);
			break;

		case OPC_PSWAP:
			VM_DEBUG_CHECK_PTR(0);
			VM_DEBUG_CHECK_PTR(1);
			MemSwap_NoInline(stk.GetPtr(0), stk.GetPtr(1), DecodeUImm24(ins));
			stk.Pop(2);
			break;

		case OPC_PUSH_RAW:
			stk.PushRaw(DecodeUImm24(ins));
			break;

		case OPC_PUSHZ_RAW:
			stk.PushRawZero(DecodeUImm24(ins));
			break;

		case OPC_AADD:
			VM_DEBUG_CHECK_PTR(1);
			stk.GetTop()[1] += (UIntPtr)stk.GetInt(0) * DecodeUImm24(ins);
			stk.Pop(1);
			break;

		case OPC_LAADD:
			VM_DEBUG_CHECK_PTR(0);
			stk.GetTop()[0] += (UIntPtr)stk.GetInt(DecodeUImm16Top(ins)) * DecodeUImm8(ins, 0);
			break;

		case OPC_AADD_ICONST:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetPtr(0, static_cast<const Byte *>(stk.GetPtr(0)) + DecodeImm24(ins));
			break;

		case OPC_AADDH_ICONST:
			VM_DEBUG_CHECK_PTR(0);
			stk.SetPtr(0, static_cast<const Byte *>(stk.GetPtr(0)) + ((size_t)(UInt)DecodeImm24(ins) << 16));
			break;

		case OPC_IADD:
			stk.SetInt(+1, stk.GetInt(+1) + stk.GetInt(+0));
			stk.Pop(1);
			break;

		case OPC_ISUB:
			stk.SetInt(+1, stk.GetInt(+1) - stk.GetInt(+0));
			stk.Pop(1);
			break;

		case OPC_IMUL:
			stk.SetInt(+1, stk.GetSignedInt(+1) * stk.GetSignedInt(+0));
			stk.Pop(1);
			break;

		case OPC_IMUL_ICONST:
			stk.SetInt(+0, stk.GetSignedInt(+0) * DecodeImm24(ins));
			break;

		case OPC_IDIV:
		{
			Int div = stk.GetSignedInt(+0);

			if (!div)
				return RuntimeException(iptr-1, "divide by zero");

			stk.SetInt(+1, stk.GetSignedInt(+1) / div);
			stk.Pop(1);
		}
		break;

		case OPC_UIDIV:
		{
			UInt div = stk.GetInt(+0);

			if (!div)
				return RuntimeException(iptr-1, "divide by zero");

			stk.SetInt(+1, stk.GetInt(+1) / div);
			stk.Pop(1);
		}
		break;

		case OPC_IMOD:
		{
			Int div = stk.GetSignedInt(+0);

			if (!div)
				return RuntimeException(iptr-1, "divide by zero");

			stk.SetInt(+1, stk.GetSignedInt(+1) % div);
			stk.Pop(1);
		}
		break;

		case OPC_UIMOD:
		{
			UInt div = stk.GetInt(+0);

			if (!div)
				return RuntimeException(iptr-1, "divide by zero");

			stk.SetInt(+1, stk.GetInt(+1) % div);
			stk.Pop(1);
		}
		break;

		case OPC_IAND:
			stk.SetInt(+1, stk.GetInt(+1) & stk.GetInt(+0));
			stk.Pop(1);
			break;

		case OPC_IAND_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) & DecodeImm24(ins));
			break;

		case OPC_IOR:
			stk.SetInt(+1, stk.GetInt(+1) | stk.GetInt(+0));
			stk.Pop(1);
			break;

		case OPC_IOR_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) | DecodeImm24(ins));
			break;

		case OPC_IXOR:
			stk.SetInt(+1, stk.GetInt(+1) ^ stk.GetInt(+0));
			stk.Pop(1);
			break;

		case OPC_IXOR_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) ^ DecodeImm24(ins));
			break;

		case OPC_ISHL:
			stk.SetInt(+1, stk.GetInt(+1) << (stk.GetInt(+0) & 31u));
			stk.Pop(1);
			break;

		case OPC_ISHL_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) << (DecodeImm24(ins) & 31));
			break;

		case OPC_ISHR:
			stk.SetInt(+1, stk.GetInt(+1) >> (stk.GetInt(+0) & 31u));
			stk.Pop(1);
			break;

		case OPC_ISHR_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) >> (DecodeImm24(ins) & 31));
			break;

		case OPC_ISAR:
			stk.SetInt(+1, stk.GetSignedInt(+1) >> (Byte)(stk.GetInt(+0) & 31u));
			stk.Pop(1);
			break;

		case OPC_ISAR_ICONST:
			stk.SetInt(+0, stk.GetSignedInt(+0) >> (Byte)(DecodeImm24(ins) & 31));
			break;

		case OPC_FADD:
			stk.SetFloat(+1, stk.GetFloat(+1) + stk.GetFloat(+0));
			stk.Pop(1);
			break;

		case OPC_FADD_ICONST:
			stk.SetFloat(+0, stk.GetFloat(+0) + (Float)DecodeImm24(ins));
			break;

		case OPC_LFADD_ICONST:
			stk.SetFloat(DecodeUImm8(ins, 0), stk.GetFloat(DecodeUImm8(ins, 1)) + (Float)DecodeImm8Top(ins));
			break;

		case OPC_LFADD:
			stk.SetFloat(DecodeUImm8(ins, 0), stk.GetFloat(0) + stk.GetFloat(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_FSUB:
			stk.SetFloat(+1, stk.GetFloat(+1) - stk.GetFloat(+0));
			stk.Pop(1);
			break;

		case OPC_LFSUB:
			stk.SetFloat(DecodeUImm8(ins, 0), stk.GetFloat(0) - stk.GetFloat(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_FMUL:
			stk.SetFloat(+1, stk.GetFloat(+1) * stk.GetFloat(+0));
			stk.Pop(1);
			break;

		case OPC_LFMUL:
			stk.SetFloat(DecodeUImm8(ins, 0), stk.GetFloat(0) * stk.GetFloat(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_FDIV:
			stk.SetFloat(+1, stk.GetFloat(+1) / stk.GetFloat(+0));
			stk.Pop(1);
			break;

		case OPC_LFDIV:
			stk.SetFloat(DecodeUImm8(ins, 0), stk.GetFloat(0) / stk.GetFloat(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_DADD:
			stk.SetDouble(+Stack::DOUBLE_WORDS, stk.GetDouble(+Stack::DOUBLE_WORDS) + stk.GetDouble(+0));
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_DSUB:
			stk.SetDouble(+Stack::DOUBLE_WORDS, stk.GetDouble(+Stack::DOUBLE_WORDS) - stk.GetDouble(+0));
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_DMUL:
			stk.SetDouble(+Stack::DOUBLE_WORDS, stk.GetDouble(+Stack::DOUBLE_WORDS) * stk.GetDouble(+0));
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_DDIV:
			stk.SetDouble(+Stack::DOUBLE_WORDS, stk.GetDouble(+Stack::DOUBLE_WORDS) / stk.GetDouble(+0));
			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_ICMPZ:
			stk.SetInt(0, stk.GetInt(0) == 0u);
			break;

		case OPC_ICMPNZ:
			stk.SetInt(0, stk.GetInt(0) != 0u);
			break;

		case OPC_FCMPZ:
			stk.SetInt(0, stk.GetFloat(0) == 0);
			break;

		case OPC_FCMPNZ:
			stk.SetInt(0, stk.GetFloat(0) != 0);
			break;

		case OPC_DCMPZ:
		{
			auto val = stk.GetDouble(0) == 0;
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPNZ:
		{
			auto val = stk.GetDouble(0) != 0;
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_ICMPNZ_BZ:
		{
			UInt tmp = stk.GetInt(0) != 0u;
			stk.SetInt(0, tmp);

			if (!tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_ICMPNZ_BNZ:
		{
			UInt tmp = stk.GetInt(0) != 0u;
			stk.SetInt(0, tmp);

			if (tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_FCMPNZ_BZ:
		{
			UInt tmp = stk.GetFloat(0) != 0.0f;
			stk.SetInt(0, tmp);

			if (!tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_FCMPNZ_BNZ:
		{
			UInt tmp = stk.GetFloat(0) != 0.0f;
			stk.SetInt(0, tmp);

			if (tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_DCMPNZ_BZ:
		{
			UInt tmp = stk.GetDouble(0) != 0.0;
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(tmp);

			if (!tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_DCMPNZ_BNZ:
		{
			UInt tmp = stk.GetDouble(0) != 0.0;
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(tmp);

			if (tmp)
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
		}
		break;

		case OPC_ICMPEQ:
			stk.SetInt(1, stk.GetInt(1) == stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_ICMPNE:
			stk.SetInt(1, stk.GetInt(1) != stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_ICMPLT:
			stk.SetInt(1, stk.GetSignedInt(1) < stk.GetSignedInt(0));
			stk.Pop(1);
			break;

		case OPC_ICMPLE:
			stk.SetInt(1, stk.GetSignedInt(1) <= stk.GetSignedInt(0));
			stk.Pop(1);
			break;

		case OPC_ICMPGT:
			stk.SetInt(1, stk.GetSignedInt(1) > stk.GetSignedInt(0));
			stk.Pop(1);
			break;

		case OPC_ICMPGE:
			stk.SetInt(1, stk.GetSignedInt(1) >= stk.GetSignedInt(0));
			stk.Pop(1);
			break;

		case OPC_UICMPLT:
			stk.SetInt(1, stk.GetInt(1) < stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_UICMPLE:
			stk.SetInt(1, stk.GetInt(1) <= stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_UICMPGT:
			stk.SetInt(1, stk.GetInt(1) > stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_UICMPGE:
			stk.SetInt(1, stk.GetInt(1) >= stk.GetInt(0));
			stk.Pop(1);
			break;

		case OPC_FCMPEQ:
			stk.SetInt(1, stk.GetFloat(1) == stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_FCMPNE:
			stk.SetInt(1, stk.GetFloat(1) != stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_FCMPLT:
			stk.SetInt(1, stk.GetFloat(1) < stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_FCMPLE:
			stk.SetInt(1, stk.GetFloat(1) <= stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_FCMPGT:
			stk.SetInt(1, stk.GetFloat(1) > stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_FCMPGE:
			stk.SetInt(1, stk.GetFloat(1) >= stk.GetFloat(0));
			stk.Pop(1);
			break;

		case OPC_DCMPEQ:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) == stk.GetDouble(0);
			stk.Pop(2*Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPNE:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) != stk.GetDouble(0);
			stk.Pop(2 * Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPLT:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) < stk.GetDouble(0);
			stk.Pop(2 * Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPLE:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) <= stk.GetDouble(0);
			stk.Pop(2 * Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPGT:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) > stk.GetDouble(0);
			stk.Pop(2 * Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_DCMPGE:
		{
			auto val = stk.GetDouble(Stack::DOUBLE_WORDS) >= stk.GetDouble(0);
			stk.Pop(2 * Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_BR:
			iptr += DecodeImm24(ins);
			break;

		case OPC_IBZ_P:
			if (!stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(1);
			break;

		case OPC_IBNZ_P:
			if (stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(1);
			break;

		case OPC_FBZ_P:
			if (!stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(1);
			break;

		case OPC_FBNZ_P:
			if (stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(1);
			break;

		case OPC_DBZ_P:
			if (!stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_DBNZ_P:
			if (stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(Stack::DOUBLE_WORDS);
			break;

		case OPC_IBZ:
			if (!stk.GetInt(0))
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
			break;

		case OPC_IBNZ:
			if (stk.GetInt(0))
				iptr += DecodeImm24(ins);
			else
				stk.Pop(1);
			break;

		case OPC_IBEQ:
			if (stk.GetInt(1) == stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_IBNE:
			if (stk.GetInt(1) != stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_IBLT:
			if (stk.GetSignedInt(1) < stk.GetSignedInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_IBLE:
			if (stk.GetSignedInt(1) <= stk.GetSignedInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_IBGT:
			if (stk.GetSignedInt(1) > stk.GetSignedInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_IBGE:
			if (stk.GetSignedInt(1) >= stk.GetSignedInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_UIBLT:
			if (stk.GetInt(1) < stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_UIBLE:
			if (stk.GetInt(1) <= stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_UIBGT:
			if (stk.GetInt(1) > stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_UIBGE:
			if (stk.GetInt(1) >= stk.GetInt(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBEQ:
			if (stk.GetFloat(1) == stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBNE:
			if (stk.GetFloat(1) != stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBLT:
			if (stk.GetFloat(1) < stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBLE:
			if (stk.GetFloat(1) <= stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBGT:
			if (stk.GetFloat(1) > stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_FBGE:
			if (stk.GetFloat(1) >= stk.GetFloat(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2);
			break;

		case OPC_DBEQ:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) == stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2*Stack::DOUBLE_WORDS);
			break;

		case OPC_DBNE:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) != stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2 * Stack::DOUBLE_WORDS);
			break;

		case OPC_DBLT:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) < stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2 * Stack::DOUBLE_WORDS);
			break;

		case OPC_DBLE:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) <= stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2 * Stack::DOUBLE_WORDS);
			break;

		case OPC_DBGT:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) > stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2 * Stack::DOUBLE_WORDS);
			break;

		case OPC_DBGE:
			if (stk.GetDouble(Stack::DOUBLE_WORDS) >= stk.GetDouble(0))
				iptr += DecodeImm24(ins);

			stk.Pop(2 * Stack::DOUBLE_WORDS);
			break;

		case OPC_PCMPEQ:
			stk.PushInt(stk.GetPtr(0) == stk.GetPtr(1));
			break;

		case OPC_PCMPNE:
			stk.PushInt(stk.GetPtr(0) != stk.GetPtr(1));
			break;

		case OPC_PCMPZ:
			stk.PushInt(!stk.GetPtr(0));
			break;

		case OPC_PCMPNZ:
			stk.PushInt(stk.GetPtr(0) != nullptr);
			break;

		case OPC_IADD_ICONST:
			stk.SetInt(+0, stk.GetInt(+0) + DecodeImm24(ins));
			break;

		case OPC_LIADD_ICONST:
			stk.SetInt(DecodeUImm8(ins, 0), stk.GetInt(DecodeUImm8(ins, 1)) + DecodeImm8Top(ins));
			break;

		case OPC_LIADD:
			stk.SetInt(DecodeUImm8(ins, 0), stk.GetInt(0) + stk.GetInt(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_LISUB:
			stk.SetInt(DecodeUImm8(ins, 0), stk.GetInt(0) - stk.GetInt(DecodeUImm8(ins, 1)));
			stk.Pop(1);
			break;

		case OPC_CALL:
			stk.PushPtr(iptr);
			iptr += DecodeImm24(ins);
			break;

		case OPC_VCALL:
		{
			const void * const *vtbl = *static_cast<const void * const * const *>(&static_cast<const BaseObject *>(stk.GetThis())->scriptVtbl);
			stk.PushPtr(iptr);
			iptr = reinterpret_cast<const Instruction * const *>(vtbl)[DecodeImm24(ins)];
		}
		break;

		case OPC_FCALL:
		{
			auto res = DoFCall<false>(iptr, stk);

			if (res != EXEC_OK)
				return res;
		}
		break;

		// problem: this slows down interpreter a lot!
		case OPC_FCALL_DG:
		{
			auto res = DoFCall<true>(iptr, stk);

			if (res != EXEC_OK)
				return res;
		}
		break;

		case OPC_NCALL:
				// necessary because of callstack()
				// note: since this is a single intruction, we allow this even in non-JIT release mode
				//if (flags & EXEC_DEBUG)
					stk.SetInsPtr(iptr);

			// fall through
		case OPC_NMCALL:
			{
				// necessary for debugging to ignore breaks in nested script calls
				if constexpr (flags & EXEC_DEBUG)
					++stk.nesting;

				auto *savedRet = stk.GetPtr(-1);
				cpool.nFunc[DecodeUImm24(ins)](stk);
				stk.SetPtr(-1, savedRet);

				if constexpr (flags & EXEC_DEBUG)
					--stk.nesting;
			}
			break;

		case OPC_BCALL:
		case OPC_BMCALL:
			cpool.nFunc[DecodeUImm24(ins)](stk);
			break;

		case OPC_BCALL_TRAP:
			{
				auto *trapMsg = ((ConstPool::NativeCallbackTrap)(void *)cpool.nFunc[DecodeUImm24(ins)])(stk);

				if (trapMsg)
					return RuntimeException(iptr, trapMsg);
			}
			break;

		case OPC_RET:
		{
			Int ofs = DecodeUImm24(ins);
			iptr = static_cast<const Instruction *>(stk.GetPtr(ofs));
			stk.Pop(ofs + 1);
		}
		break;

		case OPC_LOADTHIS:
		{
			VM_DEBUG_CHECK_PTR(0);
			const void *old = stk.GetThis();
			stk.SetThis(stk.GetPtr(0));
			stk.SetPtr(0, old);
		}
		break;

		case OPC_LOADTHIS_IMM:
		{
			Int ofs = DecodeImm24(ins);
			VM_DEBUG_CHECK_PTR(ofs);
			stk.SetThis(stk.GetPtr(ofs));
		}
		break;

		case OPC_PUSHTHIS:
		case OPC_PUSHTHIS_TEMP:
			stk.PushPtr(stk.GetThis());
			break;

		case OPC_POPTHIS:
			stk.SetThis(stk.GetPtr(0));
			stk.Pop(1);
			break;

		case OPC_SWITCH:
		{
			const UInt idx = stk.GetInt(0);
			const UInt range = DecodeUImm24(ins);
			iptr += iptr[(1 + idx)*(idx < range)];
			stk.Pop(1);
		}
		break;

		case OPC_CONV_ITOF:
			stk.SetFloat(+0, (Float)(stk.GetSignedInt(+0)));
			break;

		case OPC_CONV_UITOF:
			stk.SetFloat(+0, (Float)(stk.GetInt(+0)));
			break;

		case OPC_CONV_FTOI:
			stk.SetInt(+0, (Int)(stk.GetFloat(+0)));
			break;

		case OPC_CONV_FTOUI:
			stk.SetInt(+0, WellDefinedFloatToUnsigned<UInt>(stk.GetFloat(+0)));
			break;

		case OPC_CONV_FTOD:
		{
			auto val = (Double)stk.GetFloat(+0);
			stk.Pop(1);
			stk.PushDouble(val);
			break;
		}

		case OPC_CONV_DTOF:
		{
			auto val = (Float)stk.GetDouble(+0);
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushFloat(val);
			break;
		}

		case OPC_CONV_ITOD:
		{
			auto val = (Double)(stk.GetSignedInt(+0));
			stk.Pop(1);
			stk.PushDouble(val);
			break;
		}

		case OPC_CONV_UITOD:
		{
			auto val = (Double)(stk.GetInt(+0));
			stk.Pop(1);
			stk.PushDouble(val);
			break;
		}

		case OPC_CONV_DTOI:
		{
			auto val = (Int)stk.GetDouble(+0);
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(val);
			break;
		}

		case OPC_CONV_DTOUI:
		{
			auto val = stk.GetDouble(+0);
			stk.Pop(Stack::DOUBLE_WORDS);
			stk.PushInt(WellDefinedFloatToUnsigned<UInt>(val));
			break;
		}

		case OPC_CONV_ITOS:
			stk.SetInt(+0, (UInt)((Short)stk.GetInt(+0)));
			break;

		case OPC_CONV_ITOSB:
			stk.SetInt(+0, (UInt)((SByte)stk.GetInt(+0)));
			break;

		case OPC_CONV_PTOB:
			stk.SetInt(+0, stk.GetPtr(0) != nullptr);
			break;

		case OPC_INEG:
			stk.SetInt(0, -stk.GetSignedInt(0));
			break;

		case OPC_INOT:
			stk.SetInt(0, ~stk.GetInt(0));
			break;

		case OPC_FNEG:
			stk.SetFloat(0, -stk.GetFloat(0));
			break;

		case OPC_DNEG:
			stk.SetDouble(0, -stk.GetDouble(0));
			break;

		case OPC_BREAK:
			stk.programCounter = static_cast<Int>(iptr - 1 - prog->instructions.GetData());
			return EXEC_BREAKPOINT;

		case OPC_HALT:
		case OPC_NVCALL:
			stk.programCounter = static_cast<Int>(iptr - 1 - prog->instructions.GetData());
			return EXEC_OK;

		case OPC_CHKSTK:
		{
			Int limit = DecodeUImm24(ins);

			if (!stk.Check(limit))
				return RuntimeException(iptr-1, "stack overflow");
		}
		break;

		case OPC_FSQRT:
			stk.SetFloat(0, Sqrt(stk.GetFloat(0)));
			break;

		case OPC_DSQRT:
			stk.SetDouble(0, Sqrt(stk.GetDouble(0)));
			break;

		default:
			LETHE_UNREACHABLE;
		}

		if constexpr ((flags & (EXEC_DEBUG | EXEC_NO_BREAK)) == EXEC_DEBUG)
		{
			// check for abort
			if (Atomic::Load(stk.breakExecution))
			{
				// set program counter
				stk.programCounter = static_cast<Int>(iptr - prog->instructions.GetData());
				return EXEC_BREAK;
			}
		}
	}
}

#undef VM_DEBUG_CHECK_PTR


}
