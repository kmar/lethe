#include "ConstPool.h"
#include <Lethe/Script/TypeInfo/DataTypes.h>
#include <Lethe/Script/Vm/Builtin.h>
#include <Lethe/Core/Math/Math.h>
#include <Lethe/Core/String/StringRef.h>

namespace lethe
{

// ConstPool

ConstPool::ConstPool() : dataAlign(0)
{
	LETHE_VERIFY(RegisterBuiltins(*this));
}

ConstPool::~ConstPool()
{
	// clean up
	for (auto ofs : globalBakedStrings)
		reinterpret_cast<String *>(data.GetData() + ofs)->~String();
}

void ConstPool::Align(Int align)
{
	dataAlign = Max(dataAlign, align);

	Int mask = align-1;
	LETHE_ASSERT(!(align & mask));

	while (data.GetSize() & mask)
		data.Add(0);
}

Int ConstPool::AllocGlobalVar(const QDataType &dt, const String &name)
{
	auto res = AllocGlobal(dt);
	globalVars[name] = res;
	return res;
}

Int ConstPool::AllocGlobal(const QDataType &dt)
{
	Int align = dt.GetAlign();
	Int size = dt.GetSize();

	if (!size)
		return data.GetSize();

	LETHE_ASSERT(align);
	dataAlign = Max(dataAlign, align);
	Int res = data.GetSize();
	Int fillFrom = res;
	Int toAlign = (align - res%align) % align;
	res += toAlign;
	data.Resize(res + size);
	MemSet(data.GetData() + fillFrom, 0, data.GetSize() - fillFrom);
	return res;
}

template< typename T >
Int ConstPool::AddElem(T val, Array<T> &vlist, HashMap<T, Int> &vmap)
{
	Int idx = vmap.FindIndex(val);

	if (idx < 0)
	{
		idx = vlist.Add(val);
		vmap[val] = idx;
	}

	return idx;
}

Int ConstPool::Add(bool val)
{
	return Add((Byte)val);
}

Int ConstPool::Add(Byte val)
{
	return AddElem(val, bPool, bPoolMap);
}

Int ConstPool::Add(SByte val)
{
	return Add((Byte)val);
}

Int ConstPool::Add(UShort val)
{
	return AddElem(val, usPool, usPoolMap);
}

Int ConstPool::Add(Short val)
{
	return Add((UShort)val);
}

Int ConstPool::Add(UInt val)
{
	return AddElem(val, iPool, iPoolMap);
}

Int ConstPool::Add(Int val)
{
	return Add((UInt)val);
}

Int ConstPool::Add(Long val)
{
	return Add((ULong)val);
}

Int ConstPool::Add(ULong val)
{
	return AddElem(val, lPool, lPoolMap);
}

Int ConstPool::Add(Float val)
{
	return AddElem(val, fPool, fPoolMap);
}

Int ConstPool::Add(Double val)
{
	return AddElem(val, dPool, dPoolMap);
}

Int ConstPool::Add(const String &val)
{
	Int idx = sPoolMap.FindIndex(val);

	if (idx < 0)
	{
		idx = sPool.Add(val);
		sPoolMap[val] = idx;
	}

	return idx;
}

Int ConstPool::Add(Name val)
{
	return AddElem(val, nPool, nPoolMap);
}

Int ConstPool::BindNativeFunc(const String &fname, const NativeCallback &cbk)
{
	Int idx = nFunPoolMap.FindIndex(fname);

	if (idx >= 0)
		return nFunPoolMap.GetValue(idx);

	nFunPoolMap[fname] = nFunc.GetSize();
	return nFunc.Add(cbk);
}

Int ConstPool::FindNativeFunc(const String &fname) const
{
	Int idx = nFunPoolMap.FindIndex(fname);
	return idx >= 0 ? nFunPoolMap.GetValue(idx) : idx;
}

const String &ConstPool::GetNativeFuncName(Int idx) const
{
	return nFunPoolMap.GetKey(idx).key;
}

Int ConstPool::BindNativeClass(const String &cname, Int size, Int align, void(*ctor)(void *inst), void(*dtor)(void *inst))
{
	return BindNativeClassInternal(cname, size, align, ctor, dtor, false);
}

Int ConstPool::BindNativeStruct(const String &sname, Int size, Int align)
{
	return BindNativeClassInternal(sname, size, align, nullptr, nullptr, true);
}

Int ConstPool::BindNativeClassInternal(const String &cname, Int size, Int align, void(*ctor)(void *inst), void(*dtor)(void *inst), bool isStruct)
{
	Int idx = nClassPoolMap.FindIndex(cname);

	if (idx >= 0)
		return nClassPoolMap.GetValue(idx);

	NativeClass cls;
	cls.name = cname;
	cls.size = size;
	cls.align = align;
	cls.ctor = ctor;
	cls.dtor = dtor;
	cls.isStruct = isStruct;

	nClassPoolMap[cname] = nClass.GetSize();
	return nClass.Add(cls);
}

Int ConstPool::FindNativeClass(const String &cname) const
{
	return FindNativeClassInternal(cname, false);
}

Int ConstPool::FindNativeStruct(const String &cname) const
{
	return FindNativeClassInternal(cname, true);
}

Int ConstPool::FindNativeClassInternal(const String &cname, bool isStruct) const
{
	Int idx = nClassPoolMap.FindIndex(cname);

	if (idx >= 0)
	{
		idx = nClassPoolMap.GetValue(idx);
		LETHE_ASSERT(idx >= 0);

		if (nClass[idx].isStruct != isStruct)
			idx = -1;
	}

	return idx;
}

void ConstPool::AddGlobalBakedString(Int ofs)
{
	globalBakedStrings.Add(ofs);
}

void ConstPool::ClearGlobalBakedStrings()
{
	globalBakedStrings.Clear();
}

}
