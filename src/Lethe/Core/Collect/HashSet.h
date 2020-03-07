#pragma once

#include "Array.h"
#include "KeyValue.h"

namespace lethe
{

template< typename K, typename S = Int, typename A = HAlloc<K> >
class HashSet
{
protected:
	struct Entry
	{
		K key;
		S next;							// index of next entry of same hash (=next collision)

		inline void SwapWith(Entry &o)
		{
			Swap(key, o.key);
			Swap(next, o.next);
		}
	};

	// entries
	Array< Entry, S, typename A::template Rebind<Entry>::Type > entries;
	// hash table (indices to first entry)
	Array< S, S, typename A::template Rebind<S>::Type > table;

public:
	struct ConstIterator;

	struct Iterator
	{
		friend class HashSet;
		inline Iterator();
		inline Iterator(const ConstIterator &ci);
		template< typename U > inline bool operator ==(const U &o) const;
		template< typename U > inline bool operator !=(const U &o) const;
		inline Iterator &operator ++();
		inline Iterator operator ++(int);
		inline Iterator &operator --();
		inline Iterator operator --(int);
		inline const K &operator *() const;
		inline const K *operator ->() const;
		// move to next collision with same key; for iterating multi sets
		// returns true if valid
		bool Next();
	private:
		HashSet *self;
		S index;				// entry index
	};
	struct ConstIterator
	{
		friend class HashSet;
		inline ConstIterator();
		inline ConstIterator(const Iterator &it);
		template< typename U > inline bool operator ==(const U &o) const;
		template< typename U > inline bool operator !=(const U &o) const;
		inline ConstIterator &operator ++();
		inline ConstIterator operator ++(int);
		inline ConstIterator &operator --();
		inline ConstIterator operator --(int);
		inline const K &operator *() const;
		// move to next collision with same key; for iterating multi sets
		// returns true if valid
		bool Next();
	private:
		const HashSet *self;
		S index;				// entry index
	};

	// get unordered key
	inline const K &GetKey(S index) const;

	inline bool IsEmpty() const;

	inline S GetSize() const;

	HashSet &Clear();
	HashSet &Reset();
	HashSet &Shrink();

	size_t GetMemUsage() const;

	inline HashSet &Add(const K &key);
	inline HashSet &AddMulti(const K &key);
	// alias
	inline HashSet &Insert(const K &key);
	inline HashSet &InsertMulti(const K &key);

	inline ConstIterator End() const;
	inline Iterator End();
	inline ConstIterator Begin() const;
	inline Iterator Begin();
	// range-based loop support
	inline ConstIterator begin() const;
	inline Iterator begin();
	inline ConstIterator end() const;
	inline Iterator end();

	S GetNumCollisions() const;

	template< typename KT >
	inline S FindIndex(const KT &key) const;
	template< typename KT >
	ConstIterator Find(const KT &key) const;
	template< typename KT >
	Iterator Find(const KT &key);

	// validity check
	bool IsValid() const;

	// note: doesn't work as expected for multi set => it won't point to next collision
	Iterator Erase(Iterator it);

	inline void SwapWith(HashSet &o);
protected:
	// rebuild hash
	void Rehash();
	// hash and mask to get index into table
	inline S GetHashIndex(const K &key) const;
	// unbind entry before erased
	void UnbindEntry(S h, S index);
	// universal add for multi set
	template< bool multi > void AddUniversal(const K &key);
	Iterator EraseInternal(Iterator it);
	// find next collision with same key
	void Next(S &index) const;
};

template<typename K>
class BigHashSet : public HashSet<K, Long, HAlloc<K>>
{
public:
	inline void SwapWith(BigHashSet &o)
	{
		HashSet<K,Long, HAlloc<K>>::SwapWith(o);
	}
};

#include "Inline/HashSet.inl"

}
