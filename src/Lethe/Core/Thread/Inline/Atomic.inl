// specializations
template<> Int Atomic::Load<Int>(const AtomicInt &t);
template<> void Atomic::Store<Int>(AtomicInt &t, Int value);
template<> Int Atomic::Increment<Int>(AtomicInt &t);
template<> Int Atomic::Decrement<Int>(AtomicInt &t);
template<> Int Atomic::Add<Int>(AtomicInt &t, Int value);
template<> Int Atomic::Subtract<Int>(AtomicInt &t, Int value);
#if LETHE_ATOMIC_ADD16
template<> Short Atomic::Load<Short>(const AtomicShort &t);
template<> void Atomic::Store<Short>(AtomicShort &t, Short value);
template<> Short Atomic::Add<Short>(AtomicShort &t, Short value);
template<> Short Atomic::Subtract<Short>(AtomicShort &t, Short value);
#endif
template<> Short Atomic::Increment<Short>(AtomicShort &t);
template<> Short Atomic::Decrement<Short>(AtomicShort &t);
template<> bool Atomic::CompareAndSwap(AtomicInt &t, Int cmp, Int xch);
template<> bool Atomic::CompareAndSwap(AtomicShort &t, Short cmp, Short xch);
template<> UInt Atomic::Load<UInt>(const AtomicUInt &t);
template<> void Atomic::Store<UInt>(AtomicUInt &t, UInt value);
template<> UInt Atomic::Increment<UInt>(AtomicUInt &t);
template<> UInt Atomic::Decrement<UInt>(AtomicUInt &t);
template<> UInt Atomic::Add<UInt>(AtomicUInt &t, UInt value);
template<> UInt Atomic::Subtract<UInt>(AtomicUInt &t, UInt value);
#if LETHE_ATOMIC_ADD16
template<> UShort Atomic::Load<UShort>(const AtomicUShort &t);
template<> void Atomic::Store<UShort>(AtomicUShort &t, UShort value);
template<> UShort Atomic::Add<UShort>(AtomicUShort &t, UShort value);
template<> UShort Atomic::Subtract<UShort>(AtomicUShort &t, UShort value);
#endif
template<> UShort Atomic::Increment<UShort>(AtomicUShort &t);
template<> UShort Atomic::Decrement<UShort>(AtomicUShort &t);
template<> bool Atomic::CompareAndSwap(AtomicUInt  &t, UInt cmp, UInt xch);
template<> bool Atomic::CompareAndSwap(AtomicUShort  &t, UShort cmp, UShort xch);

template<> inline UInt Atomic::Load<UInt>(const volatile UInt &t)
{
	return (UInt)Atomic::Load(*(const volatile Int *)&t);
}

template<> inline void Atomic::Store<UInt>(volatile UInt &t, UInt value)
{
	Atomic::Store((volatile Int &)t, (Int)value);
}

template<> inline UInt Atomic::Increment<UInt>(volatile UInt &t)
{
	return (UInt)Atomic::Increment((volatile Int &)t);
}

template<> inline UInt Atomic::Decrement<UInt>(volatile UInt &t)
{
	return (UInt)Atomic::Decrement((volatile Int &)t);
}

template<> inline UInt Atomic::Add<UInt>(volatile UInt &t, UInt value)
{
	return (UInt)Atomic::Add((volatile Int &)t, (Int)value);
}

template<> inline UInt Atomic::Subtract<UInt>(volatile UInt &t, UInt value)
{
	return (UInt)Atomic::Subtract((volatile Int &)t, (Int)value);
}

#if LETHE_ATOMIC_ADD16
template<> inline UShort Atomic::Load<UShort>(const volatile UShort &t)
{
	return (UShort)Atomic::Load(*(const volatile Short *)&t);
}

template<> inline void Atomic::Store<UShort>(volatile UShort &t, UShort value)
{
	Atomic::Store((volatile Short &)t, (Short)value);
}
#endif

template<> inline UShort Atomic::Increment<UShort>(volatile UShort &t)
{
	return (UShort)Atomic::Increment((volatile Short &)t);
}

template<> inline UShort Atomic::Decrement<UShort>(volatile UShort &t)
{
	return (UShort)Atomic::Decrement((volatile Short &)t);
}

#if LETHE_ATOMIC_ADD16
template<> inline UShort Atomic::Add<UShort>(volatile UShort &t, UShort value)
{
	return (UShort)Atomic::Add((volatile Short &)t, (Short)value);
}

template<> inline UShort Atomic::Subtract<UShort>(volatile UShort &t, UShort value)
{
	return (UShort)Atomic::Subtract((volatile Short &)t, (Short)value);
}
#endif

template<> inline bool Atomic::CompareAndSwap<UInt>(volatile UInt &t, UInt cm, UInt xch)
{
	return CompareAndSwap((volatile Int &)t, (Int)cm, (Int)xch);
}

template<> inline bool Atomic::CompareAndSwap<UShort>(volatile UShort &t, UShort cm, UShort xch)
{
	return CompareAndSwap((volatile Short &)t, (Short)cm, (Short)xch);
}

// FIXME: a bit hacky but should work
template<> inline Int Atomic::Load<Int>(const volatile Int &t)
{
#if LETHE_OS_WINDOWS && !LETHE_COMPILER_NOT_MSC
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, 0);
#else
	return __atomic_load_n((volatile Int *)&t, __ATOMIC_SEQ_CST);
#endif
}

template<> inline void Atomic::Store<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS && !LETHE_COMPILER_NOT_MSC
	_InterlockedExchange((volatile long *)&t, (long)value);
#else
	__atomic_store_n((volatile Int *)&t, value, __ATOMIC_SEQ_CST);
#endif
}

#if LETHE_ATOMIC_ADD16
template<> inline Short Atomic::Load<Short>(const volatile Short &t)
{
#if LETHE_OS_WINDOWS && !LETHE_COMPILER_NOT_MSC
	return (Short)_InterlockedExchangeAdd((volatile short *)&t, 0);
#else
	return __atomic_load_n((volatile Short *)&t, __ATOMIC_SEQ_CST);
#endif
}

template<> inline void Atomic::Store<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS
	_InterlockedExchange16((volatile short *)&t, (short)value);
#else
	__atomic_store_n((volatile Short *)&t, value, __ATOMIC_SEQ_CST);
#endif
}
#endif

template<> inline Int Atomic::Increment<Int>(volatile Int &t)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedIncrement((volatile long *)&t);
#else
	return __sync_add_and_fetch((volatile Int *)&t, (Int)1);
#endif
}

template<> inline Int Atomic::Decrement<Int>(volatile Int &t)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedDecrement((volatile long *)&t);
#else
	return __sync_sub_and_fetch((volatile Int *)&t, (Int)1);
#endif
}

template<> inline Int Atomic::Add<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, (long)value) + value;
#else
	return __sync_add_and_fetch((volatile Int *)&t, value);
#endif
}

template<> inline Int Atomic::Subtract<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, (long)-value) - value;
#else
	return __sync_sub_and_fetch((volatile Int *)&t, value);
#endif
}

template<> inline Short Atomic::Increment<Short>(volatile Short &t)
{
#if LETHE_OS_WINDOWS
	return (Short)_InterlockedIncrement16((volatile short *)&t);
#else
	return __sync_add_and_fetch((volatile Short *)&t, (Short)1);
#endif
}

template<> inline Short Atomic::Decrement<Short>(volatile Short &t)
{
#if LETHE_OS_WINDOWS
	return (Short)_InterlockedDecrement16((volatile short *)&t);
#else
	return __sync_sub_and_fetch((volatile Short *)&t, (Short)1);
#endif
}

#if LETHE_ATOMIC_ADD16
template<> inline Short Atomic::Add<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && !LETHE_COMPILER_MINGW
	return (Short)((Short)_InterlockedExchangeAdd16((volatile short *)&t, (SHORT)value) + value);
#else
	return __sync_add_and_fetch((volatile Short *)&t, value);
#endif
}

template<> inline Short Atomic::Subtract<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && !LETHE_COMPILER_MINGW
	return (Short)((Short)_InterlockedExchangeAdd16((volatile short *)&t, (SHORT)-value) - value);
#else
	return __sync_sub_and_fetch((volatile Short *)&t, value);
#endif
}
#endif

template<> inline bool Atomic::CompareAndSwap<Int>(volatile Int &t, Int cm, Int xch)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedCompareExchange((volatile long *)&t, (long)xch, (long)cm) == cm;
#else
	return __sync_bool_compare_and_swap((volatile Int *)&t, cm, xch);
#endif
}

template<> inline bool Atomic::CompareAndSwap<Short>(volatile Short &t, Short cm, Short xch)
{
#if LETHE_OS_WINDOWS
	return (Int)_InterlockedCompareExchange16((volatile short *)&t, (short)xch, (short)cm) == cm;
#else
	return __sync_bool_compare_and_swap((volatile Short *)&t, cm, xch);
#endif
}
