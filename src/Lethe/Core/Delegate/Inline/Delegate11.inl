// C++11 version using variadic template arguments
template< typename R, typename... P >
class DelegateVA : public DelegateBase
{
	typedef R (DummyClass::*PMem)(P...);	// pointer to member function
	typedef R (*PFun)(P...);				// pointer to static function

	inline R LETHE_VISIBLE DummyFunc(P...) const
	{
		return R();
	}

	struct FuncWrapper : public LambdaStorageBase
	{
		FuncWrapper(PFun nfref) : fref(nfref)
		{
			AddRef();
		}
		LambdaStorageBase *Clone() override
		{
			AddRef();
			return this;
		}
		inline R LETHE_VISIBLE Invoke(P... p) const
		{
			return fref(p...);
		}
		PFun fref;
	};
	template< typename X > struct LambdaWrapper : public LambdaStorageBase
	{
		LambdaWrapper(const X &lref) : lcopy(lref)
		{
			AddRef();
		}
		LambdaStorageBase *Clone() override
		{
			AddRef();
			return this;
		}
		inline R LETHE_VISIBLE Invoke(P... p) const
		{
			return lcopy(p...);
		}
		X lcopy;
	};
public:

	DelegateVA()
	{
		Set(0);
	}

	template< typename X > DelegateVA(const X &lam)
	{
		Set(lam);
	}

	DelegateVA(PFun fun)
	{
		Set(fun);
	}

	template< typename X > DelegateVA(const X *ptr, R (X::*cpmem)(P...) const)
	{
		Set(ptr, cpmem);
	}

	template< typename X > DelegateVA(const X *ptr, R (X::*cpmem)(P...))
	{
		Set(ptr, cpmem);
	}

	inline DelegateVA &Set(int none)
	{
		(void)none;
		LETHE_ASSERT(none == 0);
		Set(this, &DelegateVA::DummyFunc);
		lambdaStorage = nullptr;
		return *this;
	}

	template< typename X > LETHE_NOINLINE DelegateVA &Set(const X *ptr, R (X::*cpmem)(P...))
	{
		LETHE_ASSERT(ptr);

		if (!ptr)
			return Set(0);

		FreeStorage();
		classptr = ptr;
		// avoid uninitialized memory access (this is required for relational operators too)
		MemSet(&pmem, 0, sizeof(pmem));
		// hack but works
		// note that sizeof cpmem can be <= sizeof pmem!
		LETHE_ASSERT(sizeof(pmem) >= sizeof(cpmem));
		MemCpy(&pmem, &cpmem, sizeof(cpmem));
		lambdaStorage = reinterpret_cast<LambdaStorageBase *>(this);		// not empty
		return *this;
	}

	template< typename X > LETHE_NOINLINE DelegateVA &Set(const X *ptr, R (X::*cpmem)(P...) const)
	{
		LETHE_ASSERT(ptr);

		if (!ptr)
			return Set(0);

		FreeStorage();
		classptr = ptr;
		// avoid uninitialized memory access (this is required for relational operators too)
		MemSet(&pmem, 0, sizeof(pmem));
		// hack but works
		// note that sizeof cpmem can be <= sizeof pmem!
		MemCpy(&pmem, &cpmem, sizeof(cpmem));
		lambdaStorage = reinterpret_cast<LambdaStorageBase *>(this);		// not empty
		return *this;
	}

	LETHE_NOINLINE DelegateVA &Set(PFun fun)
	{
		Set(reinterpret_cast<FuncWrapper *>(&lambdaStorage), &FuncWrapper::Invoke);
		lambdaStorage = new FuncWrapper(fun);
		classptr = lambdaStorage;
		return *this;
	}

	template< typename X > LETHE_NOINLINE DelegateVA &Set(const X &lam)
	{
		Set(reinterpret_cast<LambdaWrapper<X> *>(&lambdaStorage), &LambdaWrapper<X>::Invoke);
		lambdaStorage = new LambdaWrapper<X>(lam);
		classptr = lambdaStorage;
		return *this;
	}

	// invoke delegate
	inline R Invoke(P... p) const
	{
		LETHE_ASSERT(classptr);
		return (((DummyClass*)classptr)->*(PMem &)(pmem))(p...);
	}

	// same as invoke
	inline R operator()(P... p) const
	{
		return Invoke(p...);
	}

	DelegateVA &Clear()
	{
		return Set(0);
	}

};

template< typename F > class Delegate;

template< typename R, typename... P > class Delegate< R(P...) > : public DelegateVA< R, P... >
{
public:
	Delegate()
	{
	}

	template< typename X > Delegate(const X &lam) : DelegateVA< R, P... >(lam)
	{
	}

	Delegate(R (*cpfun)(P...)) : DelegateVA< R, P... >(cpfun)
	{
	}

	template< typename X > Delegate(const X *ptr, R (X::*cpmem)(P...) const)
		: DelegateVA< R, P... >(ptr, cpmem)
	{
	}

	template< typename X > Delegate(const X *ptr, R (X::*cpmem)(P...))
		: DelegateVA< R, P... >(ptr, cpmem)
	{
	}

	template<typename X>
	inline Delegate &operator =(const X &lam)
	{
		this->Set(lam);
		return *this;
	}

};
