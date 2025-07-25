#include "DataTypes.h"
#include <Lethe/Script/Ast/AstNode.h>
#include <Lethe/Script/Ast/Function/AstFunc.h>
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Builtin.h>
#include <Lethe/Script/Vm/Vm.h>
#include "BaseObject.h"

#include <Lethe/Core/Math/Templates.h>
#include <Lethe/Core/String/StringBuilder.h>

#if LETHE_OS_WINDOWS
#	include <windows.h>
#endif

namespace lethe
{

LETHE_BUCKET_ALLOC_DEF(DataType)

// DataType

DataType::DataType()
	: type(DT_NONE)
	, align(0)
	, size(0)
	, vtblOffset(-1)
	, vtblSize(0)
	, arrayDims(0)
	, typeIndex(-1)
	, currentStateDelegateOffset(-1)
	, structQualifiers(0)
	, complementaryType(nullptr)
	, complementaryType2(nullptr)
	, funcRef(nullptr)
	, ctorRef(nullptr)
	, structScopeRef(nullptr)
	, classNameGroupKey(0)
	, funCtor(-1)
	, funVCtor(-1)
	, funAssign(-1)
	, funVAssign(-1)
	, funDtor(-1)
	, funVDtor(-1)
	, funCmp(-1)
	, nativeCtor(nullptr)
	, nativeDtor(nullptr)
{
	location.column = location.line = 0;
}

const DataType::Member *DataType::FindMember(const StringRef &nname) const
{
	for (Int i=0; i<members.GetSize(); i++)
	{
		const Member &m = members[i];

		if (m.name == nname)
			return &m;
	}

	return baseType.ref && baseType.GetTypeEnum() != DT_NONE ? baseType.ref->FindMember(nname) : 0;
}

bool DataType::MatchType(const DataType &o, bool narrowStatic) const
{
	if (this == &o)
		return 1;

	if (type == DT_ARRAY_REF || type == DT_DYNAMIC_ARRAY)
	{
		LETHE_RET_FALSE(o.type == DT_ARRAY_REF);
		return elemType.GetType() == o.elemType.GetType();
	}

	if (type == DT_STATIC_ARRAY)
	{
		return type == o.type && (narrowStatic ? arrayDims <= o.arrayDims : arrayDims == o.arrayDims) &&
			   elemType.GetType() == o.elemType.GetType();
	}

	// allow to pass static array of elem to ref of elem
	if (narrowStatic && o.type == DT_STATIC_ARRAY)
		return *this == o.elemType.GetType();

	// FIXME: ...
	if (type == DT_STRUCT || type == DT_CLASS)
	{
		LETHE_RET_FALSE(type == o.type);

		// struct/class types must match exactly
		// OR, o must be derived from this
		if (this == &o)
			return 1;

		QDataType tmp = o.baseType;

		while (tmp.GetType().type == type)
		{
			if (this == &tmp.GetType())
				return 1;

			tmp = tmp.GetType().baseType;
		}

		return 0;
	}

	if (type == o.type)
	{
		if (type == DT_STRONG_PTR || type == DT_WEAK_PTR || type == DT_RAW_PTR)
			return elemType.GetType() == o.elemType.GetType();
	}

	return type == o.type;
}

bool DataType::operator ==(const DataType &o) const
{
	return MatchType(o);
}

bool DataType::IsSmallNumber() const
{
	return type >= DT_BOOL && type <= DT_ENUM && size < 4;
}

bool DataType::IsInteger() const
{
	return type >= DT_BOOL && type <= DT_ULONG;
}

bool DataType::IsNumberEnum(DataTypeEnum t)
{
	return t >= DT_BYTE && t <= DT_DOUBLE;
}

DataTypeEnum DataType::EvalTypeEnum(DataTypeEnum t)
{
	return (t >= DT_BOOL && t < DT_INT) ? DT_INT : t;
}

DataTypeEnum DataType::ComposeTypeEnum(DataTypeEnum t0, DataTypeEnum t1)
{
	t0 = EvalTypeEnum(t0);
	t1 = EvalTypeEnum(t1);

	if (t0 == t1)
		return t0;

	if (t0 == DT_NULL)
		return t1;

	if (t1 == DT_NULL)
		return t0;

	if (t0 > t1)
		Swap(t0, t1);

	if (IsNumberEnum(t0) && IsNumberEnum(t1))
	{
		// simply use max type
		return Max(t0, t1);
	}

	// note: same types already handled
	switch(t1)
	{
	case DT_STRING:
		return DT_STRING;

	case DT_STRONG_PTR:
		return (t0 == DT_WEAK_PTR || t0 == DT_RAW_PTR) ? DT_STRONG_PTR : DT_NONE;

	// all other cannot be composed
	default:
		;
	}

	// allow string + presumably string_view => string
	if (t0 == DT_STRING && t1 == DT_ARRAY_REF)
		return t0;

	// invalid if we get here
	return DT_NONE;
}

String DataType::GetName() const
{
	String prefix;
	const char *suffix;

	if (IsInteger() && baseType.GetTypeEnum() == DT_ENUM)
	{
		StringBuilder sb;
		DataType tmp;
		tmp.type = type;
		sb += tmp.GetName();
		sb += " in ";
		sb += baseType.GetName();
		return sb.Get();
	}

	switch(type)
	{
	case DT_NONE:
		return "void";

	case DT_BOOL:
		return "bool";

	case DT_BYTE:
		return "byte";

	case DT_SBYTE:
		return "sbyte";

	case DT_SHORT:
		return "short";

	case DT_USHORT:
		return "ushort";

	case DT_CHAR:
		return "char";

	case DT_INT:
		return "int";

	case DT_UINT:
		return "uint";

	case DT_LONG:
		return "long";

	case DT_ULONG:
		return "ulong";

	case DT_FLOAT:
		return "float";

	case DT_DOUBLE:
		return "double";

	case DT_NAME:
		return "name";

	case DT_FUNC_PTR:
		return name.IsEmpty() ? "function ptr" : name;

	case DT_DELEGATE:
		return name.IsEmpty() ? "delegate" : name;

	case DT_STRING:
		return "string";

	case DT_NULL:
		return "null";

	case DT_RAW_PTR:
		return "raw " + elemType.GetName();

	case DT_WEAK_PTR:
		return "weak " + elemType.GetName();

	case DT_STRONG_PTR:
		return elemType.GetName();

	case DT_ENUM:
	case DT_STRUCT:
	case DT_CLASS:
		return name;

	case DT_ARRAY_REF:
	case DT_STATIC_ARRAY:
	case DT_DYNAMIC_ARRAY:
		if (type == DT_ARRAY_REF)
		{
			prefix = "[](";
			suffix = ")";
		}
		else if (type == DT_STATIC_ARRAY)
		{
			prefix.Format("[%d](", arrayDims);
			suffix = ")";
		}
		else
		{
			prefix = "array<";
			suffix = ">";
		}

		return prefix + elemType.GetName() + suffix;

	default:
		;
	}

	return String::Printf(LETHE_FORMAT_UINTPTR_HEX, (UIntPtr)(const void *)this);
}

String DataType::GetSimpleTypeName(DataTypeEnum dte)
{
	switch(dte)
	{
	case DT_NONE:
		return "void";

	case DT_BOOL:
		return "bool";

	case DT_BYTE:
		return "byte";

	case DT_SBYTE:
		return "sbyte";

	case DT_SHORT:
		return "short";

	case DT_USHORT:
		return "ushort";

	case DT_CHAR:
		return "char";

	case DT_INT:
		return "int";

	case DT_UINT:
		return "uint";

	case DT_LONG:
		return "long";

	case DT_ULONG:
		return "ulong";

	case DT_FLOAT:
		return "float";

	case DT_DOUBLE:
		return "double";

	case DT_NAME:
		return "name";

	case DT_FUNC_PTR:
		return "function ptr";

	case DT_DELEGATE:
		return "delegate";

	case DT_STRING:
		return "string";

	case DT_NULL:
		return "null";

	case DT_RAW_PTR:
		return "raw ptr";

	case DT_WEAK_PTR:
		return "weak ptr";

	case DT_STRONG_PTR:
		return "strong ptr";

	case DT_ENUM:
		return "enum";

	case DT_STRUCT:
		return "struct";

	case DT_CLASS:
		return "class";

	case DT_ARRAY_REF:
		return "array ref";

	case DT_STATIC_ARRAY:
		return "static array";

	case DT_DYNAMIC_ARRAY:
		return "dynamic array";

	default:
		;
	}

	return "unknown";
}

bool DataType::ZeroInit() const
{
	if (type == DT_STATIC_ARRAY)
		return elemType.ZeroInit();

	// a struct requiring a dtor is already handled by ZeroInit in QDataType
	if (type == DT_STRUCT)
		return false;

	return type < DT_INT || type > DT_DOUBLE;
}

bool DataType::IsArray() const
{
	return type == DT_STATIC_ARRAY || type == DT_DYNAMIC_ARRAY || type == DT_ARRAY_REF;
}

bool DataType::HasArrayRef() const
{
	LETHE_RET_FALSE(IsArray());

	if (type == DT_ARRAY_REF)
		return true;

	return elemType.HasArrayRef();
}

bool DataType::HasArrayRefWithNonConstElem() const
{
	LETHE_RET_FALSE(IsArray());

	if (type == DT_ARRAY_REF && !elemType.IsConst())
		return true;

	return elemType.HasArrayRefWithNonConstElem();
}

bool DataType::IsSmartPointer() const
{
	return type == DT_STRONG_PTR || type == DT_WEAK_PTR;
}

bool DataType::IsPointer() const
{
	return type == DT_RAW_PTR || IsSmartPointer();
}

bool DataType::IsStruct() const
{
	return type == DT_STRUCT;
}

bool DataType::IsFuncPtr() const
{
	return type == DT_FUNC_PTR || type == DT_DELEGATE;
}

bool DataType::IsIndexableStruct() const
{
	// TODO: maybe no base is too strict?
	LETHE_RET_FALSE(IsStruct() && !members.IsEmpty() && baseType.GetTypeEnum() == DT_NONE);

	for (Int i=1; i<members.GetSize(); i++)
		LETHE_RET_FALSE(members[i-1].type == members[i].type);

	return true;
}

bool DataType::IsEnumFlags() const
{
	LETHE_RET_FALSE(type == DT_ENUM && attributes);

	for (auto &&it : attributes->tokens)
		if (it.type == TOK_IDENT && it.text == "flags")
			return true;

	return false;
}

void DataType::GenBaseChain() const
{
	isa.Clear();

	className = name;

	const DataType *tmp = this;

	while (tmp && tmp->type == DT_CLASS)
	{
		isa.Add(Name(tmp->name));
		tmp = tmp->baseType.ref;
	}

	isa.Sort();
}

bool DataType::IsA(Name n) const
{
	return BinarySearch(isa.Begin(), isa.End(), n);
}

bool DataType::IsBaseOf(const DataType &o) const
{
	const DataType *t = &o;

	while (t && t->type != DT_NONE)
	{
		if (this == t)
			return true;

		t = t->baseType.ref;
	}

	return false;
}

bool DataType::HasMemberVarsAfter(const DataType *clsBase) const
{
	const DataType *t = this;

	while (t && t->type != DT_NONE)
	{
		if (clsBase == t)
			return false;

		if (!t->members.IsEmpty())
			break;

		t = t->baseType.ref;
	}

	return true;
}

bool DataType::GenCtor(CompiledProgram &p) const
{
	// FIXME: this is mostly ugly copy-paste from GenDtor so there may be bugs!

	if (funCtor >= 0)
	{
		// already generated
		return true;
	}

	// base for composites, elem for static arrays
	const DataType *base = nullptr;
	const DataType *elem = nullptr;

	String typeName = name;

	LETHE_ASSERT(type != DT_DYNAMIC_ARRAY && "dynarrays not supported!");

	if (elemType.GetTypeEnum() != DT_NONE)
	{
		elem = &elemType.GetType();

		if (elem->funCtor < 0 && elemType.HasCtor())
		{
			// generate elem ctor first
			LETHE_RET_FALSE(elem->GenCtor(p));
		}
	}

	if (baseType.GetTypeEnum() != DT_NONE)
	{
		base = &baseType.GetType();

		if (base->funCtor < 0 && baseType.HasCtor())
		{
			// generate base first
			LETHE_RET_FALSE(base->GenCtor(p));
		}
	}

	// generate extra ctors now
	for (Int i = members.GetSize() - 1; i >= 0; i--)
	{
		const Member &m = members[i];

		if (!m.type.HasCtor())
			continue;

		// TODO: more!
		const DataType &dt = m.type.GetType();

		if ((dt.type == DT_STRUCT || dt.type == DT_CLASS || dt.type == DT_STATIC_ARRAY) && dt.funCtor < 0)
			LETHE_RET_FALSE(dt.GenCtor(p));
	}

	p.FlushOpt();
	p.EmitFunc(String::Printf(".ctor.%s", typeName.Ansi()), 0);

	AstFunc *cctor = AstStaticCast<AstFunc *>(ctorRef);

	funCtor = p.instructions.GetSize();

	// handle object counter if base object
	if (type == DT_CLASS && typeName == "object")
		p.EmitI24(OPC_BCALL, BUILTIN_INC_OBJECT_COUNTER);

	// stack layout:
	// [0] = ret adr, [1] = ptr

	Int firstArg = 1 - p.IsFastCall();

	// load ptr first
	p.Emit(OPC_LPUSHPTR + ((UInt)firstArg << 8));

	const auto native = (structQualifiers & AST_Q_NATIVE) != 0;

	if (base && base->funCtor >= 0 && !native)
		p.EmitBackwardJump(OPC_CALL, base->funCtor);

	if (native && nativeCtor)
	{
		// native class/struct
		// call native ctor!
		p.EmitIntConst(typeIndex);
		p.EmitI24(OPC_BCALL, BUILTIN_NATIVE_CTOR);

		if (type == DT_CLASS && base)
		{
			// call base ctor now
			auto tbase = base;

			for(;;)
			{
				LETHE_ASSERT(tbase);
				auto parent = &tbase->baseType.GetType();

				if (parent && parent->type == DT_CLASS)
					tbase = parent;
				else
					break;
			}

			// this must be object (TODO: verify?!)
			if (tbase && tbase->funCtor >= 0)
				p.EmitBackwardJump(OPC_CALL, tbase->funCtor);
		}
	}

	// set vtbl if class
	if (type == DT_CLASS)
	{
		p.EmitI24(OPC_GLOADADR, vtblOffset);
		p.Emit(OPC_LPUSHPTR + (UInt(firstArg+2) << 8));
		p.EmitI24(OPC_PSTOREPTR_IMM, BaseObject::OFS_VTBL);
	}

	Int ofs = 0;

	for (Int i = 0; i<members.GetSize(); i++)
	{
		const Member &m = members[i];

		if (!m.type.HasCtor() || (m.type.qualifiers & AST_Q_NATIVE))
			continue;

		// FIXME: better
		// TODO: more!!!
		if (m.type.GetType().funCtor >= 0)
		{
			Int delta = (Int)m.offset - ofs;
			p.Emit(OPC_PUSH_ICONST + ((UInt)delta << 8));
			p.Emit(OPC_AADD + (1 << 8));
			p.EmitBackwardJump(OPC_CALL, m.type.GetType().funCtor);
			ofs += delta;
		}
	}

	if (ofs)
	{
		p.EmitI24(OPC_POP, 1);
		p.Emit(OPC_LPUSHPTR + ((UInt)firstArg << 8));
	}

	Int pop = 1;

	if (cctor)
	{
		pop = 0;
		// call custom ctor
		p.Emit(OPC_LOADTHIS);
		Int handle = p.EmitForwardJump(OPC_CALL);
		p.Emit(OPC_POPTHIS);
		cctor->AddForwardRef(handle);
	}

	if (elem)
	{
		// static array => have to call vector ctor
		p.Emit(p.GenIntConst(arrayDims));

		if (elem->funVCtor >= 0)
		{
			p.EmitBackwardJump(OPC_CALL, elem->funVCtor);
			pop = 2;
		}
	}

	p.Emit(OPC_RET + ((UInt)pop << 8));

	// no vector ctor for classes
	if (type == DT_CLASS)
		return 1;

	p.FlushOpt();
	p.EmitFunc(String::Printf(".vctor.%s", typeName.Ansi()), 0);

	funVCtor = p.instructions.GetSize();

	// vector ctor has:
	// [0] = ret adr [1] = count, [2] = ptr

	// FIXME: should iterate FORWARD here!!!

	p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
	p.Emit(OPC_IBNZ_P + (1 << 8));
	p.Emit(OPC_RET);

	p.FlushOpt();

	p.EmitI24(OPC_PUSH_ICONST, 0);
	firstArg++;

	p.FlushOpt();

	Int vloop = p.instructions.GetSize();

	p.EmitI24(OPC_LPUSHPTR, firstArg+1);
	p.Emit(OPC_LPUSH32 + (1 << 8));
	p.EmitI24(OPC_AADD, size);
	LETHE_RET_FALSE(p.EmitBackwardJump(OPC_CALL, funCtor));
	p.Emit(OPC_POP + (1 << 8));

	// inc idx
	p.Emit(OPC_LIADD_ICONST + (0 << 8) + (0 << 16) + 0x01000000u);
	// dec count
	p.Emit(OPC_LIADD_ICONST + ((UInt)firstArg << 8) + ((UInt)firstArg << 16) + 0xff000000u);
	p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
	LETHE_RET_FALSE(p.EmitBackwardJump(OPC_IBNZ_P, vloop));

	p.EmitI24(OPC_RET, 1);

	return 1;
}

bool DataType::GenDtor(CompiledProgram &p) const
{
	if (funDtor >= 0 || type == DT_STRING || type == DT_RAW_PTR)
	{
		// already generated/skip
		return true;
	}

	// TODO: more types...

	// base for composites, elem for static arrays
	const DataType *base = nullptr;
	const DataType *elem = nullptr;

	String typeName = name;

	bool skipDtor = false;

	if (IsPointer())
	{
		elem = &elemType.GetType();

		if (type == DT_WEAK_PTR)
			skipDtor = p.weakDtor >= 0;
		else if (type == DT_STRONG_PTR)
			skipDtor = p.strongDtor >= 0;
	}
	else if (type != DT_STRING && elemType.GetTypeEnum() != DT_NONE)
	{
		elem = &elemType.GetType();

		if (elem->funDtor < 0 && elem->type != DT_STRING && elemType.HasDtor())
		{
			// generate elem dtor first
			LETHE_RET_FALSE(elem->GenDtor(p));
		}
	}

	if (baseType.GetTypeEnum() != DT_NONE)
	{
		base = &baseType.GetType();

		if (base->funDtor < 0)
		{
			// generate base first
			LETHE_RET_FALSE(base->GenDtor(p));
		}
	}

	LETHE_ASSERT(!typeName.IsEmpty());

	// generate extra dtors now
	for (Int i=members.GetSize()-1; i>=0; i--)
	{
		const Member &m = members[i];

		if (!m.type.HasDtor())
			continue;

		// TODO: more!
		const DataType &dt = m.type.GetType();

		if ((dt.type == DT_STRUCT || dt.type == DT_CLASS || dt.type == DT_STATIC_ARRAY || dt.type == DT_DYNAMIC_ARRAY || dt.IsPointer()) && dt.funDtor < 0)
			LETHE_RET_FALSE(dt.GenDtor(p));

	}

	const Int firstArg = 1 - p.IsFastCall();

	if (skipDtor)
	{
		funDtor = type == DT_WEAK_PTR ? p.weakDtor : p.strongDtor;
		funVDtor = type == DT_WEAK_PTR ? p.weakVDtor : p.strongVDtor;
	}
	else
	{
		p.FlushOpt();
		p.EmitFunc(String::Printf(".dtor.%s", typeName.Ansi()), 0);

		// for composite types, dtor expects pointer on top of stack

		// TODO: optimize: if derived members have nothing that needs dtors, just map to base type's dtors

		// now, must chain dtors; dtors occur in reverse order so...
		AstFunc *cdtor = AstStaticCast<AstFunc *>(funcRef);

		funDtor = p.instructions.GetSize();

		// stack layout:
		// [0] = ret adr, [1] = ptr

		// set vtbl if class
		if (type == DT_CLASS)
		{
			p.EmitI24(OPC_GLOADADR, vtblOffset);
			p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
			p.EmitI24(OPC_PSTOREPTR_IMM, BaseObject::OFS_VTBL);
		}

		if (cdtor)
		{
			// call custom dtor first
			p.Emit(OPC_LPUSHPTR + ((UInt)firstArg << 8));
			p.Emit(OPC_LOADTHIS);
			Int handle = p.EmitForwardJump(OPC_CALL);
			p.Emit(OPC_POPTHIS);
			cdtor->AddForwardRef(handle);
		}

		// load ptr first
		p.Emit(OPC_LPUSHPTR + ((UInt)firstArg << 8));

		Int ofs = 0;

		for (Int i=members.GetSize()-1; i>=0; i--)
		{
			const Member &m = members[i];

			if (!m.type.HasDtor())
				continue;

			// FIXME: better
			// TODO: more!!!
			if (m.type.GetTypeEnum() == DT_STRING)
			{
				Int delta = (Int)m.offset - ofs;
				p.Emit(OPC_PUSH_ICONST + ((UInt)delta << 8));
				p.Emit(OPC_AADD + (1 << 8));
				p.Emit(OPC_BCALL + (BUILTIN_PDELSTR_NP << 8));
				ofs += delta;
			}
			else
			{
				if (m.type.GetType().funDtor >= 0)
				{
					Int delta = (Int)m.offset - ofs;
					p.Emit(OPC_PUSH_ICONST + ((UInt)delta << 8));
					p.Emit(OPC_AADD + (1 << 8));
					p.EmitBackwardJump(OPC_CALL, m.type.GetType().funDtor);
					ofs += delta;
				}
			}
		}

		Int pop = 1;

		if (type == DT_STRING)
		{
			p.EmitI24(OPC_BCALL, BUILTIN_PDELSTR_NP);
		}
		else if (IsPointer())
		{
			// preserve pointer
			p.Emit(OPC_LPUSHPTR);
			p.Emit(OPC_PLOADPTR_IMM);

			if (type == DT_WEAK_PTR)
				p.EmitI24(OPC_BCALL, BUILTIN_DEC_WEAK);
			else
			{
				LETHE_ASSERT(type != DT_RAW_PTR);
				// strong pointers: dec_ref
				// if zero => call dtor, free or shrink block afterwards
				p.Emit(OPC_BCALL + (BUILTIN_DEC_STRONG << 8));
				auto fwd = p.EmitForwardJump(OPC_IBNZ_P);

				p.Emit(OPC_PUSHTHIS);
				// fetch dtor ptr
				p.EmitI24(OPC_LOADTHIS_IMM, 1);
				p.Emit(OPC_PUSHTHIS);
				p.EmitI24(OPC_VCALL, 0);
				p.Emit(OPC_POPTHIS);
				p.Emit(OPC_POPTHIS);

				p.Emit(OPC_BCALL + (BUILTIN_STRONG_ZERO << 8));
				p.FixupForwardTarget(fwd);
			}

			p.Emit(OPC_LSWAPPTR);
			p.Emit(OPC_PSTOREPTR_IMM);
			pop = 0;
		}
		else if (elem)
		{
			if (type == DT_DYNAMIC_ARRAY)
			{
				// prepare vcall
				p.Emit(OPC_LOADTHIS);
				p.EmitIntConst(elem->typeIndex);

				Int fidx = p.cpool.FindNativeFunc("__da_reset");
				LETHE_ASSERT(fidx >= 0);
				p.EmitI24(OPC_NMCALL, fidx);

				p.EmitI24(OPC_POP, 1);
				p.Emit(OPC_POPTHIS);
				pop = 0;
			}
			else
			{
				// static array => have to call vector dtor
				p.EmitIntConst(arrayDims);

				if (elemType.GetTypeEnum() == DT_STRING)
				{
					// VDELSTR does clean up
					p.Emit(OPC_BCALL + (BUILTIN_VDELSTR << 8));
					pop = 0;
				}
				else if (elem->funVDtor >= 0)
				{
					p.EmitBackwardJump(OPC_CALL, elem->funVDtor);
					pop = 2;
				}
			}
		}

		const auto native = (structQualifiers & AST_Q_NATIVE) != 0;

		if (native && nativeDtor)
		{
			// native class/struct
			// call native dtor!
			p.EmitIntConst(typeIndex);
			p.EmitI24(OPC_BCALL, BUILTIN_NATIVE_DTOR);

			if (type == DT_CLASS && base)
			{
				// call base dtor now
				auto tbase = base;

				for (;;)
				{
					auto parent = &tbase->baseType.GetType();

					if (parent && parent->type == DT_CLASS)
						tbase = parent;
					else
						break;
				}

				// this must be object (TODO: verify?!)
				if (tbase && tbase->funDtor >= 0)
					p.EmitBackwardJump(OPC_CALL, tbase->funDtor);
			}
		}

		// handle object counter if base object
		if (type == DT_CLASS && typeName == "object")
			p.EmitI24(OPC_BCALL, BUILTIN_DEC_OBJECT_COUNTER);

		if (base && !native)
		{
			p.Emit(OPC_POP + (1 << 8));

			p.EmitBackwardJump(OPC_BR, base->funDtor);
		}
		else
			p.Emit(OPC_RET + ((UInt)pop << 8));

		// no vector dtor/copying for classes (really? but copying might be useful for cloning!)
		if (type == DT_CLASS)
			return true;

		p.FlushOpt();
		p.EmitFunc(String::Printf(".vdtor.%s", typeName.Ansi()), 0);

		funVDtor = p.instructions.GetSize();

		// vector deleter has:
		// [0] = ret adr [1] = count, [2] = ptr

		p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
		p.Emit(OPC_IBNZ_P + (1 << 8));
		p.Emit(OPC_RET);

		p.FlushOpt();

		Int vloop = p.instructions.GetSize();

		p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
		p.Emit(OPC_LPUSH32 + (UInt(firstArg+1) << 8));
		p.Emit(p.GenIntConst(-1));
		p.Emit(OPC_IADD);
		p.Emit(OPC_AADD + ((UInt)size << 8));
		LETHE_RET_FALSE(p.EmitBackwardJump(OPC_CALL, funDtor));
		p.Emit(OPC_POP + (1 << 8));

		// dec count
		p.Emit(OPC_LIADD_ICONST + ((UInt)firstArg << 8) + ((UInt)firstArg << 16) + 0xff000000u);
		p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
		LETHE_RET_FALSE(p.EmitBackwardJump(OPC_IBNZ_P, vloop));

		p.Emit(OPC_RET);

		if (IsPointer())
		{
			// cache dtors
			if (type == DT_WEAK_PTR)
			{
				p.weakDtor = funDtor;
				p.weakVDtor = funVDtor;
			}
			else if (type == DT_STRONG_PTR)
			{
				p.strongDtor = funDtor;
				p.strongVDtor = funVDtor;
			}
		}
	}

	// TODO: gen assign/vector assign operators (really need vector assign?! => yes, to copy [dynamic] arrays)
	// problem with copying (assignment): need two pointers (performance?)
	// assign(dst, src) => dst pushed first
	// [0] = ret adr, [1] = dst_ptr, [2] = src_ptr

	// note: for pointers, src is value, not ptr!

	p.FlushOpt();
	p.EmitFunc(String::Printf(".copy.%s", typeName.Ansi()), 0);

	// this check allows for custom __copy function
	if (funAssign < 0)
		funAssign = p.instructions.GetSize();

	if (base && base->funAssign >= 0)
	{
		// chain-call base assign; we have to push again before calling (unless fastCall [JIT] is on)
		p.EmitU24(OPC_LPUSHPTR, firstArg+1);
		p.EmitU24(OPC_LPUSHPTR, firstArg+1);
		LETHE_RET_FALSE(p.EmitBackwardJump(OPC_CALL, base->funAssign));
		p.EmitU24(OPC_POP, 2);
	}

	// FIXME: I should finally switch to C++11 and use lambdas
#define GENDT_FLUSH_COPY() \
		if (lastCopyOfs >= 0) { \
			p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8)); \
			if (lastCopyOfs) p.Emit(OPC_AADD_ICONST + ((UInt)lastCopyOfs << 8)); \
			p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8)); \
			if (lastCopyOfs) p.Emit(OPC_AADD_ICONST + ((UInt)lastCopyOfs << 8)); \
			p.Emit(OPC_PCOPY + ((UInt)copyLen << 8)); \
			lastCopyOfs = -1; \
		}

	Int lastCopyOfs = -1;
	Int copyLen = 0;

	for (Int i=0; i<members.GetSize(); i++)
	{
		const Member &m = members[i];

		if (!m.type.HasDtor())
		{
			if (lastCopyOfs >= 0 && lastCopyOfs + copyLen == m.offset)
			{
				// merge_copy
				copyLen += m.type.GetSize();
				continue;
			}

			// flush-copy, start new
			GENDT_FLUSH_COPY()
			lastCopyOfs = (Int)m.offset;
			copyLen = m.type.GetSize();
			continue;
		}

		GENDT_FLUSH_COPY()
		// okay, we have to do something special here...
		// aw, crap, here we just... oh well...
		p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));

		if (m.offset)
			p.Emit(OPC_AADD_ICONST + ((UInt)m.offset << 8));

		if (m.type.IsPointer())
			p.Emit(OPC_PLOADPTR_IMM);

		p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));

		if (m.offset)
			p.Emit(OPC_AADD_ICONST + ((UInt)m.offset << 8));

		DataTypeEnum mdte = m.type.GetTypeEnum();

		// TODO: more types
		if (mdte == DT_STRING)
		{
			p.Emit(OPC_BCALL + (BUILTIN_PCOPYSTR << 8));
			continue;
		}
		else if (m.type.GetType().funAssign >= 0)
		{
			p.EmitBackwardJump(OPC_CALL, m.type.GetType().funAssign);
			p.EmitI24(OPC_POP, 2);
			continue;
		}

		LETHE_ASSERT(0 && "invalid assignment type!");
	}

	GENDT_FLUSH_COPY()

	if (type == DT_STRING)
	{
		p.EmitI24(OPC_LPUSHPTR, firstArg + 1);
		p.EmitI24(OPC_LPUSHPTR, firstArg + 1);
		p.EmitI24(OPC_BCALL, BUILTIN_PCOPYSTR);
	}
	else if (IsPointer())
	{
		// copy pointers...

		p.Emit(OPC_LPUSHPTR + (UInt(firstArg + 1) << 8));

		p.EmitNameConst(Name(elemType.GetType().name));

		p.EmitI24(OPC_BCALL, type == DT_WEAK_PTR ? BUILTIN_FIX_ADD_WEAK : BUILTIN_FIX_ADD_STRONG);

		p.Emit(OPC_LPUSHPTR + (UInt(firstArg + 1) << 8));

		p.EmitBackwardJump(OPC_CALL, funDtor);
		p.Emit(OPC_PSTOREPTR_IMM);
	}
	else if (elem)
	{
		if (type == DT_DYNAMIC_ARRAY)
		{
			p.EmitI24(OPC_LPUSHPTR, firstArg+1);
			p.EmitI24(OPC_LPUSHPTR, firstArg+1);
			// stack: [dst], [src]
			p.Emit(OPC_LOADTHIS);

			p.EmitIntConst(elem->typeIndex);

			Int fidx = p.cpool.FindNativeFunc("__da_assign");
			LETHE_ASSERT(fidx >= 0);
			p.EmitI24(OPC_NMCALL, fidx);

			p.EmitI24(OPC_POP, 1);
			p.Emit(OPC_POPTHIS);
			p.EmitI24(OPC_POP, 1);
		}
		else
		{
			if (elem->type == DT_STRING)
			{
				p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
				p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
				p.Emit(p.GenIntConst(arrayDims));
				p.Emit(OPC_BCALL + (BUILTIN_VCOPYSTR << 8));
			}
			else if (elem->funVAssign >= 0)
			{
				// call funVAssign!
				p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
				p.Emit(OPC_LPUSHPTR + (UInt(firstArg+1) << 8));
				p.Emit(p.GenIntConst(arrayDims));
				p.EmitBackwardJump(OPC_CALL, elem->funVAssign);
				p.Emit(OPC_POP + (3 << 8));
			}
		}
	}

	p.Emit(OPC_RET);

#undef GENDT_FLUSH_COPY

	p.FlushOpt();
	p.EmitFunc(String::Printf(".vcopy.%s", typeName.Ansi()), 0);

	funVAssign = p.instructions.GetSize();

	// [0] = ret adr, [1] = count, [2] = dst_ptr, [3] = src_ptr

	p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
	p.Emit(OPC_IBNZ_P + (1 << 8));
	p.Emit(OPC_RET);

	p.FlushOpt();

	Int vcloop = p.instructions.GetSize();

	p.Emit(OPC_LPUSHPTR + (UInt(2+firstArg) << 8));

	p.Emit(OPC_LPUSH32 + (UInt(1 + firstArg) << 8));
	p.Emit(p.GenIntConst(-1));
	p.Emit(OPC_IADD);
	p.Emit(OPC_AADD + ((UInt)size << 8));

	if (IsPointer())
		p.Emit(OPC_PLOADPTR_IMM);

	p.Emit(OPC_LPUSHPTR + (UInt(2+firstArg) << 8));
	p.Emit(OPC_LPUSH32 + (UInt(2+firstArg) << 8));
	p.Emit(p.GenIntConst(-1));
	p.Emit(OPC_IADD);
	p.Emit(OPC_AADD + ((UInt)size << 8));

	LETHE_RET_FALSE(p.EmitBackwardJump(OPC_CALL, funAssign));
	p.EmitI24(OPC_POP, 2);

	// dec count
	p.Emit(OPC_LIADD_ICONST + ((UInt)firstArg << 8) + ((UInt)firstArg << 16) + 0xff000000u);
	p.Emit(OPC_LPUSH32 + ((UInt)firstArg << 8));
	LETHE_RET_FALSE(p.EmitBackwardJump(OPC_IBNZ_P, vcloop));

	p.Emit(OPC_RET);

	return true;
}

bool DataType::GenDynArr(CompiledProgram &) const
{
	return true;
	//return p.Error(nullptr, "dynamic arrays not implemented");
}

// QDataType

const DataType QDataType::voidType;

String QDataType::GetName() const
{
	auto res = ref->GetName();

	if (qualifiers & AST_Q_CONST)
	{
		if (qualifiers & AST_Q_METHOD)
			res += " const";
		else
			res = "const " + res;
	}

	if (qualifiers & AST_Q_REFERENCE)
		res += '&';

	return res;
}

bool QDataType::AllowNativeProps() const
{
	const auto dte = GetTypeEnum();

	switch(dte)
	{
	case DT_STRING:
	case DT_ARRAY_REF:
	case DT_DYNAMIC_ARRAY:
		return true;

	default:;
	}

	return false;
}

bool QDataType::IsConst() const
{
	return (qualifiers & AST_Q_CONST) != 0;
}

bool QDataType::IsReference() const
{
	return (qualifiers & AST_Q_REFERENCE) != 0;
}

bool QDataType::IsStatic() const
{
	return (qualifiers & AST_Q_STATIC) != 0;
}

bool QDataType::IsElementary() const
{
	const auto dte = GetTypeEnum();

	return IsNumber() || dte == DT_NAME || dte == DT_STRING;
}

bool QDataType::HasArrayRef() const
{
	return GetType().HasArrayRef();
}

bool QDataType::HasArrayRefWithNonConstElem() const
{
	return GetType().HasArrayRefWithNonConstElem();
}

bool QDataType::IsArray() const
{
	return GetType().IsArray();
}

bool QDataType::IsPointer() const
{
	return GetType().IsPointer();
}

bool QDataType::IsSmartPointer() const
{
	return GetType().IsSmartPointer();
}

bool QDataType::IsStruct() const
{
	return GetType().IsStruct();
}

bool QDataType::IsIndexableStruct() const
{
	return GetType().IsIndexableStruct();
}

bool QDataType::IsFuncPtr() const
{
	return GetType().IsFuncPtr();
}

bool QDataType::IsTernaryCompatible() const
{
	return IsSwitchable() || IsPointer() || GetTypeEnum() == DT_STRUCT;
}

bool QDataType::IsSwitchable() const
{
	switch(GetTypeEnum())
	{
	case DT_BOOL:
	case DT_BYTE:
	case DT_SBYTE:
	case DT_SHORT:
	case DT_USHORT:
	case DT_CHAR:
	case DT_ENUM:
	case DT_INT:
	case DT_UINT:
	case DT_LONG:
	case DT_ULONG:
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_NAME:
	case DT_STRING:
		return true;

	default:
		;
	}

	return false;
}

bool QDataType::IsSigned() const
{
	switch(GetTypeEnumUnderlying())
	{
	case DT_SBYTE:
	case DT_SHORT:
	case DT_ENUM:
	case DT_CHAR:
	case DT_INT:
	case DT_LONG:
		return true;

	default:
		return false;
	}
}

bool QDataType::IsLongInt() const
{
	auto dte = GetTypeEnumUnderlying();
	return dte == DT_LONG || dte == DT_ULONG;
}

bool QDataType::IsSmallNumber() const
{
	return ref->IsSmallNumber();
}

bool QDataType::IsNumber() const
{
	auto dte = GetTypeEnum();
	return dte > DT_NONE && dte <= DT_DOUBLE;
}

bool QDataType::IsFloatingPoint() const
{
	auto dte = GetTypeEnum();
	return dte == DT_FLOAT || dte == DT_DOUBLE;
}

const DataType &QDataType::GetTypeUnderlying() const
{
	return ref->type == DT_ENUM ? *ref->elemType.ref : *ref;
}

DataTypeEnum QDataType::GetTypeEnum() const
{
	return ref->type;
}

DataTypeEnum QDataType::GetTypeEnumUnderlying() const
{
	return ref->type == DT_ENUM ? ref->elemType.ref->type : ref->type;
}

bool QDataType::IsMethodPtr() const
{
	return GetTypeEnum() == DT_FUNC_PTR && (qualifiers & AST_Q_METHOD);
}

bool QDataType::IsProperty() const
{
	return (qualifiers & AST_Q_PROPERTY) != 0;
}

bool QDataType::IsEnumFlags() const
{
	return ref->IsEnumFlags();
}

const DataType *QDataType::GetEnumType() const
{
	if (GetTypeEnum() == DT_ENUM)
		return ref;

	return ref->baseType.GetTypeEnum() == DT_ENUM ? ref->baseType.ref : nullptr;
}

bool QDataType::HasCtor() const
{
	DataTypeEnum dte = GetTypeEnum();

	if (qualifiers & AST_Q_CTOR)
		return true;

	if (IsProperty() || IsReference() || dte <= DT_STRING || dte == DT_FUNC_PTR || dte == DT_DELEGATE || dte == DT_ARRAY_REF)
		return false;

	// note: dynamic arrays don't need ctor; zero-init will do
	if (IsArray())
		return dte == DT_DYNAMIC_ARRAY ? false : ref->elemType.HasCtor();

	for (Int i = 0; ref && i < ref->members.GetSize(); i++)
		if (ref->members[i].type.HasCtor())
			return true;

	// TODO: more...
	return ref ? ref->baseType.HasCtor() : false;
}

bool QDataType::IsRecursive(const DataType *rec) const
{
	StackArray<QDataType, 64> stack;
	StackArray<QDataType, 64> processed;

	stack.Add(*this);

	auto tryAdd = [&](const QDataType &qdt)
	{
		if (qdt.GetTypeEnum() != DT_NONE)
		{
			for (auto &&it : processed)
				if (it == qdt)
					return;

			stack.Add(qdt);
		}
	};

	while (!stack.IsEmpty())
	{
		auto qdt = stack.Back();
		processed.Add(qdt);
		stack.Pop();

		auto dte = qdt.GetTypeEnum();

		if (dte < DT_STRING || dte == DT_DELEGATE || qdt.IsPointer() || dte == DT_ARRAY_REF)
			continue;

		if (qdt.ref == rec)
			return true;

		auto &tpe = qdt.GetType();

		tryAdd(tpe.elemType);
		tryAdd(tpe.baseType);

		for (auto &&it : tpe.members)
			tryAdd(it.type);
	}

	return false;
}

bool QDataType::HasDtor() const
{
	DataTypeEnum dte = GetTypeEnum();

	if (qualifiers & (AST_Q_SKIP_DTOR | AST_Q_PROPERTY))
		return false;

	if ((qualifiers & AST_Q_DTOR) || dte == DT_STRING || dte == DT_STRONG_PTR || dte == DT_WEAK_PTR || dte == DT_CLASS)
		return true;

	if (IsReference() || dte < DT_STRING || dte == DT_FUNC_PTR || dte == DT_DELEGATE || dte == DT_ARRAY_REF)
		return false;

	if (IsArray())
		return ref->elemType.HasDtor();

	for (Int i=0; ref && i<ref->members.GetSize(); i++)
	{
		if (ref->members[i].type.HasDtor())
			return true;
	}

	// TODO: more...
	return ref ? ref->baseType.HasDtor() : false;
}

void QDataType::RemoveReference()
{
	qualifiers &= ~AST_Q_REFERENCE;
}

Int QDataType::GetSize() const
{
	return IsReference() ? (Int)sizeof(IntPtr) : GetType().size;
}

Int QDataType::GetAlign() const
{
	return IsReference() ? (Int)sizeof(IntPtr) : GetType().align;
}

bool QDataType::ZeroInit() const
{
	if (IsReference())
		return false;

	bool explicitNoZero = !IsPointer() && (qualifiers & AST_Q_NOINIT) && !(qualifiers & (AST_Q_CTOR | AST_Q_DTOR));

	if (GetType().type == DT_STRUCT)
	{
		if (qualifiers & (AST_Q_CTOR | AST_Q_DTOR))
			return true;

		// unless noinit, we zero structs with gaps
		if (!explicitNoZero && ((qualifiers | ref->structQualifiers) & AST_Q_HAS_GAPS))
			return true;
	}

	return !explicitNoZero && GetType().ZeroInit();
}

bool QDataType::operator ==(const QDataType &o) const
{
	LETHE_RET_FALSE((qualifiers & AST_Q_TYPE_CMP_MASK) == (o.qualifiers & AST_Q_TYPE_CMP_MASK));
	return *ref == *o.ref;
}

bool QDataType::TypesMatch(const QDataType &o) const
{
	return (qualifiers & AST_Q_TYPE_CMP_MASK) == (o.qualifiers & AST_Q_TYPE_CMP_MASK) && NonRefTypesMatch(o);
}

bool QDataType::NonRefTypesMatch(const QDataType &o) const
{
	// allow static array narrowing match
	return GetType().MatchType(o.GetType(), true);
}

QDataType QDataType::MakeType(const DataType &dt)
{
	QDataType res;
	res.ref = &dt;
	return res;
}

QDataType QDataType::MakeConstType(const DataType &dt)
{
	QDataType res;
	res.ref = &dt;
	res.qualifiers = AST_Q_CONST;
	return res;
}

QDataType QDataType::MakeQType(const DataType &dt, ULong nqualifiers)
{
	QDataType res;
	res.ref = &dt;
	res.qualifiers = nqualifiers;
	return res;
}

bool QDataType::CanAlias(const QDataType &o) const
{
	return CanAssign(o, false, true);
}

bool QDataType::CanAssign(const QDataType &o, bool allowPointers, bool strictStruct) const
{
	DataTypeEnum dte = GetTypeEnum();

	if (dte == DT_ENUM)
	{
		if (o.GetType().IsInteger() && o.ref->baseType.ref == ref)
			return true;

		return o.ref == ref;
	}

	if (dte == DT_ARRAY_REF && o.IsArray())
	{
		// make sure we don't cast out const
		LETHE_RET_FALSE(ref->elemType.IsConst() || !o.ref->elemType.IsConst());

		return *ref->elemType.ref == *o.ref->elemType.ref;
	}

	if (dte == DT_DYNAMIC_ARRAY)
	{
		LETHE_RET_FALSE(o.GetTypeEnum() == DT_DYNAMIC_ARRAY || o.GetTypeEnum() == DT_ARRAY_REF);

		return *ref->elemType.ref == *o.ref->elemType.ref;
	}

	if (dte == DT_STRUCT || dte == DT_CLASS)
	{
		if (ref == o.ref)
			return true;

		LETHE_RET_FALSE(o.GetTypeEnum() == dte);

		if (strictStruct && dte == DT_STRUCT)
			LETHE_RET_FALSE(ref->size == o.ref->size);

		return ref->IsBaseOf(*o.ref);
	}

	if (IsPointer())
	{
		if (o.GetTypeEnum() == DT_NULL)
			return true;

		LETHE_RET_FALSE(o.IsPointer());

		// don't break const-correctness
		if (!IsConst() && o.IsConst())
			return false;

		if (allowPointers)
			return true;

		const auto &el0 = GetType().elemType;
		const auto &el1 = o.GetType().elemType;
		return el0.ref->IsBaseOf(*el1.ref);
	}

	if (dte != DT_FUNC_PTR && dte != DT_DELEGATE)
		return true;

	if (o.GetTypeEnum() == DT_NULL)
		return true;

	// if other is thread unsafe, so must be funcptr/delegate
	if (o.qualifiers & AST_Q_THREAD_UNSAFE)
		LETHE_RET_FALSE(qualifiers & AST_Q_THREAD_UNSAFE);

	if (dte == DT_FUNC_PTR)
	{
		LETHE_RET_FALSE(o.GetTypeEnum() == dte);
		LETHE_RET_FALSE(!((qualifiers | o.qualifiers) & AST_Q_METHOD));
	}
	else
		LETHE_RET_FALSE(o.GetTypeEnum() == DT_DELEGATE || o.IsMethodPtr());

	const auto &f0 = GetType();
	const auto &f1 = o.GetType();

	// compare args and result types

	LETHE_RET_FALSE(f0.elemType == f1.elemType);
	LETHE_RET_FALSE(f0.argTypes.GetSize() == f1.argTypes.GetSize());

	for (Int i=0; i<f0.argTypes.GetSize(); i++)
		LETHE_RET_FALSE(f0.argTypes[i] == f1.argTypes[i]);

	return true;
}

void QDataType::RemoveVirtualProps()
{
	if (qualifiers & AST_Q_TYPEDEF_LOCK)
		return;

	qualifiers |= AST_Q_TYPEDEF_LOCK;

	auto *dt = const_cast<DataType *>(ref);

	for (Int i=dt->members.GetSize()-1; i>=0; i--)
	{
		if (dt->members[i].type.qualifiers & AST_Q_PROPERTY)
		{
			dt->members.EraseIndex(i);
			continue;
		}

		// recurse to members
		dt->members[i].type.RemoveVirtualProps();
	}

	qualifiers &= ~AST_Q_TYPEDEF_LOCK;
}

// DataType

void DataType::GetVariableText(StringBuilder &sb, const void *ptr, Int maxLen) const
{
	HashSet<const void *> hset;
	GetVariableTextInternal(0, false, hset, sb, ptr, maxLen, false, true);
}

bool DataType::ValidReadPtr(const void *ptr, IntPtr size)
{
	(void)ptr;
	(void)size;
#if LETHE_OS_WINDOWS
	if (size > 0)
	{
		const Byte *src = (const Byte *)ptr;
		const Byte *end = src + size;

		while (src < end)
		{
			// reference: https://stackoverflow.com/questions/496034/most-efficient-replacement-for-isbadreadptr
			MEMORY_BASIC_INFORMATION mbi;
			MemSet(&mbi, 0, sizeof(mbi));

			if (::VirtualQuery(src, &mbi, sizeof(mbi)))
			{
				DWORD mask = (PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY);
				bool b = !(mbi.Protect & mask);
				// check the page is not a guard page
				if (mbi.Protect & (PAGE_GUARD|PAGE_NOACCESS)) b = true;

				if (b)
					return false;

				src += mbi.RegionSize;
			}
			else
				return false;
		}
	}
#endif
	return true;
}

void DataType::GetVariableTextInternal(Int bitfld, bool skipReadCheck, HashSet<const void *> &hset, StringBuilder &sb, const void *ptr, Int maxLen, bool baseStruct, bool depth0) const
{
	hset.Add(ptr);

	Int bitSize = bitfld & 65535;
	Int bitOffset = bitfld >> 16;

	if (!skipReadCheck && !ValidReadPtr(ptr, size))
	{
		sb += '?';
		return;
	}

	if (type == DT_RAW_PTR)
		sb += "raw ";

	if (type == DT_WEAK_PTR)
		sb += "weak ";

	(void)maxLen;

	auto bptr = static_cast<const Byte *>(ptr);

	auto maskVal = [=](Int val)->Int
	{
		if (bitSize <= 0)
			return val;

		Int mask = Int(((ULong)1 << bitSize)-1);

		val >>= bitOffset;
		val &= mask;

		return val;
	};

	auto maskValSigned = [=](Int val)->Int
	{
		val = maskVal(val);
		val <<= 32 - bitSize;
		val >>= 32 - bitSize;

		return val;
	};

	switch(type)
	{
	case DT_BOOL:
	{
		auto val = maskVal(*static_cast<const Byte *>(ptr));
		sb += val ? "true" : "false";
	}
	break;

	case DT_CHAR:
	{
		auto val = maskValSigned(*static_cast<const Int *>(ptr));
		char tmp[8] = {0};
		Int len = CharConv::EncodeUTF8(val, tmp, tmp+8);
		String s(tmp, len);
		sb.AppendFormat("u'%s'", s.Escape().Ansi());
	}
	break;

	case DT_BYTE:
	{
		auto val = maskVal(*static_cast<const Byte *>(ptr));
		sb.AppendFormat("%d", val);
	}
	break;

	case DT_SBYTE:
	{
		auto val = maskValSigned(*static_cast<const SByte *>(ptr));
		sb.AppendFormat("%d", val);
	}
	break;

	case DT_SHORT:
	{
		auto val = maskValSigned(*static_cast<const Short *>(ptr));
		sb.AppendFormat("%d", val);
	}
	break;

	case DT_USHORT:
	{
		auto val = maskVal(*static_cast<const UShort *>(ptr));
		sb.AppendFormat("%d", val);
	}
	break;

	case DT_ENUM:
	{
		Long val = 0;

		switch(GetTypeEnumUnderlying())
		{
		case DT_SBYTE: val = *static_cast<const SByte *>(ptr);break;
		case DT_BYTE: val = *static_cast<const Byte *>(ptr);break;
		case DT_SHORT: val = *static_cast<const Short *>(ptr);break;
		case DT_USHORT: val = *static_cast<const UShort *>(ptr);break;
		case DT_INT: val = *static_cast<const Int *>(ptr);break;
		case DT_UINT: val = *static_cast<const UInt *>(ptr);break;
		case DT_LONG: case DT_ULONG: val = (Long)*static_cast<const ULong *>(ptr);break;
		default:;
		}

		bool found = false;

		for (auto &&m : members)
		{
			if (m.offset == val)
			{
				found = true;
				sb += m.name;
				break;
			}
		}

		if (found)
			break;

		// probably enum flags
		bool first = true;

		for (auto &&m : members)
		{
			if (m.offset & val)
			{
				val &= ~m.offset;

				if (!first)
					sb += "/";

				first = false;

				sb += m.name;
			}
		}
	}
	break;

	case DT_INT:
	{
		auto val = maskValSigned(*static_cast<const Int *>(ptr));
		sb.AppendFormat("%d", val);
	}
	break;

	case DT_UINT:
	{
		UInt val = maskVal((Int)*static_cast<const UInt *>(ptr));
		sb.AppendFormat("%u", val);
	}
	break;

	case DT_LONG:
	{
		Long val = *static_cast<const Long *>(ptr);
		sb.AppendFormat(LETHE_FORMAT_LONG, val);
	}
	break;

	case DT_ULONG:
	{
		ULong val = *static_cast<const ULong *>(ptr);
		sb.AppendFormat(LETHE_FORMAT_ULONG, val);
	}
	break;

	case DT_FLOAT:
	{
		Float val = *static_cast<const Float *>(ptr);
		sb.AppendFormat("%g", val);
	}
	break;

	case DT_DOUBLE:
	{
		auto val = *static_cast<const Double *>(ptr);
		sb.AppendFormat("%lg", val);
	}
	break;

	case DT_NAME:
	{
		Name val = *static_cast<const Name *>(ptr);
		// validate name index => note, lo 32 bits is string index which should fit in nt size
		if ((UInt)val.GetValue() >= (UInt)NameTable::Get().GetSize())
			sb += '?';
		else
			sb.AppendFormat("'%s' #%d", val.ToString().Escape().Ansi(), (int)(UInt)val.GetValue());
	}
	break;

	case DT_STRING:
	{
		String val = *static_cast<const String *>(ptr);

		// note: this doesn't cover all validptr cases, because there's StringData before that
		// +1 to include zero-terminator
		if (!val.IsEmpty() && !ValidReadPtr(val.Ansi(), (IntPtr)val.GetLength()+1))
			sb += '?';
		else
			sb.AppendFormat("\"%s\"", val.Escape().Ansi());
	}
	break;

	case DT_RAW_PTR:
	case DT_WEAK_PTR:
	case DT_STRONG_PTR:
	{
		const void *ptrval = *static_cast<const void * const*>(ptr);

		if (!ptrval)
			sb += "null";
		else if (!ValidReadPtr(ptrval, sizeof(BaseObject)))
			sb += "?";
		else
		{
			auto obj = static_cast<const BaseObject *>(ptrval);

			sb.AppendFormat("0x" LETHE_FORMAT_UINTPTR_HEX "#%d/%d", (UIntPtr)ptrval, obj->strongRefCount, obj->weakRefCount);

			if (depth0)
			{
				if (type == DT_WEAK_PTR && !obj->strongRefCount)
					break;

				if (!obj->GetScriptClassType())
					break;

				sb += ' ';
				sb += obj->GetScriptClassType()->name;
				sb += ' ';

				obj->GetScriptClassType()->GetVariableTextInternal(0, false, hset, sb, ptrval, maxLen);
			}
		}
	}
	break;

	case DT_STATIC_ARRAY:
	{
		sb.AppendFormat("[%d]", arrayDims);
		sb += '{';

		if (!ValidReadPtr(bptr, size))
			sb += '?';
		else
		{
			for (Int i=0; i<arrayDims; i++)
			{
				elemType.GetType().GetVariableTextInternal(0, true, hset, sb, bptr, maxLen);

				if (i+1 < arrayDims)
					sb += ", ";

				bptr += elemType.GetSize();

				if (sb.GetLength() > maxLen)
				{
					sb += "...";
					break;
				}
			}
		}

		sb += '}';
	}
	break;

	case DT_ARRAY_REF:
	case DT_DYNAMIC_ARRAY:
	{
		const ArrayRef<Byte> *aref = reinterpret_cast<const ArrayRef<Byte>*>(bptr);
		auto count = aref->GetSize();

		sb.AppendFormat("[%d]", count);
		sb += '{';

		bptr = aref->GetData();

		if (!ValidReadPtr(bptr, (IntPtr)count * elemType.GetSize()))
			sb += '?';
		else
		{
			for (Int i = 0; i<count; i++)
			{
				// avoid infinite recursion
				if (hset.FindIndex(bptr) >= 0)
					sb += "?";
				else
					elemType.GetType().GetVariableTextInternal(0, true, hset, sb, bptr, maxLen);

				if (i + 1 < count)
					sb += ", ";

				bptr += elemType.GetSize();

				if (sb.GetLength() > maxLen)
				{
					sb += "...";
					break;
				}
			}
		}

		sb += '}';
	}
	break;

	case DT_STRUCT:
	case DT_CLASS:
	{
		sb += baseStruct ? "" : "{";

		if (baseType.GetTypeEnum() != DT_NONE)
		{
			auto olen = sb.GetLength();
			baseType.GetType().GetVariableTextInternal(0, false, hset, sb, bptr, maxLen, true);

			if (sb.GetLength() > olen && !members.IsEmpty())
				sb += ", ";
		}

		for (Int i=0; i<members.GetSize(); i++)
		{
			const auto &m = members[i];
			sb += m.name;
			sb += '=';

			auto *mptr = bptr + m.offset;

			if (m.type.IsReference())
				mptr = reinterpret_cast<const Byte *>(*(const void **)mptr);

			Int nbitfld = 0;

			if (m.bitSize > 0)
			{
				// it's a bitfield!
				nbitfld = m.bitSize + m.bitOffset*65536;
			}

			m.type.GetType().GetVariableTextInternal(nbitfld, !m.type.IsReference(), hset, sb, mptr, maxLen);

			if (i+1 < members.GetSize())
				sb += ", ";

			if (sb.GetLength() > maxLen)
			{
				sb += "...";
				break;
			}
		}

		if (!baseStruct)
			sb += '}';
	}
	break;

	case DT_FUNC_PTR:
	case DT_DELEGATE:
	{
		sb += name;
		sb += ' ';
		const void *ptrval = *static_cast<const void * const*>(ptr);

		const void *funptr = ptrval;

		if (type == DT_DELEGATE)
			funptr = static_cast<const void * const*>(ptr)[1];

		if (!ptrval)
			sb += "null";
		else
		{
			if (type == DT_DELEGATE)
			{
				auto vidx = (UIntPtr)funptr;
				sb += String::Printf((vidx & 2) ?
					"struct: 0x" LETHE_FORMAT_UINTPTR_HEX " " :
					"obj: 0x" LETHE_FORMAT_UINTPTR_HEX " ", (UIntPtr)ptrval);

				if (vidx & 1)
					sb += String::Printf("vtblidx: %d", int(vidx >> 2));
				else
				{
					vidx &= ~(UIntPtr)3;
					funptr = (const void *)vidx;
					sb += String::Printf("funptr: 0x" LETHE_FORMAT_UINTPTR_HEX, (UIntPtr)funptr);
				}
			}
			else
				sb += String::Printf("funptr: 0x" LETHE_FORMAT_UINTPTR_HEX, (UIntPtr)funptr);
		}
	}
	break;

	default:
		;
	}
}

size_t DataType::GetMemUsage() const
{
	return sizeof(*this) + isa.GetMemUsage() + argTypes.GetMemUsage() + members.GetMemUsage() + methods.GetMemUsage();
}

const DataType *DataType::GetPointerType(DataTypeEnum dte) const
{
	LETHE_ASSERT(IsPointer());

	if (type == dte)
		return this;

	switch(type)
	{
	case DT_STRONG_PTR:
		return dte == DT_WEAK_PTR ? complementaryType : complementaryType2;
	case DT_WEAK_PTR:
	case DT_RAW_PTR:
		return dte == DT_STRONG_PTR ? complementaryType : complementaryType2;

	default:;
	}

	return nullptr;
}

Int DataType::FindMethodOffset(const StringRef &mname, bool ignoreNative) const
{
	const auto ci = methods.Find(mname);

	if (ci != methods.End())
		return ci->value;

	if (baseType.GetTypeEnum() == DT_NONE)
		return 0;

	if (ignoreNative && (baseType.qualifiers & AST_Q_NATIVE))
		return 0;

	return baseType.GetType().FindMethodOffset(mname, ignoreNative);
}

String DataType::FindMethodName(Int idx) const
{
	if (idx == 0)
		return String();

	for (auto &&it : methods)
		if (it.value == idx)
			return it.key;

	if (baseType.GetTypeEnum() == DT_NONE)
		return String();

	return baseType.GetType().FindMethodName(idx);
}

String DataType::FindMethodName(const ScriptDelegate &dg, const CompiledProgram &prog) const
{
	if (!dg.instancePtr)
		return String();

	// decode
	auto fptr = UIntPtr(dg.funcPtr);

	if (fptr & 1)
	{
		// vtbl index
		fptr &= 0xffffffffu;
		fptr >>= 2;
		return FindMethodName(-(Int)fptr);
	}

	// otherwise it's instruction ptr
	fptr &= ~(UIntPtr)3;
	auto pc = Int(reinterpret_cast<const Instruction *>(fptr) - prog.instructions.GetData());
	return FindMethodName(pc);
}

}
