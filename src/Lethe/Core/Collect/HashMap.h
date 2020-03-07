#pragma once

#include "HashSet.h"
#include "KeyValue.h"

namespace lethe
{

// S MUST be signed type
template< typename K, typename V, typename S = Int, typename A = HAlloc<KeyValue<K,V>> >
class HashMap : public HashSet< KeyValue<K,V>, S, A >
{
	typedef HashSet< KeyValue<K,V>, S, A > Super;
public:
	struct Iterator;

	struct ConstIterator
	{
		friend class HashMap;
		friend struct Iterator;
		ConstIterator();
		ConstIterator(const Iterator &o);

		inline const KeyValue<K,V> &operator *() const;
		inline const KeyValue<K,V> *operator->() const;

		template< typename U > inline bool operator ==(const U &o) const;
		template< typename U > inline bool operator !=(const U &o) const;

		inline ConstIterator &operator ++();
		inline ConstIterator operator ++(int);
		inline ConstIterator &operator --();
		inline ConstIterator operator --(int);
		// move to next collision with same key; for iterating multi maps
		// returns true if valid
		bool Next();
	private:
		const HashMap *self;
		S index;
	};

	struct Iterator
	{
		friend class HashMap;
		Iterator();

		inline ConstKeyValue<K,V> &operator *() const;
		inline ConstKeyValue<K,V> *operator->() const;

		template< typename U > inline bool operator ==(const U &o) const;
		template< typename U > inline bool operator !=(const U &o) const;

		inline Iterator &operator ++();
		inline Iterator operator ++(int);
		inline Iterator &operator --();
		inline Iterator operator --(int);
		// move to next collision with same key; for iterating multi maps
		// returns true if valid
		bool Next();
	private:
		HashMap *self;
		S index;
	};

	ConstIterator Begin() const;
	Iterator Begin();
	ConstIterator End() const;
	Iterator End();

	// this allows for range looping:
	inline ConstIterator begin() const;
	inline Iterator begin();
	inline ConstIterator end() const;
	inline Iterator end();

	template< typename KT >
	inline S FindIndex(const KT &key) const;

	// get value at index
	inline const V &GetValue(S index) const;
	inline V &GetValue(S index);
	// get index for value pointer
	inline S GetIndexForValue(const V *pv) const;

	template< typename KT >
	inline ConstIterator Find(const KT &key) const;
	template< typename KT >
	inline Iterator Find(const KT &key);

	inline HashMap &Add(const K &key, const V &value);
	inline HashMap &AddMulti(const K &key, const V &value);
	// alias
	inline HashMap &Insert(const K &key, const V &value);
	inline HashMap &InsertMulti(const K &key, const V &value);

	Iterator Erase(Iterator it);
	// alias
	inline Iterator Remove(Iterator it);

	inline const V &operator [](const K &key) const;
	inline V &operator [](const K &key);

	inline void SwapWith(HashMap &o);
private:
	template< typename KT >
	S FindKey(const KT &k) const;
	template< typename KT, bool multi >
	Iterator FindAdd(const KT &k);
};

template< typename K, typename V >
class BigHashMap : public HashMap< K,V,Long >
{
public:
	inline void SwapWith(BigHashMap &o)
	{
		HashMap<K,V,Long>::SwapWith(o);
	}
};

#include "Inline/HashMap.inl"

}
