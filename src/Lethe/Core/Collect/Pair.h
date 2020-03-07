#pragma once

#include "../Math/Templates.h"

namespace lethe
{

template< typename F, typename S >
struct Pair
{
	F first;
	S second;

	inline Pair() {}
	inline Pair(const F &f, const S &s) : first(f), second(s) {}

	inline bool operator <(const Pair &o) const
	{
		return Compare(*this, o) < 0;
	}
	inline bool operator ==(const Pair &o) const
	{
		return Equal(first, o.first) && Equal(second, o.second);
	}
	friend inline int Compare(const Pair &x, const Pair &y)
	{
		int fcmp = Compare(x.first, y.first);

		if (fcmp)
			return fcmp;

		return Compare(x.second, y.second);
	}
	friend inline int Equal(const Pair &x, const Pair &y)
	{
		return x == y;
	}
	friend inline Pair MakePair(const F &f, const F &s)
	{
		return Pair(f, s);
	}
	inline void SwapWith(Pair &o)
	{
		Swap(first, o.first);
		Swap(second, o.second);
	}
};

template< typename F, typename S >
struct ConstPair
{
	const F first;
	S second;

	inline ConstPair() {}
	inline ConstPair(const ConstPair &o) : first(o.first), second(o.second) {}
	inline ConstPair(const F &f, const S &s) : first(f), second(s) {}

	inline bool operator <(const ConstPair &o) const
	{
		return Compare(*this, o) < 0;
	}
	inline bool operator ==(const ConstPair &o) const
	{
		return Equal(first, o.first) && Equal(second, o.second);
	}
	friend inline int Compare(const ConstPair &x, const ConstPair &y)
	{
		int fcmp = Compare(x.first, y.first);

		if (fcmp)
			return fcmp;

		return Compare(x.second, y.second);
	}
	friend inline int Equal(const ConstPair &x, const ConstPair &y)
	{
		return x == y;
	}
	friend inline ConstPair MakeConstPair(const F &f, const F &s)
	{
		return ConstPair(f, s);
	}
private:
	ConstPair &operator =(const ConstPair &)
	{
		return *this;
	}
};

}
