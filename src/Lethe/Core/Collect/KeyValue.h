#pragma once

#include "../Math/Templates.h"

namespace lethe
{

template< typename K, typename V >
struct KeyValue
{
	K key;
	V value;

	inline KeyValue() {}
	inline KeyValue(const K &k, const V &v) : key(k), value(v) {}

	inline bool operator <(const KeyValue &o) const
	{
		return key < o.key;
	}
	inline bool operator ==(const KeyValue &o) const
	{
		return key == o.key;
	}
	friend inline int Compare(const KeyValue &x, const KeyValue &y)
	{
		return Compare(x.key, y.key);
	}
	friend inline int Equal(const KeyValue &x, const KeyValue &y)
	{
		return x == y;
	}
	static inline KeyValue Make(const K &k, const V &v)
	{
		return KeyValue(k, v);
	}
	friend inline UInt Hash(const KeyValue &x)
	{
		return Hash(x.key);
	}

	inline void SwapWith(KeyValue &o)
	{
		Swap(key, o.key);
		Swap(value, o.value);
	}
};

template< typename K, typename V >
struct ConstKeyValue
{
	const K key;
	V value;

	inline ConstKeyValue() {}
	inline ConstKeyValue(const ConstKeyValue &o) : key(o.key), value(o.value) {}
	inline ConstKeyValue(const K &k, const V &v) : key(k), value(v) {}

	inline bool operator <(const ConstKeyValue &o) const
	{
		return key < o.key;
	}
	inline bool operator ==(const ConstKeyValue &o) const
	{
		return key == o.key;
	}
	friend inline int Compare(const ConstKeyValue &x, const ConstKeyValue &y)
	{
		return Compare(x.key, y.key);
	}
	friend inline int Equal(const ConstKeyValue &x, const ConstKeyValue &y)
	{
		return x == y;
	}
	static inline ConstKeyValue Make(const K &k, const V &v)
	{
		return ConstKeyValue(k, v);
	}
	friend inline UInt Hash(const ConstKeyValue &x)
	{
		return Hash(x.key);
	}
private:
	ConstKeyValue &operator =(const ConstKeyValue &)
	{
		return *this;
	}
};

}
