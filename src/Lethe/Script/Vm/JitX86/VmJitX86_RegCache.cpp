#include "VmJitX86.h"

#if LETHE_JIT_X86

namespace lethe
{

// VmJitX86::RegCache

void VmJitX86::RegCache::Init(Int count, RegExpr base)
{
	Init(count, base, 0, Eax);
}

void VmJitX86::RegCache::Init(Int count, RegExpr base, Int count2, RegExpr base2)
{
	for (Int i=0; i<REG_CACHE_MAX; i++)
		index[i] = Byte(i);

	reserved = 0;

	for (Int i=0; i<count+count2; i++)
	{
		RegCacheEntry &e = cache[i];
		Int ofs = i < count ? i : i - count;
		e.reg = i < count ? base : base2;
		e.reg.base = (GprEnum)(e.reg.base + ofs);
		index[e.reg.base & 15] = Byte(i);
		e.offset = INVALID_STACK_INDEX;
		e.counter = 0;
		e.write = false;
		e.pointer = false;
		e.doublePrec = false;
	}

	trackMap.Clear();
	size = count+count2;
	mru = 0;
}

bool VmJitX86::RegCache::RegInUse(const RegExpr &nreg) const
{
	for (Int i=0; i<size; i++)
	{
		const RegCacheEntry &e = cache[i];

		if ((e.reg.base & 15) == (nreg.base & 15) && e.offset != INVALID_STACK_INDEX)
			return 1;
	}

	return 0;
}

Int VmJitX86::RegCache::FindEntry(Int offset, bool write)
{
	LETHE_ASSERT(offset != INVALID_STACK_INDEX);

	HashMap<Int, Int>::Iterator it = trackMap.Find(offset);

	if (it != trackMap.End())
	{
		offset = it->value;

		if (write)
			return -1;

	}

	for (Int i = 0; i < size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset == offset)
		{
			e.counter = ++mru;

			if (write)
				e.write = 1;

			return i;
		}
	}

	return -1;
}

RegExpr VmJitX86::RegCache::Find(Int offset, bool write)
{
	Int ei = FindEntry(offset, write);
	return ei < 0 ? RegExpr() : cache[ei].reg;
}

void VmJitX86::RegCache::Spill(Int offset, VmJitX86 &jit)
{
	DontFlush _(jit);
	LETHE_ASSERT(offset != INVALID_STACK_INDEX);

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset == offset)
		{
			SpillEntry(offset, e, jit);
			// don't erase offset yet => will be done in free!
			break;
		}
	}

	// FIXME: better
	Free(offset, jit);
}

void VmJitX86::RegCache::SpillAbove(Int offset, VmJitX86 &jit)
{
	DontFlush _(jit);
	LETHE_ASSERT(offset != INVALID_STACK_INDEX);

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset != INVALID_STACK_INDEX && e.offset >= offset)
		{
			SpillEntry(e.offset, e, jit);
			// don't erase offset yet => will be done in free!
		}
	}

	// FIXME: better
	FreeAbove(offset, jit);
}

void VmJitX86::RegCache::AdjustTrackmap(Int offset, RegCacheEntry &re, VmJitX86 &jit)
{
	auto ti = trackMap.Begin();
	bool first = true;
	Int newBase = 0;
	Int freeIdx = -1;
	RegExpr freeReg;

	// try to find free register first
	for (Int i=0; i<size; i++)
	{
		if (cache[i].offset == INVALID_STACK_INDEX)
		{
			freeIdx = i;
			freeReg = cache[i].reg;
			break;
		}
	}

	if (freeIdx < 0)
	{
		while (ti != trackMap.End())
		{
			if (ti->value == offset)
			{
				SpillEntry(ti->key, re, jit);
				ti = trackMap.Erase(ti);
			}
			else
				++ti;
		}

		return;
	}

	while (ti != trackMap.End())
	{
		if (ti->value == offset)
		{
			if (first)
			{
				RegExpr tmp;
				first = false;

				newBase = ti->key;
				auto &ce = cache[freeIdx];
				ce.write = true;
				ce.counter = ++mru;
				ce.offset = newBase;
				ce.doublePrec = ce.pointer = false;

				if (re.doublePrec)
				{
					jit.Movq(freeReg, re.reg);
					ce.doublePrec = true;
				}
				else if (re.pointer)
				{
					jit.Mov(freeReg.ToRegPtr(), re.reg);
					ce.pointer = true;
				}
				else
				{
					jit.Mov(freeReg, re.reg);
				}

				ti = trackMap.Erase(ti);
				continue;
			}

			ti->value = newBase;
			++ti;
		}
		else
			++ti;
	}
}

void VmJitX86::RegCache::SpillEntry(Int offset, RegCacheEntry &re, VmJitX86 &jit)
{
	if (re.doublePrec)
		jit.Movq(Mem32(Edi + offset * Stack::WORD_SIZE), re.reg);
	else
		jit.Mov(Mem32(Edi + offset * Stack::WORD_SIZE), re.reg);
}

RegExpr VmJitX86::RegCache::Alloc(Int offset, VmJitX86 &jit, Int flags)
{
	bool load = (flags & RA_LOAD) != 0;
	bool write = (flags & RA_WRITE) != 0;
	bool pointer = (flags & RA_PTR) != 0;
	bool isDouble = (flags & RA_DOUBLE) != 0;

	DontFlush _(jit);

	Int ei = FindEntry(offset, write);

	RegExpr res;

	if (ei >= 0)
	{
		auto &ce = cache[ei];

		if (IsX64 && ce.reg.GetSize() != MEM_XMM)
		{
			bool isPtr = ce.reg.ToRegPtr().base == ce.reg.base;

			if (isPtr != !!(flags & RA_PTR))
			{
				ce.pointer = pointer;
				auto &creg = ce.reg;

				if (flags & RA_PTR)
					creg = creg.ToRegPtr();
				else
					creg = creg.ToReg32();
			}
		}

		res = ce.reg;
		ce.counter = ++mru;
	}

	if (res.IsRegister())
	{
		if (IsX64)
		{
			LETHE_ASSERT(pointer || res.ToReg32().base == res.base);
			LETHE_ASSERT(!pointer || res.ToRegPtr().base == res.base);
		}

		if (write)
			AdjustTrackmap(offset, cache[ei], jit);

		return res;
	}

	RegExpr src;

	if (write)
	{
		// check trackmap!
		HashMap<Int, Int>::Iterator it = trackMap.Find(offset);

		if (it != trackMap.End())
		{
			src = Find(it->value, false);
			LETHE_ASSERT(src.IsRegister() || src.IsImmediate());
			trackMap.Erase(it);
		}
	}

	// okay, we need to allocate a new one...
	// find the one with max (mru-count)
	Int besti = -1;
	UInt best = 0;

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (reserved & (1 << (e.reg.base & 15)))
			continue;

		if (e.offset == INVALID_STACK_INDEX)
		{
			// always prefer to reuse free one ASAP
			besti = i;
			break;
		}

		UInt delta = mru - e.counter;

		if (besti < 0 || delta > best)
		{
			best = delta;
			besti = i;
		}
	}

	LETHE_ASSERT(besti >= 0);

	RegCacheEntry &re = cache[besti];

	if (re.offset != INVALID_STACK_INDEX)
	{
		// don't forget to spill!
		if (re.write)
			SpillEntry(re.offset, re, jit);

		// trackmap...
		AdjustTrackmap(re.offset, re, jit);
	}

	re.write = write;
	re.offset = offset;
	re.counter = ++mru;
	re.reg = pointer ? re.reg.ToRegPtr() : re.reg.ToReg32();
	re.pointer = pointer;
	re.doublePrec = isDouble;

	if (!load)
		return re.reg;

	if (src.IsRegister() && src.base != re.reg.base)
	{
		if (re.reg.GetSize() == MEM_DWORD && src.GetSize() == MEM_DWORD)
			jit.Mov(re.reg, src);
		else
		{
			if (isDouble)
				jit.Movq(re.reg, src);
			else
				jit.Mov(re.reg.ToRegPtr(), src.ToRegPtr());
		}
	}
	else
	{
		if (isDouble)
			jit.Movq(re.reg, Mem32(Edi + re.offset * Stack::WORD_SIZE));
		else
			jit.Mov(re.reg, Mem32(Edi + re.offset * Stack::WORD_SIZE));
	}

	return re.reg;
}

void VmJitX86::RegCache::SetWrite(Int offset)
{
	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset == offset)
		{
			e.write = 1;
			break;
		}
	}
}

bool VmJitX86::RegCache::Free(Int offset, VmJitX86 &jit, bool nospill)
{
	bool res = 0;

	// free trackMap src/targ
	for (HashMap<Int, Int>::Iterator it = trackMap.Begin(); it != trackMap.End();)
	{
		if (it->key == offset || it->value == offset)
		{
			Int ei = FindEntry(it->value, false);

			RegExpr reg = ei < 0 ? RegExpr() : cache[ei].reg;
			LETHE_ASSERT(reg.IsRegister());

			if (!nospill)
			{
				// spill
				DontFlush _(jit);

				if (cache[ei].doublePrec)
					jit.Movq(Mem32(Edi + it->key*Stack::WORD_SIZE), reg);
				else
					jit.Mov(Mem32(Edi + it->key*Stack::WORD_SIZE), reg);
			}

			it = trackMap.Erase(it);
			res = 1;
		}
		else
			++it;
	}

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset == offset)
		{
			e.offset = INVALID_STACK_INDEX;

			res |= e.write;
			break;
		}
	}

	return res;
}

bool VmJitX86::RegCache::FreeAbove(Int offset, VmJitX86 &jit, bool nospill)
{
	bool res = 0;

	// free trackMap src/targ
	for (HashMap<Int, Int>::Iterator it = trackMap.Begin(); it != trackMap.End();)
	{
		if (it->key >= offset || it->value >= offset)
		{
			Int ei = FindEntry(it->value, false);
			RegExpr reg = ei < 0 ? RegExpr() : cache[ei].reg;
			LETHE_ASSERT(reg.IsRegister());

			if (!nospill)
			{
				// spill
				DontFlush _(jit);
				if (cache[ei].doublePrec)
					jit.Movq(Mem32(Edi + it->key*Stack::WORD_SIZE), reg);
				else
					jit.Mov(Mem32(Edi + it->key*Stack::WORD_SIZE), reg);
			}

			it = trackMap.Erase(it);
			res = 1;
		}
		else
			++it;
	}

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset != INVALID_STACK_INDEX && e.offset >= offset)
		{
			e.offset = INVALID_STACK_INDEX;
			res |= e.write;
		}
	}

	return res;
}

bool VmJitX86::RegCache::WouldFlush() const
{
#if 0

	// hmm, this does worse for bubblesort (bubbletest3...) and I've no idea why? => disabled
	for (Int i = 0; i < size; i++)
	{
		const RegCacheEntry &e = cache[i];

		if (e.offset == INVALID_STACK_INDEX)
			continue;

		if (e.write)
			return 1;

		for (Int j = 0; j < trackMap.GetSize(); j++)
		{
			Int targ = trackMap.GetValue(j);

			if (targ == e.offset)
				return 1;
		}
	}

	return 0;
#else
	return 1;
#endif
}

void VmJitX86::RegCache::Flush(VmJitX86 &jit, bool soft, Int softOfs)
{
	DontFlush _(jit);

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset == INVALID_STACK_INDEX)
			continue;

		if (e.write)
			SpillEntry(e.offset, e, jit);

		for (Int j=0; j<trackMap.GetSize(); j++)
		{
			Int targ = trackMap.GetValue(j);
			Int src = trackMap.GetKey(j).key;

			if (targ == e.offset)
				SpillEntry(src, e, jit);
		}

		if (soft)
		{
			e.write = false;
			e.offset -= softOfs;
		}
		else
			e.offset = INVALID_STACK_INDEX;
	}

	if (!soft)
		trackMap.Clear();
	else
	{
		// adjust trackmap, offseting both
		StackArray<KeyValue<Int, Int>, 256> adjusted;
		adjusted.Reserve(trackMap.GetSize());

		for (Int i=0; i<trackMap.GetSize(); i++)
		{
			auto kv = trackMap.GetKey(i);
			kv.key -= softOfs;
			kv.value -= softOfs;
			adjusted.Add(kv);
		}

		trackMap.Clear();

		for (auto &&it : adjusted)
			trackMap[it.key] = it.value;
	}
}

void VmJitX86::RegCache::FlushBelow(Int offset)
{
	HashMap<Int, Int>::Iterator it = trackMap.Begin();

	while (it != trackMap.End())
	{
		if (it->key < offset || it->value < offset)
			it = trackMap.Erase(it);
		else
			++it;
	}

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &e = cache[i];

		if (e.offset < offset)
			e.offset = INVALID_STACK_INDEX;
	}
}

void VmJitX86::RegCache::SwapRegs(const RegExpr &r0, const RegExpr &r1)
{
	Int i0 = index[r0.base & 15];
	Int i1 = index[r1.base & 15];
	auto creg0 = cache[i0];
	auto creg1 = cache[i1];
	Swap(cache[i0], cache[i1]);
	auto &c0 = cache[i0];
	auto &c1 = cache[i1];
	c0.reg = creg0.reg;

	if (c0.pointer)
		c0.reg = c0.reg.ToRegPtr();
	else
		c0.reg = c0.reg.ToReg32();

	c0.pointer = creg0.pointer;

	c1.reg = creg1.reg;

	if (c1.pointer)
		c1.reg = c1.reg.ToRegPtr();
	else
		c1.reg = c1.reg.ToReg32();

	c1.pointer = creg1.pointer;
}

bool VmJitX86::RegCache::TransferRegWrite(const RegExpr &src, Int ofs, Int ignoreOfs)
{
	RegCacheEntry &e = cache[index[src.base & 15]];
	Int oldOfs = e.offset;
	LETHE_ASSERT(oldOfs != INVALID_STACK_INDEX);

	HashMap< Int, Int >::Iterator it = trackMap.Begin();

	bool ptr = false;
	// FIXME: not sure if we have to care about isDouble here?!
	bool isDouble = e.doublePrec;

	while (it != trackMap.End())
	{
		LETHE_ASSERT(it->key != oldOfs);

		// problem: what if somethings tracks ofs?! we can simply transfer the offset here!
		if (it->value == ofs)
		{
			auto ei = FindEntry(ofs, false);
			LETHE_ASSERT(ei >= 0);
			auto &ce = cache[ei];
			ptr = ce.pointer;
			isDouble = ce.doublePrec;
			ce.offset = it->key;
			ce.write = true;
			it = trackMap.Erase(it);
			continue;
		}

		// don't allow transfer if tracked; would cause potentially unnecessary spill
		if (it->value == oldOfs)
		{
			if ((it->key != ofs && it->key != ignoreOfs) || (ignoreOfs != INVALID_STACK_INDEX && e.write))
				return 0;
		}

		++it;
	}

	it = trackMap.Find(ofs);

	if (it != trackMap.End())
		trackMap.Erase(it);

	it = trackMap.Find(ignoreOfs);

	if (it != trackMap.End())
		trackMap.Erase(it);

	// if we already have our reg cached, free it!

	for (Int i=0; i<size; i++)
	{
		RegCacheEntry &re = cache[i];

		if (re.offset == ofs)
		{
			ptr = re.pointer;
			isDouble = re.doublePrec;
			re.offset = INVALID_STACK_INDEX;
			break;
		}
	}

	if (IsX64 && src.GetSize() != MEM_XMM && src.base == src.ToRegPtr().base)
	{
		e.reg = e.reg.ToRegPtr();
		ptr = true;
	}

	e.offset = ofs;
	e.write = 1;
	e.pointer = ptr;
	e.doublePrec = isDouble;
	return 1;
}


void VmJitX86::RegCache::MoveTracked(RegCache &dst, Int ofs)
{
	HashMap<Int, Int>::Iterator it;

	for (it = trackMap.Begin(); it != trackMap.End();)
	{
		if (it->value == ofs)
		{
			dst.trackMap[it->key] = ofs;
			it = trackMap.Erase(it);
		}
		else
			++it;
	}
}

void VmJitX86::RegCache::MarkAsTemp(const RegExpr &r, VmJitX86 &jit)
{
	LETHE_ASSERT(r.IsRegister());
	auto &tmp = cache[index[r.base & 15]];

	if (tmp.offset == INVALID_STACK_INDEX)
		return;

	// must actually SPILL here!
	// because trackmap can be used to create vars or push on stack
	// problem: this is severe performance regression!
	Spill(tmp.offset, jit);

/*	if (!tmp.write)
		Free(tmp.offset, jit, true);
	else
		Spill(tmp.offset, jit);*/

	tmp.offset = TEMP_STACK_INDEX;
	tmp.write = 0;
}

/*void VmJitX86::RegCache::Dump()
{
	for ( Int i=0; i<size; i++ ) {
		const RegCacheEntry &e = cache[i];
		if ( e.offset == INVALID_STACK_INDEX )
			continue;
		printf("reg %d: stk %d write:%d ", e.reg.base, e.offset, (int)e.write);
	}
	printf("\n");
	for ( Int i=0; i<trackMap.GetSize(); i++ ) {
		auto t = trackMap.GetKey(i);
		printf("track: %d => %d, ", t.key, t.value);
	}
	printf("\n");
}*/

}

#endif
