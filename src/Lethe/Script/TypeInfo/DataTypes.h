#pragma once

#include "../Common.h"

#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/String/Name.h>
#include <Lethe/Core/String/StringRef.h>
#include <Lethe/Core/Ptr/UniquePtr.h>
#include <Lethe/Core/Ptr/SharedPtr.h>
#include <Lethe/Core/Memory/BucketAlloc.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Script/ScriptBaseObject.h>

#include "Qualifiers.h"
#include "Attributes.h"

namespace lethe
{

class StringBuilder;
class AstNode;
class AstFunc;
class CompiledProgram;

// note: order DOES matter!
enum DataTypeEnum
{
	DT_NONE,			// none/unknown/void
	DT_BOOL,			// bool type
	DT_BYTE,			// (unsigned)byte
	DT_SBYTE,			// signed byte
	DT_SHORT,
	DT_USHORT,
	DT_CHAR,			// internal char type (same length as Int)
	DT_ENUM,			// enum type
	DT_INT,
	DT_UINT,
	DT_LONG,
	DT_ULONG,
	DT_FLOAT,
	DT_DOUBLE,
	DT_NAME,
	DT_RAW_PTR,			// raw class instance pointer
	DT_FUNC_PTR,		// function ptr
	DT_STRING,
	DT_DELEGATE,		// class instance ptr + function ptr
	DT_NULL,			// special null type
	DT_WEAK_PTR,		// weak class instance pointer
	DT_STRONG_PTR,		// strong class instance pointer
	DT_STRUCT,			// struct type
	DT_CLASS,			// class type
	DT_STATIC_ARRAY,	// static array type
	DT_DYNAMIC_ARRAY,	// dynamic array type
	DT_ARRAY_REF,		// array ref type (pointer + size)
	DT_MAX
};

class DataType;

struct ScriptDelegate : public lethe::ScriptDelegateBase
{
	inline void Clear()
	{
		instancePtr = funcPtr = nullptr;
	}

	inline bool IsEmpty() const
	{
		return !instancePtr;
	}

	inline bool IsVirtual() const
	{
		return !IsEmpty() && ((UIntPtr)funcPtr & 1) != 0;
	}
};

struct LETHE_API QDataType
{
	// ref datatype
	const DataType *ref;
	// qualifiers
	ULong qualifiers;

	inline QDataType()
		: ref(&voidType)
		, qualifiers(0)
	{
	}

	bool AllowNativeProps() const;
	// is const?
	bool IsConst() const;
	// is reference?
	bool IsReference() const;
	// contains array ref?
	bool HasArrayRef() const;
	// is array?
	bool IsArray() const;
	// is pointer?
	bool IsPointer() const;
	// is smart pointer?
	bool IsSmartPointer() const;
	// is struct?
	bool IsStruct() const;
	// is indexable struct? (all members have same type)
	bool IsIndexableStruct() const;
	// can be used in switch?
	bool IsSwitchable() const;
	// can be used in ternary op?
	bool IsTernaryCompatible() const;
	// has array ref with any non-const element?
	bool HasArrayRefWithNonConstElem() const;
	// has custom constructor?
	bool HasCtor() const;
	// has destructor?
	bool HasDtor() const;
	// is illegal recursive type (i.e. struct with a member of itself)
	bool IsRecursive(const DataType *rec) const;
	// needs zero-init?
	bool ZeroInit() const;
	// is elementary numeric type smaller than int?
	bool IsSmallNumber() const;
	// is elementary numeric type?
	bool IsNumber() const;
	// is long integer type (64-bit)?
	bool IsLongInt() const;
	// is func_ptr to method?
	bool IsMethodPtr() const;
	// is virtual property?
	bool IsProperty() const;

	// try to get underlying type for enums, returns nullptr if not an enum
	const DataType *GetEnumType() const;

	// allow pointers: allow incompatible pointers
	bool CanAssign(const QDataType &o, bool allowPointers = false, bool strictStruct = false) const;
	bool CanAlias(const QDataType &o) const;

	static QDataType MakeConstType(const DataType &dt);
	static QDataType MakeType(const DataType &dt);

	bool TypesMatch(const QDataType &o) const;
	bool NonRefTypesMatch(const QDataType &o) const;

	Int GetSize() const;
	Int GetAlign() const;

	void RemoveReference();

	inline const DataType &GetType() const
	{
		return *ref;
	}
	DataTypeEnum GetTypeEnum() const;

	// get/extract human readable type name
	String GetName() const;

	// type matching
	bool operator ==(const QDataType &o) const;
	inline bool operator !=(const QDataType &o) const
	{
		return !(*this == o);
	}

private:
	static const DataType voidType;
};

class NamedScope;

class LETHE_API DataType
{
	LETHE_BUCKET_ALLOC(DataType)
public:
	DataType();

	DataTypeEnum type;
	// alignment
	Int align;
	// size in bytes
	Int size;
	// vtbl offset for class
	Int vtblOffset;
	// vtbl size for class
	Int vtblSize;

	// static array dimensions
	Int arrayDims;
	// type index within program
	Int typeIndex;
	// for class: cached class name
	mutable Name className;
	// elem type ref for arrays
	// return type for function ptr
	QDataType elemType;

	// for structs/classes
	// note: name is only required for debugging
	String name;
	QDataType baseType;
	// class/struct qualifiers (native, placeable)
	ULong structQualifiers;

	// for function pointer
	Array<QDataType> argTypes;

	// complementary type refptr for dynamic arrays/smart pointers
	const DataType *complementaryType;
	// extra complementary type (raw pointer for strong/weak, weak for raw)
	const DataType *complementaryType2;

	// for functions or funcptrs;
	// also custom dtor for composite types
	// (temporary during codegen phase)
	AstNode *funcRef;
	// temporary for ctors
	AstNode *ctorRef;
	// temporary for struct operators
	NamedScope *structScopeRef;

	struct Member
	{
		String name;
		// temporary: for delayed member pointer type resolve inside classes
		AstNode *node = nullptr;
		// per-member user-defined attributes
		SharedPtr<Attributes> attributes;
		QDataType type;
		Int offset = -1;
	};

	// for structs/classes:
	Array<Member> members;
	// for classes only:
	// value: PC offset; 0 = none, <0 => -vtbl index, otherwise PC of final function
	HashMap<String, Int> methods;
	// user-defined attributes (only valid for enums, struct and classes)
	SharedPtr<Attributes> attributes;

	// get complementary pointer type
	const DataType *GetPointerType(DataTypeEnum dte) const;

	// returns method special index or 0 if not found
	Int FindMethodOffset(const StringRef &mname) const;

	// find method name for special index (<0 = -vtbl idx, 0 = none, > 0 = PC)
	// returns empty string if not found
	String FindMethodName(Int idx) const;
	// find method name for delegate
	String FindMethodName(const ScriptDelegate &dg, const CompiledProgram &prog) const;

	// for composite types, bytecode index for ctor/assignment/dtor (and vector counterparts)
	// note: funAssign for pointers has &dst, src; for everything else &dst, &src
	// funCmp is special __cmp function for structs so that we can compare in C++ when sorting arrays etc.
	mutable Int funCtor, funVCtor, funAssign, funVAssign, funDtor, funVDtor, funCmp;

	// for native classes/structs
	void (*nativeCtor)(void *inst);
	void (*nativeDtor)(void *inst);

	// returns true if zero-init needed
	bool ZeroInit() const;

	// is elementary numeric type smaller than int?
	bool IsSmallNumber() const;

	// contains array ref?
	bool HasArrayRef() const;

	// is array type?
	bool IsArray() const;

	// is pointer type?
	bool IsPointer() const;

	// is smart pointer type?
	bool IsSmartPointer() const;

	// is struct type?
	bool IsStruct() const;

	// is indexable struct type?
	bool IsIndexableStruct() const;

	// has array ref with any non-const element?
	bool HasArrayRefWithNonConstElem() const;

	// type matching
	bool operator ==(const DataType &o) const;
	inline bool operator !=(const DataType &o) const
	{
		return !(*this == o);
	}

	// narrow static: if true, static arrays can match if #this <= #o
	bool MatchType(const DataType &o, bool narrowStatic = 0) const;

	const Member *FindMember(const StringRef &name) const;

	static DataTypeEnum ComposeTypeEnum(DataTypeEnum t0, DataTypeEnum t1);

	bool IsBaseOf(const DataType &o) const;

	// special helper
	bool HasMemberVarsAfter(const DataType *clsBase) const;

	// used by Vm for class type matching
	bool IsA(Name n) const;

	// generate constructor
	bool GenCtor(CompiledProgram &p) const;

	// generate destructor
	// cdtor: custom destructor func
	bool GenDtor(CompiledProgram &p) const;

	// gen isa array for class type
	void GenBaseChain() const;

	// generate dynamic array methods
	bool GenDynArr(CompiledProgram &p) const;

	// get/extract full human readable type name
	String GetName() const;

	// get simple human readable type name (for const conv errors)
	static String GetSimpleTypeName(DataTypeEnum dte);

	// for debugging
	void GetVariableText(StringBuilder &sb, const void *ptr, Int maxLen = 1024) const;

	size_t GetMemUsage() const;

private:
	// class inheritance name list for dynamic casts (sorted)
	mutable StackArray<Name, 16> isa;

	// only works for numeric types
	static Int GetTypeEnumSize(DataTypeEnum t);
	// TODO: handle const?
	static DataTypeEnum EvalTypeEnum(DataTypeEnum t);
	static bool IsNumberEnum(DataTypeEnum t0);

	void GetVariableTextInternal(StringBuilder &sb, const void *ptr, Int maxLen = 256, bool baseStruct = false, bool depth0 = false) const;
	static bool ValidReadPtr(const void *ptr, Int size);
};

}
