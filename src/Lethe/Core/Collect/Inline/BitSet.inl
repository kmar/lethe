template< typename T, typename S, typename A >
inline BitSetBase<T,S,A>::BitSetBase() : size(0) {}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A>::BitSetBase(S initialSize) : size(0)
{
	Resize(initialSize);
}

template< typename T, typename S, typename A >
inline S BitSetBase<T,S,A>::GetSize() const
{
	return size;
}

template< typename T, typename S, typename A >
inline bool BitSetBase<T,S,A>::TestBit(S bit) const
{
	LETHE_ASSERT(bit >= 0 && bit < size);
	return (data[bit / BIT_DIV] & ((T)1 << (bit & BIT_MASK))) != 0;
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::SetBit(S bit)
{
	LETHE_ASSERT(bit >= 0 && bit < size);
	data[bit / BIT_DIV] |= ((T)1 << (bit & BIT_MASK));
	return *this;
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::SetBit(S bit, bool value)
{
	LETHE_ASSERT(bit >= 0 && bit < size);

	if (value)
		data[bit / BIT_DIV] |= ((T)1 << (bit & BIT_MASK));
	else
		data[bit / BIT_DIV] &= ~((T)1 << (bit & BIT_MASK));

	return *this;
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::ClearBit(S bit)
{
	LETHE_ASSERT(bit >= 0 && bit < size);
	data[bit / BIT_DIV] &= ~((T)1 << (bit & BIT_MASK));
	return *this;
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::ResetBit(S bit)
{
	return ClearBit(bit);
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::FlipBit(S bit)
{
	LETHE_ASSERT(bit >= 0 && bit < size);
	data[bit / BIT_DIV] ^= ((T)1 << (bit & BIT_MASK));
	return *this;
}

template< typename T, typename S, typename A >
bool BitSetBase<T,S,A>::TestAnd(const BitSetBase &o)
{
	S sz = Min(data.GetSize(), o.data.GetSize());

	for (S i=0; i<sz; i++)
	{
		if (data[i] & o.data[i])
			return 1;
	}

	return 0;
}

template< typename T, typename S, typename A >
bool BitSetBase<T,S,A>::TestNAnd(const BitSetBase &o)
{
	for (S i=0; i<data.GetSize(); i++)
	{
		T tmp = i < o.GetSize() ? o.data[i] : (T)0;

		if (data[i] & ~tmp)
			return 1;
	}

	return 0;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::Resize(S newSizeBits)
{
	LETHE_ASSERT(newSizeBits >= 0);
	S newSize = (newSizeBits + BIT_DIV - 1) / BIT_DIV;
	S oldSize = data.GetSize();
	data.Resize(newSize);

	// clear if growing
	while (oldSize < newSize)
		data[oldSize++] = 0;

	size = newSizeBits;
	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::Clear()
{
	data.MemSet(0);
	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::Reset()
{
	data.Reset();
	size = 0;
	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::Shrink()
{
	data.Shrink();
	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::operator &=(const BitSetBase &o)
{
	S i, sz = Min(data.GetSize(), o.data.GetSize());

	for (i=0; i<sz; i++)
		data[i] &= o.data[i];

	// clear the rest
	while (i < data.GetSize())
		data[i++] = 0;

	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::operator |=(const BitSetBase &o)
{
	S sz = Min(data.GetSize(), o.data.GetSize());

	for (S i=0; i<sz; i++)
		data[i] |= o.data[i];

	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::operator ^=(const BitSetBase &o)
{
	S sz = Min(data.GetSize(), o.data.GetSize());

	for (S i=0; i<sz; i++)
		data[i] ^= o.data[i];

	return *this;
}

template< typename T, typename S, typename A >
S BitSetBase<T,S,A>::GetPopCount() const
{
	S res = 0;

	for (S i=0; i<data.GetSize(); i++)
	{
		T tmp = data[i];

		while (tmp)
		{
			tmp ^= tmp & ((T)0 - tmp);
			res++;
		}
	}

	return res;
}

template< typename T, typename S, typename A >
inline S BitSetBase<T,S,A>::GetBitCount() const
{
	return GetPopCount();
}

template< typename T, typename S, typename A >
const BitSetBase<T,S,A> &BitSetBase<T,S,A>::ToByteArray(Array<Byte, S> &barr) const
{
	barr.Resize(data.GetSize() * sizeof(T));

	for (S i=0; i<data.GetSize(); i++)
	{
		T tmp = data[i];

		for (size_t j=0; j<sizeof(T); j++)
		{
			barr[(S)(i*sizeof(T) + j)] = (Byte)(tmp & 255);
			tmp >>= 8;
		}
	}

	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::FromByteArray(const Array<Byte, S> &barr)
{
	Resize(barr.GetSize()*8);
	Clear();

	for (S i=0; i<barr.GetSize(); i++)
		data[i/sizeof(T)] |= (T)barr[i] << 8*(i % sizeof(T));

	return *this;
}

template< typename T, typename S, typename A >
inline void BitSetBase<T,S,A>::SwapWith(BitSetBase &o)
{
	Swap(data, o.data);
	Swap(size, o.size);
}

template< typename T, typename S, typename A >
size_t BitSetBase<T,S,A>::GetMemUsage() const
{
	return data.GetMemUsage() + sizeof(*this);
}

// range
template< typename T, typename S, typename A >
bool BitSetBase<T,S,A>::TestRange(S from, S to) const
{
	if (LETHE_UNLIKELY(from >= to))
		return false;

	// fast range check
	S wfroml = from / BIT_DIV;
	S wfrom = (from + BIT_MASK)/BIT_DIV;
	S wto = to/BIT_DIV;

	auto fromMaskUp = ~(((T)1 << (from & BIT_MASK))-1);
	auto toMaskDown = ((T)1 << (to & BIT_MASK))-1;

	if (wfroml == wto)
		return (data[wfroml] & (fromMaskUp & toMaskDown)) != 0;

	for (S i=wfrom; i<wto; i++)
		if (data[i])
			return true;

	if (wfroml != wfrom && (data[wfroml] & fromMaskUp))
		return true;

	return data[wto] & toMaskDown;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::SetRange(S from, S to)
{
	if (LETHE_UNLIKELY(from >= to))
		return *this;

	// fast range check
	S wfroml = from / BIT_DIV;
	S wfrom = (from + BIT_MASK)/BIT_DIV;
	S wto = to/BIT_DIV;

	auto fromMaskUp = ~(((T)1 << (from & BIT_MASK))-1);
	auto toMaskDown = ((T)1 << (to & BIT_MASK))-1;

	if (wfroml == wto)
	{
		data[wfroml] |= fromMaskUp & toMaskDown;
		return *this;
	}

	for (S i=wfrom; i<wto; i++)
		data[i] = ~(T)0;

	if (wfroml != wfrom)
		data[wfroml] |= fromMaskUp;

	data[wto] |= toMaskDown;

	return *this;
}

template< typename T, typename S, typename A >
inline BitSetBase<T,S,A> &BitSetBase<T,S,A>::SetRange(S from, S to, bool value)
{
	return value ? SetRange(from, to) : ClearRange(from, to);
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::ClearRange(S from, S to)
{
	if (LETHE_UNLIKELY(from >= to))
		return *this;

	// fast range check
	S wfroml = from / BIT_DIV;
	S wfrom = (from + BIT_MASK)/BIT_DIV;
	S wto = to/BIT_DIV;

	auto fromMaskUp = ~(((T)1 << (from & BIT_MASK))-1);
	auto toMaskDown = ((T)1 << (to & BIT_MASK))-1;

	if (wfroml == wto)
	{
		data[wfroml] &= ~(fromMaskUp & toMaskDown);
		return *this;
	}

	for (S i=wfrom; i<wto; i++)
		data[i] = 0;

	if (wfroml != wfrom)
		data[wfroml] &= ~fromMaskUp;

	data[wto] &= ~toMaskDown;

	return *this;
}

template< typename T, typename S, typename A >
BitSetBase<T,S,A> &BitSetBase<T,S,A>::FlipRange(S from, S to)
{
	if (LETHE_UNLIKELY(from >= to))
		return *this;

	// fast range check
	S wfroml = from / BIT_DIV;
	S wfrom = (from + BIT_MASK)/BIT_DIV;
	S wto = to/BIT_DIV;

	auto fromMaskUp = ~(((T)1 << (from & BIT_MASK))-1);
	auto toMaskDown = ((T)1 << (to & BIT_MASK))-1;

	if (wfroml == wto)
	{
		data[wfroml] ^= fromMaskUp & toMaskDown;
		return *this;
	}

	for (S i=wfrom; i<wto; i++)
		data[i] ^= ~(T)0;

	if (wfroml != wfrom)
		data[wfroml] ^= fromMaskUp;

	data[wto] ^= toMaskDown;

	return *this;
}

template< typename T, typename S, typename A >
S BitSetBase<T,S,A>::Scan(S idx) const
{
	LETHE_ASSERT(idx >= 0);
	S wfrom = idx/BIT_DIV;

	while (wfrom < data.GetSize())
	{
		T tmp = data[wfrom];
		S base = wfrom * BIT_DIV;

		while (tmp)
		{
			S bit = base + Bits::GetLsb(tmp);

			if (bit >= idx)
				return bit;

			tmp &= tmp-1;
		}

		wfrom++;
	}

	return -1;
}
