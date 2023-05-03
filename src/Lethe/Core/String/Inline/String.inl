inline void StringData::AddRef()
{
	Atomic::Increment(refCount);
}

inline void StringData::Release()
{
	if (!Atomic::Decrement(refCount))
	{
		// for safety, we clear string data (just in case it'd hold password text etc.)
		MemSet(this, 0, sizeof(StringData) + capacity*sizeof(char));
		StringAllocator.CallFree(this);
	}
}

inline int String::Compare(const char *str, const char *strEnd) const
{
	return Comp(str, strEnd);
}

inline int String::Compare(const String &str) const
{
	return Comp(str);
}

inline int String::CompareNoCase(const char *str, const char *strEnd) const
{
	return CompNC(str, strEnd);
}

inline int String::CompareNoCase(const String &str) const
{
	return CompNC(str);
}

inline bool String::CharIterator::operator ==(const CharIterator &o) const
{
	return ptr == o.ptr;
}

inline bool String::CharIterator::operator !=(const CharIterator &o) const
{
	return ptr != o.ptr;
}

inline String::CharIterator String::CharIterator::operator ++(int)
{
	CharIterator tmp(*this);
	return ++tmp;
}
