template<typename K, typename S, typename A>
inline S HashSet<K, S, A>::GetHashIndex(const K &key) const
{
	S sz = table.GetSize();
	LETHE_ASSERT(sz);
	UInt h = Hash(key);
	h &= (UInt)sz-1;
	return (S)h;
}

template<typename K, typename S, typename A>
void HashSet<K, S, A>::Rehash()
{
	LETHE_ASSERT(entries.IsEmpty() || !table.IsEmpty());
	table.Fill(-1);
	UInt mask = (UInt)table.GetSize()-1;

	for (S i=0; i<entries.GetSize(); i++)
	{
		auto &e = entries[i];
		S h = (S)(Hash(e.key) & mask);
		e.next = table[h];
		table[h] = i;
	}
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A>::Iterator::Iterator() : self(0), index(-1) {}

template<typename K, typename S, typename A>
inline HashSet<K, S, A>::Iterator::Iterator(const ConstIterator &ci)
{
	self = const_cast<HashSet *>(ci.self);
	index = ci.index;
}

template<typename K, typename S, typename A>
template< typename U >
inline bool HashSet<K, S, A>::Iterator::operator ==(const U &o) const
{
	LETHE_ASSERT(self == o.self);
	return index == o.index;
}

template<typename K, typename S, typename A>
template< typename U >
inline bool HashSet<K, S, A>::Iterator::operator !=(const U &o) const
{
	LETHE_ASSERT(self == o.self);
	return index != o.index;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator &HashSet<K, S, A>::Iterator::operator ++()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());

	if (++index >= self->GetSize())
		index = -1;

	return *this;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::Iterator::operator ++(int)
{
	Iterator tmp(*this);
	++*this;
	return tmp;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator &HashSet<K, S, A>::Iterator::operator --()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());
	index--;
	return *this;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::Iterator::operator --(int)
{
	Iterator tmp(*this);
	--*this;
	return tmp;
}

template<typename K, typename S, typename A>
inline const K &HashSet<K, S, A>::Iterator::operator *() const
{
	LETHE_ASSERT(self);
	return self->entries[index].key;
}

template<typename K, typename S, typename A>
inline const K *HashSet<K, S, A>::Iterator::operator ->() const
{
	LETHE_ASSERT(self);
	return &self->entries[index].key;
}

template<typename K, typename S, typename A>
bool HashSet<K, S, A>::Iterator::Next()
{
	LETHE_ASSERT(self);
	self->Next(index);
	return index >= 0;
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A>::ConstIterator::ConstIterator() : self(0), index(-1) {}

template<typename K, typename S, typename A>
inline HashSet<K, S, A>::ConstIterator::ConstIterator(const Iterator &it) : self(it.self), index(it.index) {}

template<typename K, typename S, typename A>
template< typename U >
inline bool HashSet<K, S, A>::ConstIterator::operator ==(const U &o) const
{
	LETHE_ASSERT(self == o.self);
	return index == o.index;
}

template<typename K, typename S, typename A>
template< typename U >
inline bool HashSet<K, S, A>::ConstIterator::operator !=(const U &o) const
{
	return !(*this == o);
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator &HashSet<K, S, A>::ConstIterator::operator ++()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());

	if (++index >= self->GetSize())
		index = -1;

	return *this;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::ConstIterator::operator ++(int)
{
	Iterator tmp(*this);
	++*this;
	return tmp;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator &HashSet<K, S, A>::ConstIterator::operator --()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());
	index--;
	return *this;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::ConstIterator::operator --(int)
{
	ConstIterator tmp(*this);
	--*this;
	return tmp;
}

template<typename K, typename S, typename A>
const K &HashSet<K, S, A>::ConstIterator::operator *() const
{
	LETHE_ASSERT(self);
	return self->entries[index].key;
}

template<typename K, typename S, typename A>
bool HashSet<K, S, A>::ConstIterator::Next()
{
	LETHE_ASSERT(self);
	self->Next(index);
	return index >= 0;
}

template<typename K, typename S, typename A>
inline const K &HashSet<K, S, A>::GetKey(S index) const
{
	return entries[index].key;
}

template<typename K, typename S, typename A>
template< typename KT >
inline S HashSet<K, S, A>::FindIndex(const KT &key) const
{
	return Find(key).index;
}

template<typename K, typename S, typename A>
inline bool HashSet<K, S, A>::IsEmpty() const
{
	return entries.IsEmpty();
}

template<typename K, typename S, typename A>
inline S HashSet<K, S, A>::GetSize() const
{
	return entries.GetSize();
}

template<typename K, typename S, typename A>
HashSet<K, S, A> &HashSet<K, S, A>::Clear()
{
	entries.Clear();
	table.Clear();
	return *this;
}

template<typename K, typename S, typename A>
HashSet<K, S, A> &HashSet<K, S, A>::Reset()
{
	entries.Reset();
	table.Reset();
	return *this;
}

template<typename K, typename S, typename A>
HashSet<K, S, A> &HashSet<K, S, A>::Shrink()
{
	entries.Shrink();

	while (!table.IsEmpty() && table.GetSize() >= entries.GetSize()*2)
		table.Resize(table.GetSize()/2);

	table.Shrink();
	Rehash();
	return *this;
}

template<typename K, typename S, typename A>
size_t HashSet<K, S, A>::GetMemUsage() const
{
	return entries.GetMemUsage() + table.GetMemUsage() + sizeof(*this);
}

template<typename K, typename S, typename A>
template< bool multi >
void HashSet<K, S, A>::AddUniversal(const K &key)
{
	UInt h = Hash(key);
	S sz = table.GetSize();
	Entry e;
	e.key = key;
	e.next = -1;

	if (LETHE_UNLIKELY(!sz))
	{
		// just add one entry (empty table)
		entries.Add(e);
		table.Add(0);
		return;
	}

	LETHE_ASSERT(IsPowerOfTwo((UInt)sz));
	UInt index = h & ((UInt)sz-1);

	if (!multi)
	{
		S tmp = table[(S)index];

		while (tmp >= 0)
		{
			auto &he = entries[tmp];

			if (he.key == key)
			{
				// already in table
				he.key = key;		// because of HashMap
				return;
			}

			tmp = he.next;
		}
	}

	e.next = table[(S)index];
	table[index] = entries.GetSize();
	entries.Add(e);

	// rehash if too big
	if (entries.GetSize() > table.GetSize())
	{
		LETHE_ASSERT(sz*2 > sz);
		table.Resize(sz*2);
		Rehash();
	}

	LETHE_ASSERT(Find(key) != End());
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A> &HashSet<K, S, A>::Add(const K &key)
{
	AddUniversal<0>(key);
	return *this;
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A> &HashSet<K, S, A>::AddMulti(const K &key)
{
	AddUniversal<1>(key);
	return *this;
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A> &HashSet<K, S, A>::Insert(const K &key)
{
	return Add(key);
}

template<typename K, typename S, typename A>
inline HashSet<K, S, A> &HashSet<K, S, A>::InsertMulti(const K &key)
{
	return AddMulti(key);
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::End() const
{
	ConstIterator res;
	res.self = this;
	res.index = -1;
	return res;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::End()
{
	Iterator res;
	res.self = this;
	res.index = -1;
	return res;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::Begin() const
{
	ConstIterator res;
	res.self = this;
	res.index = 0 - IsEmpty();
	return res;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::Begin()
{
	Iterator res;
	res.self = this;
	res.index = 0 - IsEmpty();
	return res;
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::end() const
{
	return End();
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::end()
{
	return End();
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::begin() const
{
	return Begin();
}

template<typename K, typename S, typename A>
inline typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::begin()
{
	return Begin();
}

template<typename K, typename S, typename A>
S HashSet<K, S, A>::GetNumCollisions() const
{
	S res = 0;

	// get total number of collisions
	for (S i=0; i<entries.GetSize(); i++)
		res += entries[i].next >= 0;

	return res;
}

template<typename K, typename S, typename A>
template< typename KT >
typename HashSet<K, S, A>::ConstIterator HashSet<K, S, A>::Find(const KT &key) const
{
	S sz = table.GetSize();

	if (LETHE_UNLIKELY(!sz))
		return End();

	UInt h = Hash(key) & ((UInt)sz-1);
	S index = table[(S)h];

	while (index >= 0)
	{
		const auto &e = entries[index];

		if (e.key == key)
		{
			ConstIterator res;
			res.self = this;
			res.index = index;
			return res;
		}

		index = e.next;
	}

	return End();
}

template<typename K, typename S, typename A>
template< typename KT >
typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::Find(const KT &key)
{
	S sz = table.GetSize();

	if (LETHE_UNLIKELY(!sz))
		return End();

	UInt h = Hash(key) & ((UInt)sz-1);
	S index = table[(S)h];

	while (index >= 0)
	{
		const auto &e = entries[index];

		if (e.key == key)
		{
			Iterator res;
			res.self = this;
			res.index = index;
			return res;
		}

		index = e.next;
	}

	return End();
}

template<typename K, typename S, typename A>
bool HashSet<K, S, A>::IsValid() const
{
	for (S i=0; i<entries.GetSize(); i++)
	{
		if (Find(entries[i].key) == End())
		{
			Find(entries[i].key);
			return 0;
		}
	}

	return 1;
}

template<typename K, typename S, typename A>
typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::Erase(Iterator it)
{
	auto res = EraseInternal(it);

	// we need to do this to clean up
	// shrinking is 2x slower than growing, so we should be fine
	auto esize = entries.GetSize() | 1;

	if (table.GetSize() > 4*esize)
	{
		table.Resize(table.GetSize() >> 1);
		Rehash();
	}

	return res;
}

template<typename K, typename S, typename A>
typename HashSet<K, S, A>::Iterator HashSet<K, S, A>::EraseInternal(Iterator it)
{
	// EraseFast will swap entries, must reindex
	LETHE_ASSERT(it != End());
	S hi = GetHashIndex(entries[it.index].key);
	UnbindEntry(hi, it.index);
	S last = entries.GetSize()-1;

	if (LETHE_UNLIKELY(it.index == last))
	{
		entries.Pop();
		return End();
	}

	S nhi = GetHashIndex(entries[last].key);
	UnbindEntry(nhi, last);
	entries.EraseIndexFast(it.index);
	auto &e = entries[it.index];
	LETHE_ASSERT(nhi == GetHashIndex(e.key));
	e.next = table[nhi];
	table[nhi] = it.index;
	return it;
}

template<typename K, typename S, typename A>
inline void HashSet<K, S, A>::SwapWith(HashSet &o)
{
	entries.SwapWith(o.entries);
	table.SwapWith(o.table);
}

template<typename K, typename S, typename A>
void HashSet<K, S, A>::UnbindEntry(S h, S index)
{
	S tmp = table[h];
	S prev = -1;

	while (tmp >= 0)
	{
		if (tmp == index)
			break;

		const auto &e = entries[tmp];
		prev = tmp;
		tmp = e.next;
	}

	LETHE_ASSERT(tmp >= 0);
	S next = entries[index].next;

	if (prev >= 0)
		entries[prev].next = next;
	else
	{
		// replace head
		LETHE_ASSERT(table[h] == index);
		table[h] = next;
	}
}

template<typename K, typename S, typename A>
void HashSet<K, S, A>::Next(S &index) const
{
	if (LETHE_UNLIKELY(index < 0))
		return;

	const auto &e = entries[index];
	S ni = e.next;

	while (ni >= 0)
	{
		if (entries[ni].key == e.key)
		{
			index = ni;
			return;
		}

		ni = entries[ni].next;
	}

	index = -1;
}
