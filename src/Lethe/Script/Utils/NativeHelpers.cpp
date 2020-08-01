#include <Lethe/Core/String/String.h>
#include <Lethe/Core/String/StringRef.h>
#include <Lethe/Core/Math/Math.h>
#include "NativeHelpers.h"
#include <Lethe/Script/Program/CompiledProgram.h>
#include <Lethe/Script/Vm/Vm.h>
#include "../ScriptEngine.h"
#include <Lethe/Script/TypeInfo/BaseObject.h>

#include <stdlib.h>

// TODO:
// - lower_bound/upper_bound for set
// - erase for set
// - erase should return next index
// - remove to remove element(s)
// - get min/max for set + pred/succ

namespace lethe
{

namespace
{

struct DynamicArray
{
	struct CompareHandle;

	typedef Int (*compareFunc)(const CompareHandle &h, const void *a, const void *b);

	struct CompareHandle
	{
		ScriptContext *ctx;
		compareFunc cfun;
		Int funCmp;
	};

	Byte *data = nullptr;
	Int size = 0;
	Int reserve = 0;

	inline Int Fix(Int idx) const
	{
		return UInt(idx) >= (UInt)size ? -1 : idx;
	}

	inline Int GetCapacity() const
	{
		return Abs(reserve);
	}

	// dt: element type
	void Clear(ScriptContext &ctx, const DataType &dt);
	void Reset(ScriptContext &ctx, const DataType &dt);
	void Pop(ScriptContext &ctx, const DataType &dt);
	void Shrink(ScriptContext &ctx, const DataType &dt);
	void Resize(ScriptContext &ctx, const DataType &dt, Int newSize);
	void Reserve(ScriptContext &ctx, const DataType &dt, Int newSize);
	void EnsureCapacity(ScriptContext &ctx, const DataType &dt, Int newSize);
	void Reallocate(ScriptContext &ctx, const DataType &dt, Int newReserve);
	Int Push(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	Int PushUnique(ScriptContext &ctx, const DataType &dt, const void *valuePtr);

	bool PushHeap(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	bool PopHeap(ScriptContext &ctx, const DataType &dt, void *valuePtr);

	Int Find(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	Int LowerBound(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	Int UpperBound(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	Int FindSorted(ScriptContext &ctx, const DataType &dt, const void *valuePtr, Int &lbound);
	Int InsertSorted(ScriptContext &ctx, const DataType &dt, const void *valuePtr);
	Int InsertSortedUnique(ScriptContext &ctx, const DataType &dt, const void *valuePtr);

	void Assign(ScriptContext &ctx, const DataType &dt, const ArrayRef<Byte> *aref);
	bool Insert(ScriptContext &ctx, const DataType &dt, const void *valuePtr, Int index);
	bool Erase(ScriptContext &ctx, const DataType &dt, Int index);
	bool EraseUnordered(ScriptContext &ctx, const DataType &dt, Int index);
	void Reverse(const DataType &dt);
	void Sort(ScriptContext &ctx, const DataType &dt);

	// element construction

	static void ConstructObjectRange(ScriptContext &ctx, const DataType &dt, Byte *ptr, Int elemCount);
	static void DestroyObjectRange(ScriptContext &ctx, const DataType &dt, Byte *ptr, Int elemCount);
	static void CopyObjectRange(ScriptContext &ctx, const DataType &dt, Byte *dst, const Byte *src, Int elemCount);

	compareFunc GetCompareFunc(ScriptContext &ctx, const DataType &dt, CompareHandle &ch) const;
	compareFunc GetCompareFuncInternal(ScriptContext &ctx, const DataType &dt, CompareHandle &ch) const;
};

DynamicArray::compareFunc DynamicArray::GetCompareFunc(ScriptContext &ctx, const DataType &dt, CompareHandle &ch) const
{
	auto res = GetCompareFuncInternal(ctx, dt, ch);
	ch.cfun = res;
	return res;
}

DynamicArray::compareFunc DynamicArray::GetCompareFuncInternal(ScriptContext &ctx, const DataType &dt, CompareHandle &ch) const
{
	ch.ctx = &ctx;
	ch.funCmp = -1;

#define ELEM_COMPARE(dt, type) \
	case dt: return [](const CompareHandle &, const void *a, const void *b)->Int \
	{ \
		auto ta = *static_cast<const type *>(a); \
		auto tb = *static_cast<const type *>(b); \
		return (ta > tb) - (ta < tb); \
	};

	switch(dt.type)
	{
	ELEM_COMPARE(DT_BOOL, bool)
	ELEM_COMPARE(DT_BYTE, Byte)
	ELEM_COMPARE(DT_SBYTE, SByte)
	ELEM_COMPARE(DT_USHORT, UShort)
	ELEM_COMPARE(DT_SHORT, Short)
	ELEM_COMPARE(DT_INT, Int)
	ELEM_COMPARE(DT_CHAR, Int)
	ELEM_COMPARE(DT_NAME, Int)
	ELEM_COMPARE(DT_ENUM, Int)
	ELEM_COMPARE(DT_UINT, UInt)
	ELEM_COMPARE(DT_LONG, Long)
	ELEM_COMPARE(DT_ULONG, ULong)
	ELEM_COMPARE(DT_FLOAT, Float)
	ELEM_COMPARE(DT_DOUBLE, Double)
	ELEM_COMPARE(DT_FUNC_PTR, UIntPtr)
	ELEM_COMPARE(DT_RAW_PTR, UIntPtr)
	ELEM_COMPARE(DT_STRONG_PTR, UIntPtr)
	ELEM_COMPARE(DT_WEAK_PTR, UIntPtr)

	case DT_STRING:
		return [](const CompareHandle &, const void *a, const void *b)->Int
		{
			auto &sa = *static_cast<const String *>(a);
			auto &sb = *static_cast<const String *>(b);

			return (sb < sa) - (sa < sb);
		};

	case DT_DELEGATE:
		return [](const CompareHandle &, const void *a, const void *b)->Int
		{
			auto sa = static_cast<const void * const *>(a);
			auto sb = static_cast<const void * const *>(b);

			if (sa[0] != sb[0])
				return (sb[0] < sa[0]) - (sa[0] < sb[0]);

			return (sb[1] < sa[1]) - (sa[1] < sb[1]);
		};

	case DT_STRUCT:
		ch.funCmp = dt.funCmp;

		if (dt.funCmp < 0)
			return nullptr;

		return [](const CompareHandle &h, const void *a, const void *b)->Int
		{
			auto &ctx = *h.ctx;
			auto &stk = ctx.GetStack();

			stk.PushInt(0);
			stk.PushPtr(b);
			stk.PushPtr(a);
			ctx.CallOffset(h.funCmp);
			auto res = stk.GetSignedInt(2);
			stk.Pop(3);
			return res;
		};
		break;

	default:;
	}

#undef ELEM_COMPARE

	return nullptr;
}

void DynamicArray::Resize(ScriptContext &ctx, const DataType &dt, Int newSize)
{
	LETHE_ASSERT(newSize >= 0);
	newSize = MaxZero(newSize);

	Int cap = GetCapacity();

	if (newSize > cap)
	{
		if (!cap)
			Reserve(ctx, dt, newSize);
		else
			EnsureCapacity(ctx, dt, newSize);
	}

	if (size > newSize)
		DestroyObjectRange(ctx, dt, data + dt.size*newSize, size - newSize);
	else
		ConstructObjectRange(ctx, dt, data + dt.size*size, newSize - size);

	size = newSize;
}

void DynamicArray::Reallocate(ScriptContext &ctx, const DataType &dt, Int newReserve)
{
	if (newReserve == GetCapacity())
		return;

	Byte *newData = newReserve ?
		static_cast<Byte *>(AlignedAlloc::Realloc(reserve < 0 ? nullptr : data, (size_t)(newReserve) * dt.size, dt.align)) : nullptr;
	Int newSize = Min(size, newReserve);

	if (newData != data)
	{
		if (!newData)
		{
			// keep SA happy
			newSize = 0;
		}

		ConstructObjectRange(ctx, dt, newData, newSize);

		if (data)
		{
			CopyObjectRange(ctx, dt, newData, data, newSize);

			DestroyObjectRange(ctx, dt, data, size);

			if (reserve > 0)
				AlignedAlloc::Free(data);
		}
	}
	else
		LETHE_ASSERT(newSize == size);

	reserve = newReserve;
	size = newSize;
	data = newData;
}

void DynamicArray::Reserve(ScriptContext &ctx, const DataType &dt, Int newReserve)
{
	LETHE_ASSERT(newReserve >= 0);
	newReserve = MaxZero(newReserve);

	if (GetCapacity() < newReserve)
		Reallocate(ctx, dt, newReserve);
}

void DynamicArray::EnsureCapacity(ScriptContext &ctx, const DataType &dt, Int newCapacity)
{
	LETHE_ASSERT(newCapacity >= 0);

	Int newReserve = GetCapacity();

	while (newCapacity > newReserve)
		newReserve = newReserve < 2 ? newReserve + 1 : newReserve * 3 / 2;

	Reserve(ctx, dt, newReserve);
}

void DynamicArray::ConstructObjectRange(ScriptContext &ctx, const DataType &dt, Byte *ptr, Int elemCount)
{
	if (elemCount <= 0)
		return;

	LETHE_ASSERT(ptr);
	MemSet(ptr, 0, elemCount * dt.size);

	if (dt.funCtor < 0)
		return;

	LETHE_ASSERT(dt.type != DT_RAW_PTR);

	// make (v)ctor script call!!!
	if (elemCount == 1)
	{
		ctx.GetStack().PushPtr(ptr);
		ctx.CallOffset(dt.funCtor);
		ctx.GetStack().Pop(1);
	}
	else
	{
		ctx.GetStack().PushPtr(ptr);
		ctx.GetStack().PushInt(elemCount);
		ctx.CallOffset(dt.funVCtor);
		ctx.GetStack().Pop(2);
	}
}

void DynamicArray::DestroyObjectRange(ScriptContext &ctx, const DataType &dt, Byte *ptr, Int elemCount)
{
	if (dt.type == DT_STRING)
	{
		// strings require special handling!
		auto *pstr = reinterpret_cast<String *>(ptr);

		while (elemCount-- > 0)
			pstr++->~String();

		return;
	}

	if (dt.funDtor < 0 || elemCount <= 0)
		return;

	LETHE_ASSERT(dt.type != DT_RAW_PTR);

	// make (v)dtor script call!!
	if (elemCount == 1)
	{
		ctx.GetStack().PushPtr(ptr);
		ctx.CallOffset(dt.funDtor);
		ctx.GetStack().Pop(1);
	}
	else
	{
		ctx.GetStack().PushPtr(ptr);
		ctx.GetStack().PushInt(elemCount);
		ctx.CallOffset(dt.funVDtor);
		ctx.GetStack().Pop(2);
	}
}

void DynamicArray::CopyObjectRange(ScriptContext &ctx, const DataType &dt, Byte *dst, const Byte *src, Int elemCount)
{
	LETHE_ASSERT(!elemCount || (src && dst));

	if (dt.type == DT_STRING)
	{
		auto *psrc = reinterpret_cast<const String *>(src);
		auto *pdst = reinterpret_cast<String *>(dst);

		while (elemCount-- > 0)
			*pdst++ = *psrc++;

		return;
	}

	if (dt.funAssign < 0 || elemCount <= 0)
	{
		if (elemCount > 0)
			MemCpy(dst, src, elemCount * dt.size);

		return;
	}

	// make (v)assign script call!!
	if (elemCount == 1)
	{
		// note: raw ptr will be simply copied, no funAssign
		LETHE_ASSERT(dt.type != DT_RAW_PTR);

		if (dt.type == DT_STRONG_PTR || dt.type == DT_WEAK_PTR)
		{
			// must dereference src
			src = *(Byte **)src;
		}

		ctx.GetStack().PushPtr(src);
		ctx.GetStack().PushPtr(dst);
		ctx.CallOffset(dt.funAssign);
		ctx.GetStack().Pop(2);
	}
	else
	{
		ctx.GetStack().PushPtr(src);
		ctx.GetStack().PushPtr(dst);
		ctx.GetStack().PushInt(elemCount);
		ctx.CallOffset(dt.funVAssign);
		ctx.GetStack().Pop(3);
	}
}

Int DynamicArray::Push(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	if (LETHE_UNLIKELY(size >= GetCapacity()))
		EnsureCapacity(ctx, dt, size + 1);

	auto dptr = data + size*dt.size;
	ConstructObjectRange(ctx, dt, dptr, 1);
	LETHE_ASSERT(data);
	CopyObjectRange(ctx, dt, dptr, static_cast<const Byte *>(valuePtr), 1);
	return size++;
}

bool DynamicArray::PushHeap(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return false;
	}

	auto index = size;
	Push(ctx, dt, valuePtr);

	// percolate up
	Int pindex;

	auto sz = dt.size;

	auto GetParent = [](Int index)->Int
	{
		return ((index + 1) >> 1) - 1;
	};

	while (index > 0 && cfun(ch, valuePtr, data + (pindex = GetParent(index))*sz) < 0)
	{
		MemSwap(data + index*sz, data + pindex*sz, sz);
		index = pindex;
	}

	return true;
}

bool DynamicArray::PopHeap(ScriptContext &ctx, const DataType &dt, void *valuePtr)
{
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return false;
	}

	if (!size)
		return false;

	auto sz = dt.size;

	// we need to destroy the object before moving
	DestroyObjectRange(ctx, dt, static_cast<Byte *>(valuePtr), 1);

	// move out
	MemCpy(valuePtr, data, sz);

	// move last
	MemCpy(data, data + (size - 1)*sz, sz);
	// decrease size
	size--;

	// now: percolate down
	Int child;
	auto tsz = size;

	auto GetRightChild = [](Int index) -> Int
	{
		return 2 * index + 2;
	};

	Int index = 0;

	while ((child = GetRightChild(index)) < tsz)
	{
		if (cfun(ch, data + (child - 1)*sz, data + child*sz) < 0)
			child--;

		if (cfun(ch, data + child*sz, data + index*sz) >= 0)
			return true;

		MemSwap(data + index*sz, data + child*sz, sz);
		index = child;
	}

	--child;
	LETHE_ASSERT(child >= 0);

	if (child < tsz && cfun(ch, data + child*sz, data + index*sz) < 0)
		MemSwap(data + index*sz, data + child*sz, sz);

	return true;
}

Int DynamicArray::PushUnique(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	auto where = Find(ctx, dt, valuePtr);

	if (where >= 0)
		return where;

	return Push(ctx, dt, valuePtr);
}

Int DynamicArray::Find(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return -1;
	}

	auto sz = dt.size;

	auto *dptr = data;

	for (int i=0; i<size; i++)
	{
		if (cfun(ch, dptr, valuePtr) == 0)
			return i;

		dptr += sz;
	}

	return -1;
}

Int DynamicArray::LowerBound(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return -1;
	}

	Int from = 0;

	Int count = size;
	Int ci;

	Int sz = dt.size;

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (cfun(ch, data + ci*sz, valuePtr) < 0)
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

Int DynamicArray::UpperBound(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return -1;
	}

	Int from = 0;

	Int count = size;
	Int ci;

	Int sz = dt.size;

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (cfun(ch, valuePtr, data + ci * sz) >= 0)
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	return from;
}

Int DynamicArray::FindSorted(ScriptContext &ctx, const DataType &dt, const void *valuePtr, Int &lbound)
{
	// TODO: reuse lowerBound to simplify code
	CompareHandle ch;
	auto cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return -1;
	}

	Int from = 0;

	Int count = size;
	Int ci;

	Int sz = dt.size;

	while (count > 0)
	{
		ci = from;
		Int step = count >> 1;
		ci += step;

		if (cfun(ch, data + ci * sz, valuePtr) < 0)
		{
			from = ++ci;
			count -= step + 1;
		}
		else
			count = step;
	}

	lbound = from;

	if ((UInt)from < (UInt)size)
	{
		if (cfun(ch, data + from * sz, valuePtr) == 0)
			return from;
	}

	return -1;
}

Int DynamicArray::InsertSorted(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	auto idx = UpperBound(ctx, dt, valuePtr);
	return Insert(ctx, dt, valuePtr, idx);
}

Int DynamicArray::InsertSortedUnique(ScriptContext &ctx, const DataType &dt, const void *valuePtr)
{
	Int lbound = -1;
	auto idx = FindSorted(ctx, dt, valuePtr, lbound);

	if (idx >= 0)
		return idx;

	Insert(ctx, dt, valuePtr, lbound);
	return lbound;
}

void DynamicArray::Sort(ScriptContext &ctx, const DataType &dt)
{
	thread_local CompareHandle ch;
	thread_local compareFunc cfun;
	cfun = GetCompareFunc(ctx, dt, ch);

	if (!cfun)
	{
		// TODO: exception?
		return;
	}

	qsort(data, size, dt.size, [](const void *a, const void *b)->int
	{
		return cfun(ch, a, b);
	});
}

bool DynamicArray::Insert(ScriptContext &ctx, const DataType &dt, const void *valuePtr, Int index)
{
	// we don't want to die here because of bad index => aborting
	if ((UInt)index > (UInt)size)
		return false;

	if (LETHE_UNLIKELY(size >= GetCapacity()))
		EnsureCapacity(ctx, dt, size + 1);

	// okay, now make space using memmove
	MemMove(data + (index+1)*dt.size, data + index*dt.size, (size - index)*dt.size);

	auto dptr = data + index*dt.size;
	ConstructObjectRange(ctx, dt, dptr, 1);
	LETHE_ASSERT(data);

	CopyObjectRange(ctx, dt, dptr, static_cast<const Byte *>(valuePtr), 1);
	size++;
	return true;
}

bool DynamicArray::Erase(ScriptContext &ctx, const DataType &dt, Int index)
{
	// we don't want to die here because of bad index => aborting
	if ((UInt)index >= (UInt)size)
		return false;

	LETHE_ASSERT(data);

	auto dptr = data + index*dt.size;
	DestroyObjectRange(ctx, dt, dptr, 1);

	// okay, now move
	MemMove(data + index*dt.size, data + (index + 1)*dt.size, (size - index - 1)*dt.size);

	size--;
	return true;
}

bool DynamicArray::EraseUnordered(ScriptContext &ctx, const DataType &dt, Int index)
{
	// we don't want to die here because of bad index => aborting
	if ((UInt)index >= (UInt)size)
		return false;

	LETHE_ASSERT(data);

	auto dptr = data + index*dt.size;
	DestroyObjectRange(ctx, dt, dptr, 1);

	// okay, now copy last element
	MemCpy(data + index*dt.size, data + (size-1)*dt.size, dt.size);

	size--;
	return true;
}

void DynamicArray::Reverse(const DataType &dt)
{
	auto sz = dt.size;

	for (Int i=0; i<size/2; i++)
		MemSwap(data + i*sz, data + (size-i-1)*sz, sz);
}

void DynamicArray::Assign(ScriptContext &ctx, const DataType &dt, const ArrayRef<Byte> *aref)
{
	auto adata = aref->GetData();

	// avoid crashes due to overlaps!!!
	if (data && adata && adata >= data && adata < data + dt.size*size)
		return;

	Resize(ctx, dt, aref->GetSize());
	CopyObjectRange(ctx, dt, data, adata, size);
}

void DynamicArray::Clear(ScriptContext &ctx, const DataType &dt)
{
	Resize(ctx, dt, 0);
}

void DynamicArray::Reset(ScriptContext &ctx, const DataType &dt)
{
	Reallocate(ctx, dt, 0);
}

void DynamicArray::Pop(ScriptContext &ctx, const DataType &dt)
{
	Resize(ctx, dt, size-1);
}

void DynamicArray::Shrink(ScriptContext &ctx, const DataType &dt)
{
	Reallocate(ctx, dt, size);
}

// wrappers

void daResize(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto nsize = stk.GetSignedInt(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Resize(ctx, *dt, nsize);
}

void daReserve(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto nreserve = stk.GetSignedInt(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Reserve(ctx, *dt, nreserve);
}

void daClear(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Clear(ctx, *dt);
}

void daReset(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Reset(ctx, *dt);
}

void daPop(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Pop(ctx, *dt);
}

void daShrink(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Shrink(ctx, *dt);
}

void daPush(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto pushValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Push(ctx, *dt, pushValue);
	// TODO: return index (or void?)
}

void daPushUnique(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto pushValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->PushUnique(ctx, *dt, pushValue);
	// TODO: return index (or void?)
}

void daPushHeap(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto pushValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->PushHeap(ctx, *dt, pushValue);
}

void daPopHeap(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2);
	darr->PopHeap(ctx, *dt, res);
}

void daSlice(Stack &stk)
{
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto elemSize = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto from = stk.GetSignedInt(2);
	auto to = stk.GetSignedInt(3);
	auto res = reinterpret_cast<ArrayRef<Byte> *>(stk.GetTop() + 4);

	// slice now
	auto data = darr->data;
	auto size = darr->size;

	if (to < 0)
		to = size;

	if (from < 0)
		from = 0;

	if (from >= to)
		from = to = 0;

	res->Init(data + from*elemSize, to - from);
}

void daFind(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE-1) / Stack::WORD_SIZE));
	*res = darr->Find(ctx, *dt, findValue);
}

void daLowerBound(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE));
	*res = darr->LowerBound(ctx, *dt, findValue);
}

void daUpperBound(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE));
	*res = darr->UpperBound(ctx, *dt, findValue);
}

void daFindSorted(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE));
	Int tmp;
	*res = darr->FindSorted(ctx, *dt, findValue, tmp);
}

void daInsertSorted(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE));
	*res = darr->InsertSorted(ctx, *dt, findValue);
}

void daInsertSortedUnique(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto findValue = reinterpret_cast<void *>(stk.GetTop() + 2);
	auto dt = stk.GetProgram().types[ntype].Get();
	auto res = reinterpret_cast<Int *>(stk.GetTop() + 2 + ((dt->size + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE));
	*res = darr->InsertSortedUnique(ctx, *dt, findValue);
}

void daSort(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Sort(ctx, *dt);
}

void daInsert(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto pushValue = reinterpret_cast<void *>(stk.GetTop() + 3);
	auto index = stk.GetSignedInt(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Insert(ctx, *dt, pushValue, index);
}

void daErase(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto index = stk.GetSignedInt(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Erase(ctx, *dt, index);
}

void daEraseUnordered(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto index = stk.GetSignedInt(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->EraseUnordered(ctx, *dt, index);
}

void daReverse(Stack &stk)
{
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Reverse(*dt);
}

void daAssign(Stack &stk)
{
	auto &ctx = stk.GetContext();
	auto darr = static_cast<DynamicArray *>(const_cast<void *>(stk.GetThis()));
	auto ntype = stk.GetSignedInt(0);
	// skip saved this (arg 1)
	auto aref = stk.GetPtr(2);
	auto dt = stk.GetProgram().types[ntype].Get();
	darr->Assign(ctx, *dt, static_cast<const ArrayRef<Byte> *>(aref));
}

void daFromAref(Stack &stk)
{
	auto &ctx = stk.GetContext();

	auto ntype = stk.GetSignedInt(0);
	const ArrayRef<Byte> &aref = *reinterpret_cast<ArrayRef<Byte> *>(stk.GetTop() + 1);

	auto dt = stk.GetProgram().types[ntype].Get();

	DynamicArray darr;
	darr.Resize(ctx, *dt, aref.GetSize());
	DynamicArray::CopyObjectRange(ctx, *dt, darr.data, aref.GetData(), aref.GetSize());

	// now: pop 3
	stk.Pop(3);
	stk.PushRaw((sizeof(darr) + Stack::WORD_SIZE - 1) / Stack::WORD_SIZE);

	MemCpy(stk.GetTop(), &darr, sizeof(darr));
}

const DataType *FindCommonBase(const DataType *cls0, const DataType *cls1)
{
	while (cls0)
	{
		if (cls0->IsBaseOf(*cls1))
			return cls0;

		cls0 = cls0->baseType.ref;
	}

	return nullptr;
}

void objSetVtable(Stack &stk)
{
	// stack: [0] = pushed this, [1] = name index, [2] = result (bool)
	// param: name (new class)
	auto nidx = stk.GetSignedInt(1);

	Name n;
	n.SetIndex(nidx);

	Int res = 0;

	auto obj = (BaseObject *)stk.GetThis();

	do
	{
		if (!obj)
			break;

		auto objCls = obj->GetScriptClassType();
		auto cls = stk.GetProgram().FindClass(n);

		// break if class not found or vtblSize/obj size incompatible
		if (!cls || objCls->vtblSize != cls->vtblSize || objCls->size != cls->size)
			break;

		// find common base class
		auto cbase = FindCommonBase(cls, objCls);

		if (!cbase)
			break;

		// also check that common base vtblSize and type size match
		if (objCls->vtblSize != cbase->vtblSize || objCls->size != cbase->size)
			break;

		// last thing to check: make sure that no extra members were defined
		if (objCls->HasMemberVarsAfter(cbase) || cls->HasMemberVarsAfter(cbase))
			break;

		obj->scriptVtbl = (void **)(stk.GetConstantPool().GetGlobalData() + cls->vtblOffset);
		stk.GetContext().GetEngine().onVtableChange(obj, n);
		res = 1;
	} while(false);

	stk.SetInt(2, res);
}

void objGetClassName(Stack &stk)
{
	// stack: [0] = pushed this, [1] = result (name)
	// param: none
	auto obj = (BaseObject *)stk.GetThis();
	auto *objCls = obj->GetScriptClassType();

	stk.SetInt(1, objCls->className.GetIndex());
}

void objGetNonStateClassName(Stack &stk)
{
	// stack: [0] = pushed this, [1] = result (name)
	// param: none
	auto obj = (BaseObject *)stk.GetThis();
	auto *objCls = obj->GetScriptClassType();

	while (objCls->structQualifiers & AST_Q_STATE)
		objCls = &objCls->baseType.GetType();

	stk.SetInt(1, objCls->className.GetIndex());
}

void objFixStateName(Stack &stk)
{
	ArgParserMethod ap(stk);
	auto sname = ap.Get<Name>();
	auto &res = ap.Get<Name>();
	res = sname;
	auto *obj = (BaseObject *)stk.GetThis();
	auto *objCls = obj->GetScriptClassType();

	// first, find non-state class
	while (objCls->structQualifiers & AST_Q_STATE)
		objCls = &objCls->baseType.GetType();

	// extract non-state class name
	auto clsname = objCls->className;

	auto &prog = stk.GetProgram();

	// convert to local
	auto iter = prog.stateToLocalNameMap.Find(sname);

	if (iter == prog.stateToLocalNameMap.End())
		return;

	// and look up
	auto key = CompiledProgram::PackNames(clsname, iter->value);
	auto &smap = prog.fixupStateMap;

	auto it = smap.Find(key);

	res = it == smap.End() ? sname : it->value;
}

void natCallstack(Stack &stk)
{
	String res;
	auto cstk = stk.GetContext().GetCallStack();

	for (auto &&i : cstk)
	{
		res += i;
		res += '\n';
	}

	stk.SetString(0, res);
}

}

// NativeHelpers
Int NativeHelpers::ArrayInterface(ScriptContext &ctx, const DataType &dt, ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam)
{
	Int res = 0;
	auto &arr = *static_cast<DynamicArray *>(aptr);

	switch(cmd)
	{
	case ACMD_GET_SIZE:
		res = arr.size;
		break;

	case ACMD_GET_DATA:
		*static_cast<Byte **>(pparam) = arr.data;
		break;

	case ACMD_GET_ELEM:
		// copy element to dst
		arr.CopyObjectRange(ctx, dt, static_cast<Byte *>(pparam), &arr.data[iparam*dt.size], 1);
		break;

	case ACMD_CLEAR:
		arr.Clear(ctx, dt);
		break;

	case ACMD_INSERT:
		arr.Insert(ctx, dt, pparam, iparam);
		break;

	case ACMD_ADD:
		res = arr.Push(ctx, dt, pparam);
		break;

	case ACMD_RESIZE:
		arr.Resize(ctx, dt, iparam);
		break;

	case ACMD_ERASE:
		arr.Erase(ctx, dt, iparam);
		break;

	case ACMD_ERASE_FAST:
		arr.EraseUnordered(ctx, dt, iparam);
		break;

	case ACMD_POP:
		arr.Pop(ctx, dt);
		break;
	}

	return res;
}

static void native_scriptStrLen(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<const String *>(stk.GetThis());

	Int res = 0;

	if (sptr)
		res = sptr->GetLength();

	args.Get<Int>() = res;
}

static void native_scriptStrFind(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<const String *>(stk.GetThis());

	Int res = -1;

	auto what = args.Get<String>();
	auto pos = args.Get<Int>();

	if (sptr)
		res = sptr->Find(what, pos);

	args.Get<Int>() = res;
}

static void native_scriptStrInsert(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<String *>(stk.GetThis());

	Int res = -1;

	auto pos = args.Get<Int>();
	auto what = args.Get<String>();

	if (sptr)
		res = sptr->Insert(pos, what);

	args.Get<Int>() = res;
}

static void native_scriptStrStartsWith(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<const String *>(stk.GetThis());

	bool res = false;

	auto what = args.Get<String>();

	if (sptr)
		res = sptr->StartsWith(what);

	args.Get<bool>() = res;
}

static void native_scriptStrEndsWith(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<const String *>(stk.GetThis());

	bool res = false;

	auto what = args.Get<String>();

	if (sptr)
		res = sptr->EndsWith(what);

	args.Get<bool>() = res;
}

static void native_scriptStrReplace(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<String *>(stk.GetThis());

	bool res = false;

	auto what = args.Get<String>();
	auto rwith = args.Get<String>();

	if (sptr)
		res = sptr->Replace(what, rwith);

	args.Get<bool>() = res;
}

static void native_scriptStrErase(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<String *>(stk.GetThis());

	Int res = sptr ? sptr->GetLength() : 0;

	auto pos = args.Get<Int>();
	auto count = args.Get<Int>();

	if (sptr)
		res = sptr->Erase(pos, count);

	args.Get<Int>() = res;
}

static void native_scriptStrSlice(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<String *>(stk.GetThis());

	auto from = args.Get<Int>();
	auto to = args.Get<Int>();

	ArrayRef<const char> aref;

	if (sptr)
	{
		auto len = sptr->GetLength();

		if (to < 0)
			to = len;

		from = Clamp<Int>(from, 0, len);
		to = Clamp<Int>(to, 0, len);

		if (from <= to)
			aref.Init(sptr->Ansi() + from, to - from);
	}

	args.Get<ArrayRef<const char>>() = aref;
}

static void native_scriptStrTrim(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<String *>(stk.GetThis());

	if (sptr)
		sptr->Trim();
}

static void native_scriptStrSplit(Stack &stk)
{
	ArgParserMethod args(stk);
	auto sptr = static_cast<const String *>(stk.GetThis());

	auto chset = args.Get<String>();
	auto keepEmpty = args.Get<bool>();

	Array<String> res;

	if (sptr)
		res = sptr->Split(chset, keepEmpty);

	args.Get<Array<String>>() = res;
}

static void native_scriptStrToUpper(Stack &stk)
{
	auto sptr = static_cast<String *>(stk.GetThis());

	if (sptr)
		sptr->ToUpper();
}

static void native_scriptStrToLower(Stack &stk)
{
	auto sptr = static_cast<String *>(stk.GetThis());

	if (sptr)
		sptr->ToLower();
}

static void native_decodeUtf8(Stack &stk)
{
	auto *sw = static_cast<ArrayRef<const Byte> *>(stk.GetPtr(0));

	auto *beg = sw->GetData();
	auto *end = beg + sw->GetSize();
	auto ch = CharConv::DecodeUTF8(beg, end);
	*sw = sw->Slice((Int)(IntPtr)(beg-sw->GetData()));

	stk.SetInt(1, ch);
}

static void objClassNameFromDelegate(Stack &stk)
{
	ArgParser ap(stk);
	auto sd = ap.Get<ScriptDelegate>();
	auto &res = ap.Get<Name>();
	res = Name();

	if (!sd.instancePtr)
		return;

	const auto *dt = static_cast<ScriptBaseObject *>(sd.instancePtr)->GetScriptClassType();

	if (dt)
		res = dt->className;
}

static void natHashFloat(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<Float>();
	auto &res = ap.Get<UInt>();

	// get rid of negative zeroes
	if (!value)
		value = 0;

	res = Hash(FloatToBinary(value));
}

static void natHashDouble(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<Double>();
	auto &res = ap.Get<UInt>();

	// get rid of negative zeroes
	if (!value)
		value = 0;

	res = Hash(DoubleToBinary(value));
}

static void natFloorFloat(Stack &stk)
{
	ArgParser ap(stk);
	auto value = ap.Get<Float>();
	ap.Get<Float>() = floorf(value);
}

static void natCeilFloat(Stack &stk)
{
	ArgParser ap(stk);
	auto value = ap.Get<Float>();
	ap.Get<Float>() = ceilf(value);
}

static void natFloorDouble(Stack &stk)
{
	ArgParser ap(stk);
	auto value = ap.Get<Double>();
	ap.Get<Double>() = floor(value);
}

static void natCeilDouble(Stack &stk)
{
	ArgParser ap(stk);
	auto value = ap.Get<Double>();
	ap.Get<Double>() = ceil(value);
}

static void natHashName(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<Name>();
	auto &res = ap.Get<UInt>();

	// make sure we return a stable hash here because of serialization!
	res = StableHash(value);
}

static void natHashString(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<String>();
	auto &res = ap.Get<UInt>();

	res = Hash(value);
}

static void natFloatToBinary(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<Float>();
	auto &res = ap.Get<UInt>();

	res = FloatToBinary(value);
}

static void natFloatFromBinary(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<UInt>();
	auto &res = ap.Get<Float>();

	res = BinaryToFloat(value);
}

static void natDoubleToBinary(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<Double>();
	auto &res = ap.Get<ULong>();

	res = DoubleToBinary(value);
}

static void natDoubleFromBinary(Stack &stk)
{
	ArgParser ap(stk);

	auto value = ap.Get<ULong>();
	auto &res = ap.Get<Double>();

	res = BinaryToDouble(value);
}

void NativeHelpers::Init(CompiledProgram &p)
{
	p.cpool.BindNativeFunc("decode_utf8", native_decodeUtf8);

	p.cpool.BindNativeFunc("__strlen", native_scriptStrLen);
	p.cpool.BindNativeFunc("__str_trim", native_scriptStrTrim);
	p.cpool.BindNativeFunc("__str_find", native_scriptStrFind);
	p.cpool.BindNativeFunc("__str_insert", native_scriptStrInsert);
	p.cpool.BindNativeFunc("__str_starts_with", native_scriptStrStartsWith);
	p.cpool.BindNativeFunc("__str_ends_with", native_scriptStrEndsWith);
	p.cpool.BindNativeFunc("__str_replace", native_scriptStrReplace);
	p.cpool.BindNativeFunc("__str_erase", native_scriptStrErase);
	p.cpool.BindNativeFunc("__str_slice", native_scriptStrSlice);
	p.cpool.BindNativeFunc("__str_split", native_scriptStrSplit);
	p.cpool.BindNativeFunc("__str_toupper", native_scriptStrToUpper);
	p.cpool.BindNativeFunc("__str_tolower", native_scriptStrToLower);

	p.cpool.BindNativeFunc("__da_resize", daResize);
	p.cpool.BindNativeFunc("__da_reserve", daReserve);
	p.cpool.BindNativeFunc("__da_push", daPush);
	p.cpool.BindNativeFunc("__da_push_unique", daPushUnique);
	p.cpool.BindNativeFunc("__da_pop", daPop);
	p.cpool.BindNativeFunc("__da_clear", daClear);
	p.cpool.BindNativeFunc("__da_shrink", daShrink);
	p.cpool.BindNativeFunc("__da_reset", daReset);
	p.cpool.BindNativeFunc("__da_insert", daInsert);
	p.cpool.BindNativeFunc("__da_erase", daErase);
	p.cpool.BindNativeFunc("__da_erase_unordered", daEraseUnordered);
	p.cpool.BindNativeFunc("__da_reverse", daReverse);
	p.cpool.BindNativeFunc("__da_find", daFind);
	p.cpool.BindNativeFunc("__da_sort", daSort);
	p.cpool.BindNativeFunc("__da_lower_bound", daLowerBound);
	p.cpool.BindNativeFunc("__da_upper_bound", daUpperBound);
	p.cpool.BindNativeFunc("__da_find_sorted", daFindSorted);
	p.cpool.BindNativeFunc("__da_insert_sorted", daInsertSorted);
	p.cpool.BindNativeFunc("__da_insert_sorted_unique", daInsertSortedUnique);
	p.cpool.BindNativeFunc("__da_push_heap", daPushHeap);
	p.cpool.BindNativeFunc("__da_pop_heap", daPopHeap);
	p.cpool.BindNativeFunc("__da_slice", daSlice);

	// special methods:
	p.cpool.BindNativeFunc("__da_assign", daAssign);
	// special builtins:
	p.cpool.BindNativeFunc("__da_fromaref", daFromAref);

	// object extensions:
	p.cpool.BindNativeFunc("object::vtable", objSetVtable);
	p.cpool.BindNativeFunc("object::class_name", objGetClassName);
	p.cpool.BindNativeFunc("object::nonstate_class_name", objGetNonStateClassName);
	p.cpool.BindNativeFunc("object::fix_state_name", objFixStateName);
	p.cpool.BindNativeFunc("object::class_name_from_delegate", objClassNameFromDelegate);

	// callstack:
	p.cpool.BindNativeFunc("callstack", natCallstack);

	// hashers:
	p.cpool.BindNativeFunc("__float::hash", natHashFloat);
	p.cpool.BindNativeFunc("__double::hash", natHashDouble);
	p.cpool.BindNativeFunc("__name::hash", natHashName);
	p.cpool.BindNativeFunc("__string::hash", natHashString);

	// floor/ceil:
	p.cpool.BindNativeFunc("__float::floor", natFloorFloat);
	p.cpool.BindNativeFunc("__float::ceil", natCeilFloat);
	p.cpool.BindNativeFunc("__double::floor", natFloorDouble);
	p.cpool.BindNativeFunc("__double::ceil", natCeilDouble);

	// float bin conversion:
	p.cpool.BindNativeFunc("float_from_binary", natFloatFromBinary);
	p.cpool.BindNativeFunc("float_to_binary", natFloatToBinary);
	p.cpool.BindNativeFunc("double_from_binary", natDoubleFromBinary);
	p.cpool.BindNativeFunc("double_to_binary", natDoubleToBinary);
}


}
