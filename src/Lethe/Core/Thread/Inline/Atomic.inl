// atomic pointer

template<typename T>
inline T *AtomicPointer<T>::Load() const
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
#	if LETHE_CPU_X86 && !LETHE_HAS_THREAD_SANITIZER
	_ReadBarrier();
	return (T *)data;
#	else
	return (T *)_InterlockedCompareExchangePointer((void * volatile *)&data, (void *)data, (void *)data);
#	endif
#else
	return (T *)__atomic_load_n((void **)&data, __ATOMIC_SEQ_CST);
#endif
}

template<typename T>
inline void AtomicPointer<T>::Store(T *value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	_InterlockedExchangePointer((void * volatile *)&data, (void *)value);
#else
	__atomic_store_n((void **)&data, (void *)value, __ATOMIC_SEQ_CST);
#endif
}

template<typename T>
inline T *AtomicPointer<T>::Exchange(T *value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return static_cast<T *>(_InterlockedExchangePointer((void * volatile *)&data, (void *)value));
#else
	return static_cast<T *>(__atomic_exchange_n((void **)&data, (void *)value, __ATOMIC_SEQ_CST));
#endif
}

// specializations
template<> Int Atomic::Load<Int>(const AtomicInt &t);
template<> void Atomic::Store<Int>(AtomicInt &t, Int value);
template<> Int Atomic::Increment<Int>(AtomicInt &t);
template<> Int Atomic::Decrement<Int>(AtomicInt &t);
template<> Int Atomic::Add<Int>(AtomicInt &t, Int value);
template<> Int Atomic::Subtract<Int>(AtomicInt &t, Int value);
template<> Int Atomic::Or<Int>(AtomicInt &t, Int value);
template<> bool Atomic::CompareAndSwap<Int>(AtomicInt &t, Int cmp, Int xch);

#if LETHE_64BIT
// note: because of BitSet
template<> Long Atomic::Load<Long>(const AtomicLong &t);
template<> void Atomic::Store<Long>(AtomicLong &t, Long value);
template<> Long Atomic::Increment<Long>(AtomicLong &t);
template<> Long Atomic::Decrement<Long>(AtomicLong &t);
template<> Long Atomic::Add<Long>(AtomicLong &t, Long value);
template<> Long Atomic::Subtract<Long>(AtomicLong &t, Long value);
template<> Long Atomic::Or<Long>(AtomicLong &t, Long value);
template<> bool Atomic::CompareAndSwap<Long>(AtomicLong &t, Long cmp, Long xch);
#endif

template<> Short Atomic::Load<Short>(const AtomicShort &t);
template<> void Atomic::Store<Short>(AtomicShort &t, Short value);
template<> Short Atomic::Add<Short>(AtomicShort &t, Short value);
template<> Short Atomic::Subtract<Short>(AtomicShort &t, Short value);
template<> Short Atomic::Increment<Short>(AtomicShort &t);
template<> Short Atomic::Decrement<Short>(AtomicShort &t);
template<> Short Atomic::Or<Short>(AtomicShort &t, Short value);
template<> bool Atomic::CompareAndSwap<Short>(AtomicShort &t, Short cmp, Short xch);

template<> SByte Atomic::Load<SByte>(const AtomicSByte &t);
template<> void Atomic::Store<SByte>(AtomicSByte &t, SByte value);
template<> SByte Atomic::Add<SByte>(AtomicSByte &t, SByte value);
template<> SByte Atomic::Subtract<SByte>(AtomicSByte &t, SByte value);
template<> SByte Atomic::Increment<SByte>(AtomicSByte &t);
template<> SByte Atomic::Decrement<SByte>(AtomicSByte &t);
template<> SByte Atomic::Or<SByte>(AtomicSByte &t, SByte value);
template<> bool Atomic::CompareAndSwap<SByte>(AtomicSByte &t, SByte cmp, SByte xch);

template<> UInt Atomic::Load<UInt>(const AtomicUInt &t);
template<> void Atomic::Store<UInt>(AtomicUInt &t, UInt value);
template<> UInt Atomic::Increment<UInt>(AtomicUInt &t);
template<> UInt Atomic::Decrement<UInt>(AtomicUInt &t);
template<> UInt Atomic::Add<UInt>(AtomicUInt &t, UInt value);
template<> UInt Atomic::Subtract<UInt>(AtomicUInt &t, UInt value);
template<> UInt Atomic::Or<UInt>(AtomicUInt &t, UInt value);
template<> bool Atomic::CompareAndSwap<UInt>(AtomicUInt  &t, UInt cmp, UInt xch);

#if LETHE_64BIT
template<> ULong Atomic::Load<ULong>(const AtomicULong &t);
template<> void Atomic::Store<ULong>(AtomicULong &t, ULong value);
template<> ULong Atomic::Increment<ULong>(AtomicULong &t);
template<> ULong Atomic::Decrement<ULong>(AtomicULong &t);
template<> ULong Atomic::Add<ULong>(AtomicULong &t, ULong value);
template<> ULong Atomic::Subtract<ULong>(AtomicULong &t, ULong value);
template<> ULong Atomic::Or<ULong>(AtomicULong &t, ULong value);
template<> bool Atomic::CompareAndSwap<ULong>(AtomicULong  &t, ULong cmp, ULong xch);
#endif

template<> UShort Atomic::Load<UShort>(const AtomicUShort &t);
template<> void Atomic::Store<UShort>(AtomicUShort &t, UShort value);
template<> UShort Atomic::Add<UShort>(AtomicUShort &t, UShort value);
template<> UShort Atomic::Subtract<UShort>(AtomicUShort &t, UShort value);
template<> UShort Atomic::Increment<UShort>(AtomicUShort &t);
template<> UShort Atomic::Decrement<UShort>(AtomicUShort &t);
template<> UShort Atomic::Or<UShort>(AtomicUShort &t, UShort value);
template<> bool Atomic::CompareAndSwap<UShort>(AtomicUShort  &t, UShort cmp, UShort xch);

template<> Byte Atomic::Load<Byte>(const AtomicByte &t);
template<> void Atomic::Store<Byte>(AtomicByte &t, Byte value);
template<> Byte Atomic::Add<Byte>(AtomicByte &t, Byte value);
template<> Byte Atomic::Subtract<Byte>(AtomicByte &t, Byte value);
template<> Byte Atomic::Increment<Byte>(AtomicByte &t);
template<> Byte Atomic::Decrement<Byte>(AtomicByte &t);
template<> Byte Atomic::Or<Byte>(AtomicByte &t, Byte value);
template<> bool Atomic::CompareAndSwap<Byte>(AtomicByte  &t, Byte cmp, Byte xch);

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

template<> inline UInt Atomic::Or<UInt>(volatile UInt &t, UInt value)
{
	return (UInt)Atomic::Or((volatile Int &)t, (Int)value);
}

template<> inline bool Atomic::CompareAndSwap<UInt>(volatile UInt &t, UInt cm, UInt xch)
{
	return CompareAndSwap((volatile Int &)t, (Int)cm, (Int)xch);
}

#if LETHE_64BIT
template<> inline ULong Atomic::Load<ULong>(const volatile ULong &t)
{
	return (ULong)Atomic::Load(*(const volatile Long *)&t);
}

template<> inline void Atomic::Store<ULong>(volatile ULong &t, ULong value)
{
	Atomic::Store((volatile Long &)t, (Long)value);
}

template<> inline ULong Atomic::Increment<ULong>(volatile ULong &t)
{
	return (ULong)Atomic::Increment((volatile Long &)t);
}

template<> inline ULong Atomic::Decrement<ULong>(volatile ULong &t)
{
	return (ULong)Atomic::Decrement((volatile Long &)t);
}

template<> inline ULong Atomic::Add<ULong>(volatile ULong &t, ULong value)
{
	return (ULong)Atomic::Add((volatile Long &)t, (Long)value);
}

template<> inline ULong Atomic::Subtract<ULong>(volatile ULong &t, ULong value)
{
	return (ULong)Atomic::Subtract((volatile Long &)t, (Long)value);
}

template<> inline ULong Atomic::Or<ULong>(volatile ULong &t, ULong value)
{
	return (ULong)Atomic::Or((volatile Long &)t, (Long)value);
}

template<> inline bool Atomic::CompareAndSwap<ULong>(volatile ULong &t, ULong cm, ULong xch)
{
	return CompareAndSwap((volatile Long &)t, (Long)cm, (Long)xch);
}

#endif

template<> inline UShort Atomic::Load<UShort>(const volatile UShort &t)
{
	return (UShort)Atomic::Load(*(const volatile Short *)&t);
}

template<> inline void Atomic::Store<UShort>(volatile UShort &t, UShort value)
{
	Atomic::Store((volatile Short &)t, (Short)value);
}

template<> inline UShort Atomic::Increment<UShort>(volatile UShort &t)
{
	return (UShort)Atomic::Increment((volatile Short &)t);
}

template<> inline UShort Atomic::Decrement<UShort>(volatile UShort &t)
{
	return (UShort)Atomic::Decrement((volatile Short &)t);
}

template<> inline UShort Atomic::Add<UShort>(volatile UShort &t, UShort value)
{
	return (UShort)Atomic::Add((volatile Short &)t, (Short)value);
}

template<> inline UShort Atomic::Subtract<UShort>(volatile UShort &t, UShort value)
{
	return (UShort)Atomic::Subtract((volatile Short &)t, (Short)value);
}

template<> inline UShort Atomic::Or<UShort>(volatile UShort &t, UShort value)
{
	return (UShort)Atomic::Or((volatile Short &)t, (Short)value);
}

template<> inline bool Atomic::CompareAndSwap<UShort>(volatile UShort &t, UShort cm, UShort xch)
{
	return CompareAndSwap((volatile Short &)t, (Short)cm, (Short)xch);
}

template<> inline Byte Atomic::Load<Byte>(const volatile Byte &t)
{
	return (Byte)Atomic::Load(*(const volatile SByte *)&t);
}

template<> inline void Atomic::Store<Byte>(volatile Byte &t, Byte value)
{
	Atomic::Store((volatile SByte &)t, (SByte)value);
}

template<> inline Byte Atomic::Increment<Byte>(volatile Byte &t)
{
	return (Byte)Atomic::Increment((volatile SByte &)t);
}

template<> inline Byte Atomic::Decrement<Byte>(volatile Byte &t)
{
	return (Byte)Atomic::Decrement((volatile SByte &)t);
}

template<> inline Byte Atomic::Add<Byte>(volatile Byte &t, Byte value)
{
	return (Byte)Atomic::Add((volatile SByte &)t, (SByte)value);
}

template<> inline Byte Atomic::Subtract<Byte>(volatile Byte &t, Byte value)
{
	return (Byte)Atomic::Subtract((volatile SByte &)t, (SByte)value);
}

template<> inline Byte Atomic::Or<Byte>(volatile Byte &t, Byte value)
{
	return (Byte)Atomic::Or((volatile SByte &)t, (SByte)value);
}

template<> inline bool Atomic::CompareAndSwap<Byte>(volatile Byte &t, Byte cm, Byte xch)
{
	return CompareAndSwap((volatile SByte &)t, (SByte)cm, (SByte)xch);
}

// FIXME: a bit hacky but should work
template<> inline Int Atomic::Load<Int>(const volatile Int &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
#	if LETHE_CPU_X86 && !LETHE_HAS_THREAD_SANITIZER
	_ReadBarrier();
	return t;
#	else
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, 0);
#	endif
#else
	return __atomic_load_n((Int *)&t, __ATOMIC_SEQ_CST);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Load<Long>(const volatile Long &t)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
#		if LETHE_CPU_X86 && !LETHE_HAS_THREAD_SANITIZER
	_ReadBarrier();
	return t;
#		else
	return (Long)_InterlockedExchangeAdd64((volatile long long *)&t, 0);
#		endif
#	else
	return __atomic_load_n((Long *)&t, __ATOMIC_SEQ_CST);
#	endif
}
#endif

template<> inline void Atomic::Store<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	_InterlockedExchange((volatile long *)&t, (long)value);
#else
	__atomic_store_n((Int *)&t, value, __ATOMIC_SEQ_CST);
#endif
}

#if LETHE_64BIT
template<> inline void Atomic::Store<Long>(volatile Long &t, Long value)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	_InterlockedExchange64((volatile long long *)&t, (long long)value);
#	else
	__atomic_store_n((Long *)&t, value, __ATOMIC_SEQ_CST);
#	endif
}
#endif

template<> inline Short Atomic::Load<Short>(const volatile Short &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
#	if LETHE_CPU_X86 && !LETHE_HAS_THREAD_SANITIZER
	_ReadBarrier();
	return t;
#	else
	return (Short)_InterlockedExchangeAdd16((volatile short *)&t, 0);
#	endif
#else
	return __atomic_load_n((Short *)&t, __ATOMIC_SEQ_CST);
#endif
}

template<> inline void Atomic::Store<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	_InterlockedExchange16((volatile short *)&t, (short)value);
#else
	__atomic_store_n((Short *)&t, value, __ATOMIC_SEQ_CST);
#endif
}

template<> inline Int Atomic::Increment<Int>(volatile Int &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedIncrement((volatile long *)&t);
#else
	return __sync_add_and_fetch((Int *)&t, (Int)1);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Increment<Long>(volatile Long &t)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Long)_InterlockedIncrement64((volatile long long *)&t);
#	else
	return __sync_add_and_fetch((Long *)&t, (Long)1);
#	endif
}
#endif

template<> inline Int Atomic::Decrement<Int>(volatile Int &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedDecrement((volatile long *)&t);
#else
	return __sync_add_and_fetch((Int *)&t, (Int)-1);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Decrement<Long>(volatile Long &t)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Long)_InterlockedDecrement64((volatile long long *)&t);
#	else
	return __sync_add_and_fetch((Long *)&t, (Long)-1);
#	endif
}
#endif

template<> inline Int Atomic::Add<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, (long)value) + value;
#else
	return __sync_add_and_fetch((Int *)&t, value);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Add<Long>(volatile Long &t, Long value)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Long)_InterlockedExchangeAdd64((volatile long long *)&t, (long long)value) + value;
#	else
	return __sync_add_and_fetch((Long *)&t, value);
#	endif
}
#endif

template<> inline Int Atomic::Subtract<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedExchangeAdd((volatile long *)&t, (long)-value) - value;
#else
	return __sync_add_and_fetch((Int *)&t, -value);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Subtract<Long>(volatile Long &t, Long value)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Long)_InterlockedExchangeAdd64((volatile long long *)&t, (long long)-value) - value;
#	else
	return __sync_add_and_fetch((Long *)&t, -value);
#	endif
}
#endif

template<> inline Int Atomic::Or<Int>(volatile Int &t, Int value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedOr((volatile long *)&t, (long)value);
#else
	return __sync_fetch_and_or((Int *)&t, value);
#endif
}

#if LETHE_64BIT
template<> inline Long Atomic::Or<Long>(volatile Long &t, Long value)
{
#	if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (size_t)_InterlockedOr64((volatile long long *)&t, (long long)value);
#	else
	return __sync_fetch_and_or((Long *)&t, value);
#	endif
}
#endif

template<> inline Short Atomic::Increment<Short>(volatile Short &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Short)_InterlockedIncrement16((volatile short *)&t);
#else
	return __sync_add_and_fetch((Short *)&t, (Short)1);
#endif
}

template<> inline Short Atomic::Decrement<Short>(volatile Short &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Short)_InterlockedDecrement16((volatile short *)&t);
#else
	return __sync_add_and_fetch((Short *)&t, (Short)-1);
#endif
}

template<> inline Short Atomic::Add<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Short)((Short)_InterlockedExchangeAdd16((volatile short *)&t, value) + value);
#else
	return __sync_add_and_fetch((Short *)&t, (Short)value);
#endif
}

template<> inline Short Atomic::Subtract<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Short)((Short)_InterlockedExchangeAdd16((volatile short *)&t, -value) - value);
#else
	return __sync_add_and_fetch((Short *)&t, (Short)-value);
#endif
}

template<> inline Short Atomic::Or<Short>(volatile Short &t, Short value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Short)_InterlockedOr16((volatile short *)&t, (short)value);
#else
	return __sync_fetch_and_or((Short *)&t, (Short)value);
#endif
}

template<> inline SByte Atomic::Load<SByte>(const volatile SByte &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
#	if LETHE_CPU_X86 && !LETHE_HAS_THREAD_SANITIZER
	_ReadBarrier();
	return t;
#	else
	return (SByte)_InterlockedExchangeAdd8((volatile char *)&t, 0);
#	endif
#else
	return __atomic_load_n((SByte *)&t, __ATOMIC_SEQ_CST);
#endif
}

template<> inline void Atomic::Store<SByte>(volatile SByte &t, SByte value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	_InterlockedExchange8((volatile char *)&t, (char)value);
#else
	__atomic_store_n((SByte *)&t, value, __ATOMIC_SEQ_CST);
#endif
}

template<> inline SByte Atomic::Increment<SByte>(volatile SByte &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return Atomic::Add(t, (SByte)1);
#else
	return __sync_add_and_fetch((SByte *)&t, (SByte)1);
#endif
}

template<> inline SByte Atomic::Decrement<SByte>(volatile SByte &t)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return Atomic::Add(t, (SByte)-1);
#else
	return __sync_add_and_fetch((SByte *)&t, (SByte)-1);
#endif
}

template<> inline SByte Atomic::Add<SByte>(volatile SByte &t, SByte value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (SByte)((SByte)_InterlockedExchangeAdd8((volatile char *)&t, value) + value);
#else
	return __sync_add_and_fetch((SByte *)&t, value);
#endif
}

template<> inline SByte Atomic::Subtract<SByte>(volatile SByte &t, SByte value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (SByte)((SByte)_InterlockedExchangeAdd8((volatile char *)&t, -value) - value);
#else
	return __sync_add_and_fetch((SByte *)&t, (SByte)-value);
#endif
}

template<> inline SByte Atomic::Or<SByte>(volatile SByte &t, SByte value)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (SByte)_InterlockedOr8((volatile char *)&t, (char)value);
#else
	return __sync_fetch_and_or((SByte *)&t, value);
#endif
}

#if LETHE_64BIT
template<> inline bool Atomic::CompareAndSwap<Long>(volatile Long &t, Long cm, Long xch)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Long)_InterlockedCompareExchange64((volatile long long *)&t, (long long)xch, (long long)cm) == cm;
#else
	return __sync_val_compare_and_swap((Long *)&t, cm, xch) == cm;
#endif
}

#endif

template<> inline bool Atomic::CompareAndSwap<Int>(volatile Int &t, Int cm, Int xch)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedCompareExchange((volatile long *)&t, (long)xch, (long)cm) == cm;
#else
	return __sync_val_compare_and_swap((Int *)&t, cm, xch) == cm;
#endif
}

template<> inline bool Atomic::CompareAndSwap<Short>(volatile Short &t, Short cm, Short xch)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (Int)_InterlockedCompareExchange16((volatile short *)&t, (short)xch, (short)cm) == cm;
#else
	return __sync_val_compare_and_swap((Short *)&t, cm, xch) == cm;
#endif
}

template<> inline bool Atomic::CompareAndSwap<SByte>(volatile SByte &t, SByte cm, SByte xch)
{
#if LETHE_OS_WINDOWS && LETHE_COMPILER_MSC_ONLY
	return (SByte)_InterlockedCompareExchange8((volatile char *)&t, (char)xch, (char)cm) == cm;
#else
	return __sync_val_compare_and_swap((SByte *)&t, cm, xch) == cm;
#endif
}

inline void Atomic::Pause()
{
#if LETHE_CPU_X86 && LETHE_COMPILER_MSC_ONLY
	_mm_pause();
#elif LETHE_CPU_X86
	asm volatile("pause");
#elif LETHE_CPU_ARM && LETHE_COMPILER_NOT_MSC
	asm volatile("yield");
#endif
}
