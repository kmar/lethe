template<typename K, typename V, typename S, typename A>
HashMap<K,V,S,A>::ConstIterator::ConstIterator() : self(0), index(-1) {}

template<typename K, typename V, typename S, typename A>
HashMap<K,V,S,A>::ConstIterator::ConstIterator(const Iterator &o) : self(o.self), index(o.index) {}

template<typename K, typename V, typename S, typename A>
inline const KeyValue<K,V> &HashMap<K,V,S,A>::ConstIterator::operator *() const
{
	LETHE_ASSERT(self);
	return self->entries[index].key;
}

template<typename K, typename V, typename S, typename A>
inline const KeyValue<K,V> *HashMap<K,V,S,A>::ConstIterator::operator->() const
{
	LETHE_ASSERT(self);
	return &self->entries[index].key;
}

template<typename K, typename V, typename S, typename A>
template< typename U > inline bool HashMap<K,V,S,A>::ConstIterator::operator ==(const U &o) const
{
	LETHE_ASSERT(self == (const void *)o.self);
	return index == o.index;
}

template<typename K, typename V, typename S, typename A>
template< typename U > inline bool HashMap<K,V,S,A>::ConstIterator::operator !=(const U &o) const
{
	return !(*this == o);
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator &HashMap<K,V,S,A>::ConstIterator::operator ++()
{
	LETHE_ASSERT(self && index >=0 && index < self->GetSize());

	if (++index >= self->GetSize())
		index = -1;

	return *this;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::ConstIterator::operator ++(int)
{
	ConstIterator tmp(*this);
	++*this;
	return tmp;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator &HashMap<K,V,S,A>::ConstIterator::operator --()
{
	LETHE_ASSERT(self && index >=0 && index < self->GetSize());
	return *this;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::ConstIterator::operator --(int)
{
	ConstIterator tmp(*this);
	--*this;
	return tmp;
}

template<typename K, typename V, typename S, typename A>
bool HashMap<K,V,S,A>::ConstIterator::Next()
{
	LETHE_ASSERT(self);
	self->Next(index);
	return index >= 0;
}

template<typename K, typename V, typename S, typename A>
HashMap<K,V,S,A>::Iterator::Iterator() : self(0), index(-1) {}

template<typename K, typename V, typename S, typename A>
inline ConstKeyValue<K,V> &HashMap<K,V,S,A>::Iterator::operator *() const
{
	LETHE_ASSERT(self);
	// FIXME: this isn't very nice but it's important as it prevents key from being modified
	union u
	{
		ConstKeyValue<K,V> *ckv;
		KeyValue<K,V> *kv;
	} u;
	u.kv = &self->entries[index].key;
	return *u.ckv;
}

template<typename K, typename V, typename S, typename A>
inline ConstKeyValue<K,V> *HashMap<K,V,S,A>::Iterator::operator->() const
{
	LETHE_ASSERT(self);
	// FIXME: this isn't very nice but it's important as it prevents key from being modified
	union u
	{
		ConstKeyValue<K,V> *ckv;
		KeyValue<K,V> *kv;
	} u;
	u.kv = &self->entries[index].key;
	return u.ckv;
}

template<typename K, typename V, typename S, typename A>
template< typename U > inline bool HashMap<K,V,S,A>::Iterator::operator ==(const U &o) const
{
	LETHE_ASSERT(self == o.self);
	return index == o.index;
}

template<typename K, typename V, typename S, typename A>
template< typename U > inline bool HashMap<K,V,S,A>::Iterator::operator !=(const U &o) const
{
	LETHE_ASSERT(self == o.self);
	return !(*this == o);
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator &HashMap<K,V,S,A>::Iterator::operator ++()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());

	if (++index >= self->GetSize())
		index = -1;

	return *this;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Iterator::operator ++(int)
{
	Iterator tmp(*this);
	++*this;
	return tmp;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator &HashMap<K,V,S,A>::Iterator::operator --()
{
	LETHE_ASSERT(self && index >= 0 && index < self->GetSize());
	index--;
	return *this;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Iterator::operator --(int)
{
	Iterator tmp(*this);
	--*this;
	return tmp;
}

template<typename K, typename V, typename S, typename A>
bool HashMap<K,V,S,A>::Iterator::Next()
{
	LETHE_ASSERT(self);
	self->Next(index);
	return index >= 0;
}

template<typename K, typename V, typename S, typename A>
typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::Begin() const
{
	ConstIterator res;
	res.self = this;
	res.index = 0 - Super::IsEmpty();
	return res;
}

template<typename K, typename V, typename S, typename A>
typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Begin()
{
	Iterator res;
	res.self = this;
	res.index = 0 - Super::IsEmpty();
	return res;
}

template<typename K, typename V, typename S, typename A>
typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::End() const
{
	ConstIterator res;
	res.self = this;
	res.index = -1;
	return res;
}

template<typename K, typename V, typename S, typename A>
typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::End()
{
	Iterator res;
	res.self = this;
	res.index = -1;
	return res;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::begin() const
{
	return Begin();
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::begin()
{
	return Begin();
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::end() const
{
	return End();
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::end()
{
	return End();
}

template<typename K, typename V, typename S, typename A>
template< typename KT >
inline S HashMap<K,V,S,A>::FindIndex(const KT &key) const
{
	return FindKey(key);
}

template<typename K, typename V, typename S, typename A>
template< typename KT >
inline typename HashMap<K,V,S,A>::ConstIterator HashMap<K,V,S,A>::Find(const KT &key) const
{
	ConstIterator res;
	res.self = this;
	res.index = FindKey(key);
	return res;
}

template<typename K, typename V, typename S, typename A>
template< typename KT >
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Find(const KT &key)
{
	Iterator res;
	res.self = this;
	res.index = FindKey(key);
	return res;
}

template<typename K, typename V, typename S, typename A>
template< typename KT >
S HashMap<K,V,S,A>::FindKey(const KT &key) const
{
	S sz = this->table.GetSize();

	if (LETHE_UNLIKELY(!sz))
		return -1;

	UInt h = Hash(key) & ((UInt)sz-1);
	S index = this->table[(S)h];

	while (index >= 0)
	{
		const auto &e = this->entries[index];

		if (e.key.key == key)
			return index;

		index = e.next;
	}

	return -1;
}

template<typename K, typename V, typename S, typename A>
template< typename KT, bool multi >
typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::FindAdd(const KT &key)
{
	typename HashMap<K,V,S,A>::Iterator res;
	res.self = this;
	UInt h = Hash(key);
	S sz = this->table.GetSize();
	typename Super::Entry e;
	e.key.key = key;
	e.next = -1;

	if (LETHE_UNLIKELY(!sz))
	{
		// just add one entry (empty table)
		this->entries.Add(e);
		this->table.Add(0);
		res.index = 0;
		return res;
	}

	LETHE_ASSERT(IsPowerOfTwo((UInt)sz));
	UInt index = h & ((UInt)sz-1);

	if (!multi)
	{
		S tmp = this->table[(S)index];

		while (tmp >= 0)
		{
			auto &he = this->entries[tmp];

			if (he.key.key == key)
			{
				// already in table
				res.index = tmp;
				return res;
			}

			tmp = he.next;
		}
	}

	e.next = this->table[(S)index];
	res.index = this->table[index] = this->entries.GetSize();
	this->entries.Add(e);

	// rehash if too big
	if (this->entries.GetSize() >= this->table.GetSize())
	{
		LETHE_ASSERT(sz*2 > sz);
		this->table.Resize(sz*2);
		this->Rehash();
	}

	LETHE_ASSERT(FindKey(key) >= 0);
	return res;
}

template<typename K, typename V, typename S, typename A>
inline HashMap<K,V,S,A> &HashMap<K,V,S,A>::Add(const K &key, const V &value)
{
	Iterator it = FindAdd<K, 0>(key);
	LETHE_ASSERT(it != End());
	it->value = value;
	return *this;
}

template<typename K, typename V, typename S, typename A>
inline HashMap<K,V,S,A> &HashMap<K,V,S,A>::AddMulti(const K &key, const V &value)
{
	Iterator it = FindAdd<K, 1>(key);
	LETHE_ASSERT(it != End());
	it->value = value;
	return *this;
}

// alias
template<typename K, typename V, typename S, typename A>
inline HashMap<K,V,S,A> &HashMap<K,V,S,A>::Insert(const K &key, const V &value)
{
	return Add(key, value);
}

template<typename K, typename V, typename S, typename A>
inline HashMap<K,V,S,A> &HashMap<K,V,S,A>::InsertMulti(const K &key, const V &value)
{
	return AddMulti(key, value);
}

template<typename K, typename V, typename S, typename A>
typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Erase(Iterator it)
{
	// EraseFast will swap entries, must reindex
	LETHE_ASSERT(it != End());
	S hi = this->GetHashIndex(this->entries[it.index].key);
	this->UnbindEntry(hi, it.index);
	S last = this->entries.GetSize()-1;

	if (LETHE_UNLIKELY(it.index == last))
	{
		this->entries.Pop();
		return End();
	}

	S nhi = this->GetHashIndex(this->entries[last].key);
	this->UnbindEntry(nhi, last);
	this->entries.EraseIndexFast(it.index);
	auto &e = this->entries[it.index];
	LETHE_ASSERT(nhi == this->GetHashIndex(e.key));
	e.next = this->table[nhi];
	this->table[nhi] = it.index;
	return it;
}

template<typename K, typename V, typename S, typename A>
inline typename HashMap<K,V,S,A>::Iterator HashMap<K,V,S,A>::Remove(Iterator it)
{
	return Erase(it);
}

template<typename K, typename V, typename S, typename A>
inline const V &HashMap<K,V,S,A>::operator [](const K &key) const
{
	return Find(key)->value;
}

template<typename K, typename V, typename S, typename A>
inline V &HashMap<K,V,S,A>::operator [](const K &key)
{
	return FindAdd<K, 0>(key)->value;
}

template<typename K, typename V, typename S, typename A>
inline void HashMap<K,V,S,A>::SwapWith(HashMap &o)
{
	HashSet<KeyValue<K,V>,S>::SwapWith(o);
}

template<typename K, typename V, typename S, typename A>
inline const V &HashMap<K,V,S,A>::GetValue(S index) const
{
	return this->GetKey(index).value;
}

template<typename K, typename V, typename S, typename A>
inline V &HashMap<K,V,S,A>::GetValue(S index)
{
	return const_cast<V &>(this->GetKey(index).value);
}

template<typename K, typename V, typename S, typename A>
inline S HashMap<K,V,S,A>::GetIndexForValue(const V *pv) const
{
	LETHE_ASSERT(!this->entries.IsEmpty());
	UIntPtr d = ((UIntPtr)pv - (UIntPtr)&this->entries[0].key.value) / sizeof(this->entries[0]);
	LETHE_ASSERT(d < (UIntPtr)this->entries.GetSize());
	return S(d);
}
