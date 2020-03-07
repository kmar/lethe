#pragma once

#include "../Common.h"

#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/String/String.h>
#include <Lethe/Core/String/Name.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Core/Collect/HashMap.h>
#include <Lethe/Core/Thread/Atomic.h>

namespace lethe
{

class Stack;
class DataType;
struct QDataType;

struct NativeClass
{
	// fully qualified name
	String name;
	// members: name, offset
	HashMap<String, Int> members;
	// callbacks
	void (*ctor)(void *inst);
	void (*dtor)(void *inst);
	// size
	Int size;
	Int align;
	bool isStruct = false;

	void SwapWith(NativeClass &o)
	{
		Swap(name, o.name);
		Swap(members, o.members);
		Swap(ctor, o.ctor);
		Swap(dtor, o.dtor);
		Swap(size, o.size);
		Swap(align, o.align);
		Swap(isStruct, o.isStruct);
	}
};

class LETHE_API ConstPool
{
public:
	ConstPool();
	~ConstPool();

	// should also have some mechanism to init arrays and structs... but structs may need ctors
	// FIXME: really need byte const pool?
	Array<Byte> bPool;
	Array<UShort> usPool;
	Array<UInt> iPool;
	Array<ULong> lPool;
	Array<Float> fPool;
	Array<Double> dPool;
	// pool for const strings
	Array<String> sPool;
	// pool for const names
	Array<Name> nPool;
	// global data
	CacheAlignedArray< Byte > data;

	typedef void (*NativeCallback)(Stack &stk);
	typedef const char *(*NativeCallbackTrap)(Stack &stk);

	// bound native functions
	Array< NativeCallback > nFunc;

	// bound native classes
	Array<NativeClass> nClass;

	// bind native static functions
	Int BindNativeFunc(const String &fname, const NativeCallback &cbk);
	// find native func
	Int FindNativeFunc(const String &fname) const;
	// debug: get native func name
	const String &GetNativeFuncName(Int idx) const;

	// bind native class functions
	Int BindNativeClass(const String &cname, Int size, Int align, void (*ctor)(void *inst), void (*dtor)(void *inst));
	// find native class
	Int FindNativeClass(const String &cname) const;

	// bind native struct functions
	Int BindNativeStruct(const String &sname, Int size, Int align);
	// find native struct
	Int FindNativeStruct(const String &sname) const;

	// returns byte offset
	Int AllocGlobal(const QDataType &dt);
	Int AllocGlobalVar(const QDataType &dt, const String &name);

	inline Byte *GetGlobalData() const
	{
		return const_cast<Byte *>(data.GetData());
	}

	void AddGlobalBakedString(Int ofs);
	// called when running global ctors
	void ClearGlobalBakedStrings();

	Int Add(bool val);
	Int Add(Byte val);
	Int Add(SByte val);
	Int Add(UShort val);
	Int Add(Short val);
	Int Add(Int val);
	Int Add(UInt val);
	Int Add(Long val);
	Int Add(ULong val);
	Int Add(Float val);
	Int Add(Double val);
	Int Add(const String &val);
	Int Add(Name val);

	// align data
	void Align(Int align);

	HashMap< Float, Int > fPoolMap;
	HashMap< Double, Int > dPoolMap;
	// name => offset
	HashMap<String, Int> globalVars;

	// number of live script objects
	mutable AtomicInt liveScriptObjects = 0;

private:
	HashMap< Byte, Int > bPoolMap;
	HashMap< UShort, Int > usPoolMap;
	HashMap< UInt, Int > iPoolMap;
	HashMap< ULong, Int > lPoolMap;
	HashMap< String, Int > sPoolMap;
	HashMap< Name, Int > nPoolMap;
	HashMap< String, Int > nFunPoolMap;
	HashMap< String, Int > nClassPoolMap;

	// offsets for global baked strings
	Array<Int> globalBakedStrings;

	// maximum data alignment
	Int dataAlign;

	template< typename T >
	static Int AddElem(T val, Array<T> &vlist, HashMap<T, Int> &vmap);

	Int BindNativeClassInternal(const String &cname, Int size, Int align, void (*ctor)(void *inst), void (*dtor)(void *inst), bool isStruct);
	Int FindNativeClassInternal(const String &cname, bool isStruct) const;
};

}
