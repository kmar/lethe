#include "Builtin.h"
#include <Lethe/Script/Program/ConstPool.h>
#include "Vm.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/TypeInfo/BaseObject.h>
#include <Lethe/Script/ScriptEngine.h>

#include <Lethe/Core/Math/Math.h>
#include <Lethe/Core/Memory/Heap.h>
#include <Lethe/Core/Classes/ObjectHeap.h>

#include <Lethe/Core/Sys/Bits.h>

#include <Lethe/Core/Thread/Atomic.h>

namespace lethe
{

void Opcode_FMod(Stack &stk)
{
	auto res = FMod(stk.GetFloat(1), stk.GetFloat(0));
	stk.Pop(1);
	stk.SetFloat(0, res);
}

void Opcode_DMod(Stack &stk)
{
	auto res = FMod(stk.GetDouble(Stack::DOUBLE_WORDS), stk.GetDouble(0));
	stk.Pop(2*Stack::DOUBLE_WORDS);
	stk.PushDouble(res);
}

void Opcode_LDelStr0(Stack &stk)
{
	stk.DelString(0);
}

void Opcode_LDelStr1(Stack &stk)
{
	stk.DelString(1);
}

void Opcode_LDelStr(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.DelString(offset);
}

void Opcode_GDelStr(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	reinterpret_cast<const String *>(stk.GetConstantPool().data.GetData() + offset)->~String();
}

void Opcode_PDelStr_NP(Stack &stk)
{
	void *ptr = stk.GetPtr(0);
	String *vstr = static_cast<String *>(ptr);

	if (vstr)
		vstr->~String();
}

void Opcode_VDelStr(Stack &stk)
{
	Int count = stk.GetSignedInt(0);
	void *ptr = stk.GetPtr(1);
	stk.Pop(2);

	if (!ptr)
		return;

	String *vstr = static_cast<String *>(ptr);

	while (count-- > 0)
		(vstr + count)->~String();
}

void Opcode_PCopyStr(Stack &stk)
{
	void *dptr = stk.GetPtr(0);
	const void *sptr = stk.GetPtr(1);
	stk.Pop(2);
	String *dstr = static_cast<String *>(dptr);
	const String *sstr = static_cast<const String *>(sptr);
	*dstr = *sstr;
}

void Opcode_VCopyStr(Stack &stk)
{
	Int count = stk.GetSignedInt(0);
	void *ptr = stk.GetPtr(1);
	const void *sptr = stk.GetPtr(2);
	stk.Pop(3);
	String *vstr = static_cast<String *>(ptr);
	const String *sstr = static_cast<const String *>(sptr);

	for (Int i=0; i<count; i++)
		*vstr++ = *sstr++;
}

void Opcode_LPushName_Const(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.PushLong(stk.GetConstantPool().nPool[offset].GetValue());
}

void Opcode_GetStrChar(Stack &stk)
{
	auto sptr = static_cast<const String *>(stk.GetPtr(1));
	auto idx = stk.GetInt(0);
	stk.Pop(2);
	stk.PushInt(idx < (UInt)sptr->GetLength() ? Int((*sptr)[(Int)idx] & 255) : -1);
}

void Opcode_LPushStr_Const(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.PushString(stk.GetConstantPool().sPool[offset]);
}

void Opcode_APushStr_Const(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.PushPtr(&stk.GetConstantPool().sPool[offset]);
}

void Opcode_LPushStr(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.PushString(stk.GetString(offset));
}

void Opcode_GStrLoad(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.PushString(*reinterpret_cast<const String *>(stk.GetConstantPool().data.GetData() + offset));
}

void Opcode_GStrStore_NP(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	*reinterpret_cast<String *>(stk.GetConstantPool().GetGlobalData() + offset) = stk.GetString(0);
}

void Opcode_GStrStore(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	*reinterpret_cast<String *>(stk.GetConstantPool().GetGlobalData() + offset) = stk.GetString(0);
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
}

void Opcode_LStrStore_NP(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.SetString(offset, stk.GetString(0));
}

void Opcode_LStrStore(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	stk.Pop(1);
	stk.SetString(offset, stk.GetString(0));
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
}

void Opcode_LStrAdd(Stack &stk)
{
	stk.SetString(Stack::STRING_WORDS, stk.GetString(Stack::STRING_WORDS) + stk.GetString(+0));
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
}

static void Opcode_PStrAdd_Assign_Internal(Stack &stk, bool load)
{
	// on stack: string value, ptr_string
	String &dst = *static_cast<String *>(stk.GetPtr(Stack::STRING_WORDS));
	dst += stk.GetString(0);
	stk.DelString(0);
	stk.Pop(1 + Stack::STRING_WORDS);

	if (load)
		stk.PushString(dst);
}

void Opcode_PStrAdd_Assign(Stack &stk)
{
	Opcode_PStrAdd_Assign_Internal(stk, false);
}

void Opcode_PStrAdd_Assign_Load(Stack &stk)
{
	Opcode_PStrAdd_Assign_Internal(stk, true);
}

static inline void OpCode_SCmp_Cleanup(Stack &stk, UInt res)
{
	stk.DelString(0);
	stk.DelString(Stack::STRING_WORDS);
	stk.Pop(2*Stack::STRING_WORDS-1);
	stk.SetInt(0, res);
}

void Opcode_SCmpEq(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) == stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_SCmpNe(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) != stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_SCmpLt(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) < stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_SCmpLe(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) <= stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_SCmpGt(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) > stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_SCmpGe(Stack &stk)
{
	UInt res = stk.GetString(Stack::STRING_WORDS) >= stk.GetString(+0);
	OpCode_SCmp_Cleanup(stk, res);
}

void Opcode_Conv_CToS(Stack &stk)
{
	Int val = stk.GetSignedInt(0);
	stk.Pop(1);
	WChar wc[2] = { (WChar)val, 0 };
	stk.PushString(wc);
}

void Opcode_Conv_IToS(Stack &stk)
{
	Int val = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushString(String::Printf("%d", val));
}

void Opcode_Conv_UIToS(Stack &stk)
{
	UInt val = stk.GetInt(0);
	stk.Pop(1);
	stk.PushString(String::Printf("%u", val));
}

void Opcode_Conv_LToS(Stack &stk)
{
	auto val = stk.GetSignedLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushString(String::Printf(LETHE_FORMAT_LONG, val));
}

void Opcode_Conv_ULToS(Stack &stk)
{
	auto val = stk.GetLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushString(String::Printf(LETHE_FORMAT_ULONG, val));
}

void Opcode_Conv_FToS(Stack &stk)
{
	Float val = stk.GetFloat(0);
	stk.Pop(1);
	stk.PushString(String::Printf("%f", val));
}

void Opcode_Conv_DToS(Stack &stk)
{
	auto val = stk.GetDouble(0);
	stk.Pop(Stack::DOUBLE_WORDS);
	stk.PushString(String::Printf("%lf", val));
}

void Opcode_Conv_NToS(Stack &stk)
{
	auto nidx = stk.GetLong(0);
	stk.Pop(Stack::NAME_WORDS);
	Name n;
	n.SetValue(nidx);
	stk.PushString(n.ToString());
}

void Opcode_Conv_SToN(Stack &stk)
{
	auto idx = Name(stk.GetString(+0)).GetValue();
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
	stk.PushLong(idx);
}

void Opcode_Conv_SToBool(Stack &stk)
{
	bool res = !stk.GetString(+0).IsEmpty();
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
	stk.PushInt(res);
}

void Opcode_PLoadStr(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	void *ptr = stk.GetPtr(1);
	stk.Pop(2);
	stk.PushString(ptr ? *reinterpret_cast<const String *>
				   (reinterpret_cast<const Byte *>(ptr) + offset) : String());
}

void Opcode_PStrStore_Imm_NP(Stack &stk)
{
	UInt offset = stk.GetInt(0);
	void *ptr = stk.GetPtr(1);
	stk.Pop(2);
	const String &str = stk.GetString(0);

	if (ptr)
		*reinterpret_cast<String *>(reinterpret_cast<Byte *>(ptr) + offset) = str;
}

void Opcode_PStrStore_Imm(Stack &stk)
{
	Opcode_PStrStore_Imm_NP(stk);
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);
}

// note: opcode new pushes twice to simplify ctor call
Int Opcode_New_Internal(Stack &stk)
{
	ULong nidx = stk.GetLong(0);
	stk.Pop(Stack::NAME_WORDS);
	Name n;
	n.SetValue(nidx);

	auto dt = stk.GetProgram().FindClass(n);

	if (dt)
	{
		// FIXME: better
		auto ptr = ObjectHeap::Get().Alloc(dt->size, dt->align, dt->classNameGroupKey);
		MemSet(ptr, 0, dt->size);
		::new(ptr) BaseObject;
		stk.PushPtr(ptr);
		stk.PushPtr(ptr);
		auto obj = static_cast<BaseObject *>(ptr);
		// note: vtbl will be set externally
		obj->strongRefCount = 0;
		obj->weakRefCount = 1;
		return dt->funCtor;
	}

	stk.PushPtr(nullptr);
	stk.PushPtr(nullptr);
	return -1;
}

Int Opcode_New_Internal(Stack &stk, Name n, void *ptr)
{
	auto dt = stk.GetProgram().FindClass(n);

	if (dt)
	{
		MemSet(ptr, 0, dt->size);
		::new(ptr) BaseObject;
		stk.PushPtr(ptr);
		stk.PushPtr(ptr);
		auto obj = static_cast<BaseObject *>(ptr);
		// note: vtbl will be set externally
		obj->strongRefCount = 0;
		obj->weakRefCount = 1;
		return dt->funCtor;
	}

	stk.PushPtr(nullptr);
	stk.PushPtr(nullptr);
	return -1;
}

void Opcode_New(Stack &stk)
{
	Opcode_New_Internal(stk);
}

void Builtin::Opcode_New_Dynamic(Stack &stk)
{
	Int funCtor = Opcode_New_Internal(stk);

	if (funCtor < 0)
	{
		// note: not pushing null here because we'll skip fcall
		return;
	}

	auto jit = stk.GetProgram().jitRef;

	if (jit)
		stk.PushPtr(jit->GetCodePtr(funCtor));
	else
		stk.PushPtr(stk.GetProgram().instructions.GetData() + funCtor);
}

void Builtin::Opcode_New_Dynamic(Stack &stk, Name n, void *inst)
{
	Int funCtor = Opcode_New_Internal(stk, n, inst);

	if (funCtor < 0)
	{
		// note: not pushing null here because we'll skip fcall
		return;
	}

	auto jit = stk.GetProgram().jitRef;

	if (jit)
		stk.PushPtr(jit->GetCodePtr(funCtor));
	else
		stk.PushPtr(stk.GetProgram().instructions.GetData() + funCtor);
}

void Opcode_DecStrong(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
		stk.PushInt(Atomic::Decrement(obj->strongRefCount));
	else
		stk.PushInt(1);
}

void Opcode_TestWeakNull(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj && !Atomic::Load(obj->strongRefCount))
		stk.SetPtr(0, nullptr);
}

void Opcode_DecWeak(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
	{
		if (!Atomic::Decrement(obj->weakRefCount))
		{
			ObjectHeap::Get().Dealloc(obj);
			stk.SetPtr(0, nullptr);
		}
		else if (!Atomic::Load(obj->strongRefCount))
		{
			stk.SetPtr(0, nullptr);
		}
	}
}

void Opcode_StrongZero(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj && !Atomic::Decrement(obj->weakRefCount))
		ObjectHeap::Get().Dealloc(obj);

	stk.SetPtr(0, nullptr);
}

void Builtin::Opcode_AddStrong(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
		Atomic::Increment(obj->strongRefCount);
}

void Builtin::Opcode_AddStrongAfterNew(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
	{
		Atomic::Increment(obj->strongRefCount);
		auto *ctype = obj->GetScriptClassType();

		if (ctype)
			stk.GetContext().GetEngine().onNewObject(static_cast<BaseObject *>(obj), *ctype);
	}
}

void Opcode_AddWeak(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
		Atomic::Increment(obj->weakRefCount);
}

void Opcode_AddWeakNull(Stack &stk)
{
	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
	{
		if (!Atomic::Load(obj->strongRefCount))
			stk.SetPtr(0, nullptr);
		else
			Atomic::Increment(obj->weakRefCount);
	}
}

void Opcode_IsA(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	auto obj = static_cast<BaseObject *>(stk.GetPtr(Stack::NAME_WORDS));
	stk.Pop(1 + Stack::NAME_WORDS);

	if (obj)
		stk.PushInt(obj->GetScriptClassType()->IsA(n));
	else
	{
		// null incompatible with anything
		stk.PushInt(0);
	}
}

void Opcode_IsANoPop(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	auto obj = static_cast<BaseObject *>(stk.GetPtr(Stack::NAME_WORDS));
	stk.Pop(Stack::NAME_WORDS);

	if (obj)
		stk.PushInt(obj->GetScriptClassType()->IsA(n));
	else
	{
		// null incompatible with anything
		stk.PushInt(0);
	}
}

void Opcode_FixWeak(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	stk.Pop(Stack::NAME_WORDS);

	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj && (!Atomic::Load(obj->strongRefCount) || !obj->GetScriptClassType()->IsA(n)))
		stk.SetPtr(0, nullptr);
}

void Opcode_FixWeakRef(Stack &stk)
{
	auto pobj = static_cast<AtomicPointer<BaseObject>*>(stk.GetPtr(0));
	auto *obj = pobj->Load();

	if (!obj || Atomic::Load(obj->strongRefCount) != 0)
		return;

	if (auto *tmp = pobj->Exchange(nullptr))
		if (!Atomic::Decrement(tmp->weakRefCount))
			ObjectHeap::Get().Dealloc(tmp);
}

void Opcode_FixAddWeak(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	stk.Pop(Stack::NAME_WORDS);

	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
	{
		if (!Atomic::Load(obj->strongRefCount) || !obj->GetScriptClassType()->IsA(n))
			stk.SetPtr(0, nullptr);
		else
			Atomic::Increment(obj->weakRefCount);
	}
}

void Opcode_FixStrong(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	stk.Pop(Stack::NAME_WORDS);

	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj && !obj->GetScriptClassType()->IsA(n))
		stk.SetPtr(0, nullptr);
}

void Opcode_FixAddStrong(Stack &stk)
{
	Name n;
	n.SetValue(stk.GetLong(0));
	stk.Pop(Stack::NAME_WORDS);

	auto obj = static_cast<BaseObject *>(stk.GetPtr(0));

	if (obj)
	{
		if (!obj->GetScriptClassType()->IsA(n))
			stk.SetPtr(0, nullptr);
		else
			Atomic::Increment(obj->strongRefCount);
	}
}

void Opcode_CmpDG_Eq(Stack &stk)
{
	auto p0 = stk.GetPtr(0);
	auto p1 = stk.GetPtr(1);
	auto p2 = stk.GetPtr(2);
	auto p3 = stk.GetPtr(3);
	stk.Pop(4);
	stk.PushInt(p0 == p2 && p1 == p3);
}

void Opcode_CmpDG_Ne(Stack &stk)
{
	auto p0 = stk.GetPtr(0);
	auto p1 = stk.GetPtr(1);
	auto p2 = stk.GetPtr(2);
	auto p3 = stk.GetPtr(3);
	stk.Pop(4);
	stk.PushInt(p0 != p2 || p1 != p3);
}

void Opcode_Native_Ctor(Stack &stk)
{
	// stack: [type_idx] [inst_ptr]
	auto typeIdx = stk.GetSignedInt(0);
	stk.Pop(1);

	const auto &dt = stk.GetProgram().types[typeIdx];
	auto fptr = dt->nativeCtor;

	fptr(stk.GetPtr(0));
}

void Opcode_Native_Dtor(Stack &stk)
{
	// stack: [type_idx] [inst_ptr]
	auto typeIdx = stk.GetSignedInt(0);
	stk.Pop(1);

	const auto &dt = stk.GetProgram().types[typeIdx];
	auto fptr = dt->nativeDtor;

	fptr(stk.GetPtr(0));
}

void Builtin::OpCode_ProfEnter(Stack &stk)
{
	auto offset = stk.GetInt(0);
	stk.Pop(1);

	stk.GetContext().ProfEnter(offset);
}

void Builtin::OpCode_ProfExit(Stack &stk)
{
	auto offset = stk.GetInt(0);
	stk.Pop(1);

	stk.GetContext().ProfExit(offset);
}

// 64-bit integer emulation
void Opcode_PUSH_LCONST(Stack &stk)
{
	auto iconst = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushLong(iconst);
}

void Opcode_PUSHC_LCONST(Stack &stk)
{
	auto iconst = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushLong(stk.GetConstantPool().lPool[iconst]);
}

void Opcode_PLOAD64(Stack &stk)
{
	auto iconst = stk.GetInt(0);
	stk.Pop(1);
	auto ptr = (UIntPtr)stk.GetPtr(1);
	LETHE_ASSERT(ptr);
	auto val = ptr ? *reinterpret_cast<const ULong *>(ptr + (size_t)stk.GetInt(0)*iconst) : (ULong)0;
	stk.Pop(2);
	stk.PushLong(val);
}

void Opcode_GLOAD64(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushLong(*reinterpret_cast<const ULong *>(stk.GetConstantPool().data.GetData() + ofs));
}

void Opcode_LPUSH64(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushLong(stk.GetLong(ofs));
}

void Opcode_LSTORE64(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	auto val = stk.GetLong(0);
	stk.SetLong(ofs, val);
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LSTORE64_NP(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.SetLong(ofs, stk.GetLong(0));
}

void Opcode_GSTORE64(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	auto val = stk.GetLong(0);
	stk.Pop(Stack::LONG_WORDS);
	*reinterpret_cast<ULong *>(const_cast<Byte *>(stk.GetConstantPool().data.GetData()) + ofs) = val;
}

void Opcode_GSTORE64_NP(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);
	auto val = stk.GetLong(0);
	*reinterpret_cast<ULong *>(const_cast<Byte *>(stk.GetConstantPool().data.GetData()) + ofs) = val;
}

void Opcode_PSTORE64_IMM0(Stack &stk)
{
	auto ptr = (UIntPtr)stk.GetPtr(0);
	LETHE_ASSERT(ptr);

	if (ptr)
		*reinterpret_cast<ULong *>(ptr) = stk.GetLong(1);

	stk.Pop(1+Stack::LONG_WORDS);
}

void Opcode_PSTORE64_IMM0_NP(Stack &stk)
{
	auto ptr = (UIntPtr)stk.GetPtr(0);
	LETHE_ASSERT(ptr);

	if (ptr)
		*reinterpret_cast<ULong *>(ptr) = stk.GetLong(1);

	stk.Pop(1);
}

void Opcode_PINC64(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);

	auto ptr = (UIntPtr)stk.GetPtr(0);
	LETHE_ASSERT(ptr);

	ULong val = 0;

	if (ptr)
		val = (*reinterpret_cast<ULong *>(ptr) += (ULong)ofs);

	stk.Pop(1);
	stk.PushLong(val);
}

void Opcode_PINC64_POST(Stack &stk)
{
	auto ofs = stk.GetSignedInt(0);
	stk.Pop(1);

	auto ptr = (UIntPtr)stk.GetPtr(0);
	LETHE_ASSERT(ptr);

	ULong val = 0;

	if (ptr)
	{
		auto *adr = reinterpret_cast<ULong *>(ptr);
		val = *adr;
		*adr += (ULong)ofs;
	}

	stk.Pop(1);
	stk.PushLong(val);
}

void Opcode_LADD(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) + stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LSUB(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) - stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LMUL(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) * stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

const char *Opcode_LMOD(Stack &stk)
{
	LETHE_ASSERT(stk.GetSignedLong(0));

	if (!stk.GetSignedLong(0))
		return "divide by zero";

	stk.SetLong(Stack::LONG_WORDS, stk.GetSignedLong(Stack::LONG_WORDS) % stk.GetSignedLong(0));

	stk.Pop(Stack::LONG_WORDS);
	return nullptr;
}

const char *Opcode_ULMOD(Stack &stk)
{
	LETHE_ASSERT(stk.GetLong(0));

	if (!stk.GetLong(0))
		return "divide by zero";

	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) % stk.GetLong(0));

	stk.Pop(Stack::LONG_WORDS);
	return nullptr;
}

const char *Opcode_LDIV(Stack &stk)
{
	LETHE_ASSERT(stk.GetSignedLong(0));

	if (!stk.GetSignedLong(0))
		return "divide by zero";

	stk.SetLong(Stack::LONG_WORDS, stk.GetSignedLong(Stack::LONG_WORDS) / stk.GetSignedLong(0));

	stk.Pop(Stack::LONG_WORDS);
	return nullptr;
}

const char *Opcode_ULDIV(Stack &stk)
{
	LETHE_ASSERT(stk.GetLong(0));

	if (!stk.GetLong(0))
		return "divide by zero";

	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) / stk.GetLong(0));

	stk.Pop(Stack::LONG_WORDS);
	return nullptr;
}

void Opcode_LSAR(Stack &stk)
{
	stk.SetLong(1, stk.GetSignedLong(1) >> (stk.GetInt(0) & 63u));
	stk.Pop(1);
}

void Opcode_LSHR(Stack &stk)
{
	stk.SetLong(1, stk.GetLong(1) >> (stk.GetInt(0) & 63u));
	stk.Pop(1);
}

void Opcode_LAND(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) & stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LOR(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) | stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LXOR(Stack &stk)
{
	stk.SetLong(Stack::LONG_WORDS, stk.GetLong(Stack::LONG_WORDS) ^ stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
}

void Opcode_LSHL(Stack &stk)
{
	stk.SetLong(1, stk.GetLong(1) << (stk.GetInt(0) & 63u));
	stk.Pop(1);
}

void Opcode_LCMPEQ(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) == stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LCMPNE(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) != stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_ULCMPLT(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) < stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LCMPLT(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetSignedLong(Stack::LONG_WORDS) < stk.GetSignedLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_ULCMPLE(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) <= stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LCMPLE(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetSignedLong(Stack::LONG_WORDS) <= stk.GetSignedLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_ULCMPGT(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) > stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LCMPGT(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetSignedLong(Stack::LONG_WORDS) > stk.GetSignedLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_ULCMPGE(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetLong(Stack::LONG_WORDS) >= stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LCMPGE(Stack &stk)
{
	stk.SetInt(Stack::LONG_WORDS*2-1, stk.GetSignedLong(Stack::LONG_WORDS) >= stk.GetSignedLong(0));
	stk.Pop(Stack::LONG_WORDS*2-1);
}

void Opcode_LNEG(Stack &stk)
{
	stk.SetLong(0, -stk.GetSignedLong(0));
}

void Opcode_LNOT(Stack &stk)
{
	stk.SetLong(0, ~stk.GetLong(0));
}

void Opcode_LCMPZ(Stack &stk)
{
	auto val = stk.GetLong(0) == 0;
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt(val);
}

void Opcode_LCMPNZ(Stack &stk)
{
	auto val = stk.GetLong(0) != 0;
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt(val);
}

void Opcode_CONV_LTOI(Stack &stk)
{
	auto val = stk.GetLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt((UInt)val);
}

void Opcode_CONV_ITOL(Stack &stk)
{
	auto val = stk.GetSignedInt(0);
	stk.Pop(1);
	stk.PushLong((Long)val);
}

void Opcode_CONV_UITOL(Stack &stk)
{
	auto val = stk.GetInt(0);
	stk.Pop(1);
	stk.PushLong(val);
}

void Opcode_CONV_LTOF(Stack &stk)
{
	auto val = stk.GetSignedLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushFloat((Float)val);
}

void Opcode_CONV_LTOD(Stack &stk)
{
	auto val = stk.GetSignedLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushDouble((Double)val);
}

void Opcode_CONV_ULTOF(Stack &stk)
{
	auto val = stk.GetLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushFloat((Float)val);
}

void Opcode_CONV_ULTOD(Stack &stk)
{
	auto val = stk.GetLong(0);
	stk.Pop(Stack::LONG_WORDS);
	stk.PushDouble((Double)val);
}

void Opcode_CONV_FTOL(Stack &stk)
{
	auto val = stk.GetFloat(0);
	stk.Pop(1);
	stk.PushLong((Long)val);
}

void Opcode_CONV_FTOUL(Stack &stk)
{
	auto val = stk.GetFloat(0);
	stk.Pop(1);
	stk.PushLong(WellDefinedFloatToUnsigned<ULong>(val));
}

void Opcode_CONV_DTOL(Stack &stk)
{
	auto val = stk.GetDouble(0);
	stk.Pop(Stack::DOUBLE_WORDS);
	stk.PushLong((Long)val);
}

void Opcode_CONV_DTOUL(Stack &stk)
{
	auto val = stk.GetDouble(0);
	stk.Pop(Stack::DOUBLE_WORDS);
	stk.PushLong(WellDefinedFloatToUnsigned<ULong>(val));
}

void Opcode_CONV_STR_TO_AREF(Stack &stk)
{
	auto str = stk.GetString(0);
	stk.DelString(0);
	stk.Pop(Stack::STRING_WORDS);

	// note: not thread safe!
	if (str.IsUnique())
		str.Clear();

	// now create array ref
	stk.PushInt(str.GetLength());
	stk.PushPtr(str.Ansi());
}

void Opcode_CONV_AREF_TO_STR(Stack &stk)
{
	auto *ptr = static_cast<const char *>(stk.GetPtr(0));
	auto len = stk.GetSignedInt(1);
	stk.Pop(2);
	stk.PushString(String(ptr, len));
}

void Opcode_BSF(Stack &stk)
{
	stk.SetInt(0, Bits::GetLsb(stk.GetInt(0)));
}

void Opcode_BSR(Stack &stk)
{
	stk.SetInt(0, Bits::GetMsb(stk.GetInt(0)));
}

void Opcode_POPCNT(Stack &stk)
{
	stk.SetInt(0, Bits::PopCount(stk.GetInt(0)));
}

void Opcode_BSWAP(Stack &stk)
{
	auto v = stk.GetInt(0);
	Endian::ByteSwap(v);
	stk.SetInt(0, v);
}

void Opcode_BSFL(Stack &stk)
{
	auto res = Bits::GetLsb(stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt(res);
}

void Opcode_BSRL(Stack &stk)
{
	auto res = Bits::GetMsb(stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt(res);
}

void Opcode_POPCNTL(Stack &stk)
{
	auto res = Bits::PopCount(stk.GetLong(0));
	stk.Pop(Stack::LONG_WORDS);
	stk.PushInt(res);
}

void Opcode_BSWAPL(Stack &stk)
{
	auto v = stk.GetLong(0);
	Endian::ByteSwap(v);
	stk.SetLong(0, v);
}

void Opcode_SetStateLabel(Stack &stk)
{
	auto labelName = stk.GetString(0);
	Name clsname;
	clsname.SetValue(stk.GetLong(Stack::STRING_WORDS));

	Opcode_LDelStr0(stk);
	stk.Pop(Stack::STRING_WORDS + Stack::NAME_WORDS);

	auto &ctx = stk.GetContext();

	Int idx = ctx.GetEngine().FindFunctionOffset(labelName);
	// now encode as script delegate
	auto *fptr = ctx.GetEngine().MethodIndexToPointer(idx);
	ScriptDelegate sd;
	sd.instancePtr = stk.GetThis();
	sd.funcPtr = const_cast<void *>(fptr);

	auto *statedg = ctx.GetStateDelegateRef();

	if (statedg)
		*statedg = sd;
	else
	{
		// the DUMBEST part comes now, we need to find variable to store to
		const auto *dt = stk.GetProgram().FindClass(clsname);
		const auto *member = dt->FindMember("current_state_delegate");

		if (member)
			*(ScriptDelegate *)(static_cast<Byte *>(sd.instancePtr) + member->offset) = sd;
	}

/*	// TODO: check stack level (this will be tricky because there can be random garbage at the start)
	auto *stktop = stk.GetTop();
	stktop += 1 + !stk.GetProgram().GetJitFriendly();
	auto *stkbot = stk.GetBottom();*/
}

void Opcode_IncObjCounter(Stack &stk)
{
	Atomic::Increment(stk.GetConstantPool().liveScriptObjects);
}

void Opcode_DecObjCounter(Stack &stk)
{
	Atomic::Decrement(stk.GetConstantPool().liveScriptObjects);
}

struct ArrayRef_Placeholder
{
	UIntPtr ptr;
	UInt size;
};

void Opcode_SliceFwd_Inplace(Stack &stk)
{
	ArgParser ap(stk);
	auto elemSize = ap.Get<UInt>();
	auto count = ap.Get<UInt>();
	auto *aref = ap.Get<ArrayRef_Placeholder *>();

	aref->ptr += (UIntPtr)count * elemSize;

	auto osize = aref->size;
	auto nsize = osize - count;
	nsize *= nsize <= osize;

	aref->size = nsize;
	stk.Pop(2);
}

void Opcode_SliceFwd(Stack &stk)
{
	ArgParser ap(stk);
	auto elemSize = ap.Get<UInt>();
	auto count = ap.Get<UInt>();
	auto &aref = ap.Get<ArrayRef_Placeholder>();
	aref.ptr += (UIntPtr)count * elemSize;

	auto osize = aref.size;
	auto nsize = osize - count;
	nsize *= nsize <= osize;

	aref.size = nsize;
	stk.Pop(2);
}

void Opcode_MarkStructDg(Stack &stk)
{
	auto *ptr = stk.GetPtr(1);
	auto val = (UIntPtr)ptr;
	// set bit 1 to mark as a delegate pointing to a struct instead of a class
	val |= 2;
	stk.SetPtr(1, (const void *)val);
}

typedef void (*BuiltinCallback)(Stack &);

struct BuiltinTable
{
	Int idx;
	const char *name;
	BuiltinCallback func;
};

static const BuiltinTable BUILTIN_TABLE[] =
{
	{ BUILTIN_FMOD,				"*FMOD",				Opcode_FMod				},
	{ BUILTIN_DMOD,				"*DMOD",				Opcode_DMod				},
	{ BUILTIN_LDELSTR,			"*LDELSTR",				Opcode_LDelStr			},
	{ BUILTIN_LDELSTR0,			"*LDELSTR0",			Opcode_LDelStr0			},
	{ BUILTIN_LDELSTR1,			"*LDELSTR1",			Opcode_LDelStr1			},
	{ BUILTIN_GDELSTR,			"*GDELSTR",				Opcode_GDelStr			},
	{ BUILTIN_PDELSTR_NP,		"*PDELSTR_NP",			Opcode_PDelStr_NP		},
	{ BUILTIN_VDELSTR,			"*VDELSTR",				Opcode_VDelStr			},
	{ BUILTIN_PCOPYSTR,			"*PCOPYSTR",			Opcode_PCopyStr			},
	{ BUILTIN_VCOPYSTR,			"*VCOPYSTR",			Opcode_VCopyStr			},
	{ BUILTIN_LPUSHSTR_CONST,	"*LPUSHSTR_CONST",		Opcode_LPushStr_Const	},
	{ BUILTIN_APUSHSTR_CONST,	"*APUSHSTR_CONST",		Opcode_APushStr_Const	},
	{ BUILTIN_LPUSHSTR,			"*LPUSHSTR",			Opcode_LPushStr			},
	{ BUILTIN_GSTRLOAD,			"*GSTRLOAD",			Opcode_GStrLoad			},
	{ BUILTIN_GSTRSTORE_NP,		"*GSTRSTORE_NP",		Opcode_GStrStore_NP		},
	{ BUILTIN_GSTRSTORE,		"*GSTRSTORE",			Opcode_GStrStore		},
	{ BUILTIN_LSTRSTORE_NP,		"*LSTRSTORE_NP",		Opcode_LStrStore_NP		},
	{ BUILTIN_LSTRSTORE,		"*LSTRSTORE",			Opcode_LStrStore		},
	{ BUILTIN_LSTRADD,			"*LSTRADD",				Opcode_LStrAdd			},
	{ BUILTIN_SCMPEQ,			"*SCMPEQ",				Opcode_SCmpEq			},
	{ BUILTIN_SCMPNE,			"*SCMPNE",				Opcode_SCmpNe			},
	{ BUILTIN_SCMPLT,			"*SCMPLT",				Opcode_SCmpLt			},
	{ BUILTIN_SCMPLE,			"*SCMPLE",				Opcode_SCmpLe			},
	{ BUILTIN_SCMPGT,			"*SCMPGT",				Opcode_SCmpGt			},
	{ BUILTIN_SCMPGE,			"*SCMPGE",				Opcode_SCmpGe			},
	{ BUILTIN_CONV_CTOS,		"*CONV_CTOS",			Opcode_Conv_CToS		},
	{ BUILTIN_CONV_ITOS,		"*CONV_ITOS",			Opcode_Conv_IToS		},
	{ BUILTIN_CONV_UITOS,		"*CONV_UITOS",			Opcode_Conv_UIToS		},
	{ BUILTIN_CONV_LTOS,		"*CONV_LTOS",			Opcode_Conv_LToS		},
	{ BUILTIN_CONV_ULTOS,		"*CONV_ULTOS",			Opcode_Conv_ULToS		},
	{ BUILTIN_CONV_FTOS,		"*CONV_FTOS",			Opcode_Conv_FToS		},
	{ BUILTIN_CONV_DTOS,		"*CONV_DTOS",			Opcode_Conv_DToS		},
	{ BUILTIN_CONV_NTOS,		"*CONV_NTOS",			Opcode_Conv_NToS		},
	{ BUILTIN_CONV_STON,		"*CONV_STON",			Opcode_Conv_SToN		},
	{ BUILTIN_CONV_STOBOOL,		"*CONV_STOBOOL",		Opcode_Conv_SToBool		},
	{ BUILTIN_PLOADSTR,			"*PLOADSTR",			Opcode_PLoadStr			},
	{ BUILTIN_PSTRSTORE_IMM_NP,	"*PSTRSTORE_IMM_NP",	Opcode_PStrStore_Imm_NP	},
	{ BUILTIN_PSTRSTORE_IMM,	"*PSTRSTORE_IMM",		Opcode_PStrStore_Imm	},
	{ BUILTIN_LPUSHNAME_CONST,	"*LPUSHNAME_CONST",		Opcode_LPushName_Const	},
	{ BUILTIN_GETSTRCHAR,		"*GETSTRCHAR",			Opcode_GetStrChar		},
	{ BUILTIN_NEW,				"*NEW",					Opcode_New				},
	{ BUILTIN_NEW_DYNAMIC,		"*NEW_DYNAMIC",			Builtin::Opcode_New_Dynamic},
	{ BUILTIN_DEC_WEAK,			"*DEC_WEAK",			Opcode_DecWeak			},
	{ BUILTIN_TEST_WEAK_NULL,	"*TEST_WEAK_NULL",		Opcode_TestWeakNull		},
	{ BUILTIN_DEC_STRONG,		"*DEC_STRONG",			Opcode_DecStrong		},
	{ BUILTIN_STRONG_ZERO,		"*STRONG_ZERO",			Opcode_StrongZero		},
	{ BUILTIN_ADD_WEAK,			"*ADD_WEAK",			Opcode_AddWeak			},
	{ BUILTIN_ADD_WEAK_NULL,	"*ADD_WEAK_NULL",		Opcode_AddWeakNull		},
	{ BUILTIN_ADD_STRONG,		"*ADD_STRONG",			Builtin::Opcode_AddStrong},
	{ BUILTIN_ADD_STRONG_AFTER_NEW, "*ADD_STRONG_AFTER_NEW", Builtin::Opcode_AddStrongAfterNew},
	{ BUILTIN_ISA,				"*CONV_ISA",			Opcode_IsA				},
	{ BUILTIN_ISA_NOPOP,		"*ISA_NOPOP",			Opcode_IsANoPop			},
	{ BUILTIN_FIX_WEAK,			"*FIX_WEAK",			Opcode_FixWeak			},
	{ BUILTIN_FIX_WEAK_REF,		"*FIX_WEAK_REF",		Opcode_FixWeakRef		},
	{ BUILTIN_FIX_STRONG,		"*FIX_STRONG",			Opcode_FixStrong		},
	{ BUILTIN_FIX_ADD_WEAK,		"*FIX_ADD_WEAK",		Opcode_FixAddWeak		},
	{ BUILTIN_FIX_ADD_STRONG,	"*FIX_ADD_STRONG",		Opcode_FixAddStrong		},
	{ BUILTIN_PSTRADD_ASSIGN,	"*PSTRADD_ASSIGN",		Opcode_PStrAdd_Assign	},
	{ BUILTIN_PSTRADD_ASSIGN_LOAD, "*PSTRADD_ASSIGN_LOAD", Opcode_PStrAdd_Assign_Load },
	{ BUILTIN_CMPDG_EQ,			"*CMPDG_EQ",			Opcode_CmpDG_Eq			},
	{ BUILTIN_CMPDG_NE,			"*CMPDG_NE",			Opcode_CmpDG_Ne			},
	{ BUILTIN_NATIVE_CTOR,		"*NATIVE_CTOR",			Opcode_Native_Ctor		},
	{ BUILTIN_NATIVE_DTOR,		"*NATIVE_DTOR",			Opcode_Native_Dtor		},

	{ BUILTIN_PROF_ENTER,		"*PROF_ENTER",			Builtin::OpCode_ProfEnter},
	{ BUILTIN_PROF_EXIT,		"*PROF_EXIT",			Builtin::OpCode_ProfExit},

	{ BUILTIN_PUSH_LCONST,      "*PUSH_LCONST",         Opcode_PUSH_LCONST      },
	{ BUILTIN_PUSHC_LCONST,     "*PUSHC_LCONST",        Opcode_PUSHC_LCONST     },
	{ BUILTIN_PLOAD64,          "*PLOAD64",             Opcode_PLOAD64          },
	{ BUILTIN_GLOAD64,          "*GLOAD64",             Opcode_GLOAD64          },
	{ BUILTIN_LPUSH64,          "*LPUSH64",             Opcode_LPUSH64          },
	{ BUILTIN_LSTORE64,         "*LSTORE64",            Opcode_LSTORE64         },
	{ BUILTIN_LSTORE64_NP,      "*LSTORE64_NP",         Opcode_LSTORE64_NP      },
	{ BUILTIN_GSTORE64,         "*GSTORE64",            Opcode_GSTORE64         },
	{ BUILTIN_GSTORE64_NP,      "*GSTORE64_NP",         Opcode_GSTORE64_NP      },
	{ BUILTIN_PSTORE64_IMM0,    "*PSTORE64_IMM0",       Opcode_PSTORE64_IMM0    },
	{ BUILTIN_PSTORE64_IMM0_NP, "*PSTORE64_IMM0_NP",    Opcode_PSTORE64_IMM0_NP },
	{ BUILTIN_PINC64,           "*PINC64",              Opcode_PINC64           },
	{ BUILTIN_PINC64_POST,      "*PINC64_POST",         Opcode_PINC64_POST      },

	{ BUILTIN_LADD,             "*LADD",                Opcode_LADD             },
	{ BUILTIN_LSUB,             "*LSUB",                Opcode_LSUB             },
	{ BUILTIN_LMUL,             "*LMUL",                Opcode_LMUL             },
	{ BUILTIN_LMOD,             "*LMOD",                (BuiltinCallback)(void *)Opcode_LMOD },
	{ BUILTIN_ULMOD,            "*ULMOD",               (BuiltinCallback)(void *)Opcode_ULMOD },
	{ BUILTIN_LDIV,             "*LDIV",                (BuiltinCallback)(void *)Opcode_LDIV },
	{ BUILTIN_ULDIV,            "*ULDIV",               (BuiltinCallback)(void *)Opcode_ULDIV },
	{ BUILTIN_LSAR,             "*LSAR",                Opcode_LSAR             },
	{ BUILTIN_LSHR,             "*LSHR",                Opcode_LSHR             },
	{ BUILTIN_LAND,             "*LAND",                Opcode_LAND             },
	{ BUILTIN_LOR,              "*LOR",                 Opcode_LOR              },
	{ BUILTIN_LXOR,             "*LXOR",                Opcode_LXOR             },
	{ BUILTIN_LSHL,             "*LSHL",                Opcode_LSHL             },

	{ BUILTIN_LCMPEQ,           "*LCMPEQ",              Opcode_LCMPEQ           },
	{ BUILTIN_LCMPNE,           "*LCMPNE",              Opcode_LCMPNE           },
	{ BUILTIN_LCMPLT,           "*LCMPLT",              Opcode_LCMPLT           },
	{ BUILTIN_ULCMPLT,          "*ULCMPLT",             Opcode_ULCMPLT          },
	{ BUILTIN_LCMPLE,           "*LCMPLE",              Opcode_LCMPLE           },
	{ BUILTIN_ULCMPLE,          "*ULCMPLE",             Opcode_ULCMPLE          },
	{ BUILTIN_LCMPGT,           "*LCMPGT",              Opcode_LCMPGT           },
	{ BUILTIN_ULCMPGT,          "*ULCMPGT",             Opcode_ULCMPGT          },
	{ BUILTIN_LCMPGE,           "*LCMPGE",              Opcode_LCMPGE           },
	{ BUILTIN_ULCMPGE,          "*ULCMPGE",             Opcode_ULCMPGE          },

	{ BUILTIN_LNEG,             "*LNEG",                Opcode_LNEG             },
	{ BUILTIN_LNOT,             "*LNOT",                Opcode_LNOT             },
	{ BUILTIN_LCMPZ,            "*LCMPZ",               Opcode_LCMPZ            },
	{ BUILTIN_LCMPNZ,           "*LCMPNZ",              Opcode_LCMPNZ           },

	{ BUILTIN_CONV_LTOI,        "*CONV_LTOI",           Opcode_CONV_LTOI        },
	{ BUILTIN_CONV_ITOL,        "*CONV_ITOL",           Opcode_CONV_ITOL        },
	{ BUILTIN_CONV_UITOL,       "*CONV_UITOL",          Opcode_CONV_UITOL       },
	{ BUILTIN_CONV_LTOF,        "*CONV_LTOF",           Opcode_CONV_LTOF        },
	{ BUILTIN_CONV_LTOD,        "*CONV_LTOD",           Opcode_CONV_LTOD        },
	{ BUILTIN_CONV_ULTOF,       "*CONV_ULTOF",          Opcode_CONV_ULTOF       },
	{ BUILTIN_CONV_ULTOD,       "*CONV_ULTOD",          Opcode_CONV_ULTOD       },

	{ BUILTIN_CONV_FTOL,        "*CONV_FTOL",           Opcode_CONV_FTOL        },
	{ BUILTIN_CONV_FTOUL,       "*CONV_FTOUL",          Opcode_CONV_FTOUL       },
	{ BUILTIN_CONV_DTOL,        "*CONV_DTOL",           Opcode_CONV_DTOL        },
	{ BUILTIN_CONV_DTOUL,       "*CONV_DTOUL",          Opcode_CONV_DTOUL       },

	{ BUILTIN_CONV_STR_TO_AREF, "*CONV_STR_TO_AREF",    Opcode_CONV_STR_TO_AREF },
	{ BUILTIN_CONV_AREF_TO_STR, "*CONV_AREF_TO_STR",    Opcode_CONV_AREF_TO_STR },

	{ BUILTIN_INTRIN_BSF,       "*BSF",                 Opcode_BSF              },
	{ BUILTIN_INTRIN_BSR,       "*BSR",                 Opcode_BSR              },
	{ BUILTIN_INTRIN_POPCNT,    "*POPCNT",              Opcode_POPCNT           },
	{ BUILTIN_INTRIN_BSWAP,     "*BSWAP",               Opcode_BSWAP            },
	{ BUILTIN_INTRIN_BSFL,      "*BSFL",                Opcode_BSFL             },
	{ BUILTIN_INTRIN_BSRL,      "*BSRL",                Opcode_BSRL             },
	{ BUILTIN_INTRIN_POPCNTL,   "*POPCNTL",             Opcode_POPCNTL          },
	{ BUILTIN_INTRIN_BSWAPL,    "*BSWAPL",              Opcode_BSWAPL           },

	{ BUILTIN_SET_STATE_LABEL,  "*SETSTATELABEL",       Opcode_SetStateLabel    },

	{ BUILTIN_INC_OBJECT_COUNTER, "*INCOBJCOUNTER",     Opcode_IncObjCounter    },
	{ BUILTIN_DEC_OBJECT_COUNTER, "*DECOBJCOUNTER",     Opcode_DecObjCounter    },

	{ BUILTIN_SLICEFWD_INPLACE,  "*SLICEFWD_INPLACE",   Opcode_SliceFwd_Inplace },
	{ BUILTIN_SLICEFWD,          "*SLICEFWD",           Opcode_SliceFwd         },

	{ BUILTIN_MARK_STRUCT_DELEGATE, "*MARK_STR_DG",     Opcode_MarkStructDg     },

	{ -1, 0, 0 }
};

bool RegisterBuiltins(ConstPool &cpool)
{
	const BuiltinTable *t = BUILTIN_TABLE;

	while (t->func)
	{
		LETHE_RET_FALSE(cpool.BindNativeFunc(t->name, t->func) == t->idx);
		t++;
	}

	return 1;
}

}
