#include <Lethe/Lethe.h>

#include <stdio.h>
#include <cmath>

#include <vector>

// set to true to test debug server
constexpr bool test_debug_server = false;

// ugly unsafe macro to echo output to debugger
#define xprintf(...) do {auto tmp = lethe::String::Printf(__VA_ARGS__); printf("%s", tmp.Ansi()); if (g_dsrv) g_dsrv->SendOutput(tmp);} while(false)

lethe::DebugServer *g_dsrv = nullptr;

void native_div(lethe::Stack &stk)
{
	// direct stack access can be used for non-methods
	// always use ArgParserMethod for methods
	stk.SetInt(2, stk.GetSignedInt(0) / stk.GetSignedInt(1));
}

void native_div_2(lethe::Stack &stk)
{
	// alternative using argument parser (actually this is the preferred method)
	lethe::ArgParser ap(stk);
	auto a = ap.Get<lethe::Int>();
	auto b = ap.Get<lethe::Int>();
	auto &res = ap.Get<lethe::Int>();

	res = a/b;
}

void native_printf(lethe::Stack &stk)
{
	xprintf("%s", lethe::FormatStr(stk).Ansi());
}

// this must match script struct layout
struct vec
{
	float x, y, z;
};

struct vec_natural_packing
{
	float x, y, z;
};

void native_vec_length(lethe::Stack &stk)
{
	lethe::ArgParserMethod ap(stk);

	auto &v = *static_cast<const vec *>(stk.GetThis());

	ap.Get<float>() = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

template<typename T>
struct bind_std_vector
{
	static void bind(const char *tname, lethe::ScriptEngine &eng)
	{
		auto qname = lethe::String::Printf("std::vector<%s>", tname);

		auto *uint_ptr_name = sizeof(void *) == 4 ? "uint" : "ulong";

		eng.BindNativeStruct(qname, sizeof(std::vector<T>), alignof(std::vector<T>));

		eng.BindNativeFunction(qname + "::vector",
			[](lethe::Stack &stk)
			{
				new(stk.GetThis()) std::vector<T>();
			}
		);

		eng.BindNativeFunction(qname + "::~vector",
			[](lethe::Stack &stk)
			{
				static_cast<std::vector<T> *>(stk.GetThis())->~vector();
			}
		);

		eng.BindNativeFunction(qname + "::size",
			[](lethe::Stack &stk)
			{
				lethe::ArgParserMethod ap(stk);
				ap.Get<lethe::Int>() = lethe::Int(static_cast<std::vector<T> *>(stk.GetThis())->size());
			}
		);

		eng.BindNativeFunction(qname + "::push_back",
			[](lethe::Stack &stk)
			{
				lethe::ArgParserMethod ap(stk);
				static_cast<std::vector<T> *>(stk.GetThis())->push_back(*ap.Get<const T *>());
			}
		);

		eng.BindNativeFunction(qname + "::pop_back",
			[](lethe::Stack &stk)
			{
				lethe::ArgParserMethod ap(stk);
				static_cast<std::vector<T> *>(stk.GetThis())->pop_back();
			}
		);

		auto data_lambda =
			[](lethe::Stack &stk)
			{
				lethe::ArgParserMethod ap(stk);
				auto &aref = ap.Get<lethe::ArrayRef<T>>();
				auto *vec = static_cast<std::vector<T> *>(stk.GetThis());
				aref = lethe::ArrayRef<T>(vec->data(), lethe::Int(vec->size()));
			};

		eng.BindNativeFunction(qname + "::data", data_lambda);
		eng.BindNativeFunction(qname + "::cdata", data_lambda);

		eng.BindNativeFunction(qname + "::assign",
			[](lethe::Stack &stk)
			{
				lethe::ArgParserMethod ap(stk);
				auto &aref = ap.Get<lethe::ArrayRef<T>>();
				auto *vec = static_cast<std::vector<T> *>(stk.GetThis());
				vec->assign(aref.Begin(), aref.End());
			}
		);

		eng.BindNativeFunction(qname + "::__copy",
			[](lethe::Stack &stk)
			{
				lethe::ArgParser ap(stk);
				auto *dst = ap.Get<std::vector<T> *>();
				const auto *src = ap.Get<const std::vector<T> *>();

				if (dst != src)
					*dst = *src;
			}
		);

		auto subscript_lambda =
			[](lethe::Stack &stk)
			{
				lethe::ArgParser ap(stk);
				auto *vec = ap.Get<std::vector<T> *>();
				auto idx = ap.Get<size_t>();
				ap.Get<T *>() = &(*vec)[idx];
			};

		auto sub_const = lethe::String::Printf("%s::operator[] const %s&(const %s&, %s)",
			qname.Ansi(),
			tname,
			qname.Ansi(),
			uint_ptr_name
		);

		auto sub_nonconst = lethe::String::Printf("%s::operator[] %s&(%s&, %s)",
			qname.Ansi(),
			tname,
			qname.Ansi(),
			uint_ptr_name
		);

		eng.BindNativeFunction(sub_const, subscript_lambda);
		eng.BindNativeFunction(sub_nonconst, subscript_lambda);
	}
};

class direct_native_class : public lethe::ScriptBaseObject
{
public:

	lethe::String nativeString = "native_class_string";

	virtual void test()
	{
		xprintf("direct_native_class::test()\n");
	}

	void exec_test(lethe::ScriptContext &ctx)
	{
		// if no script vtable (=native only, call directly)
		if (!scriptVtbl)
			test();
		else
		{
			// otherwise issue a script call
			// note that it's more efficient to cache and use CallMethodByIndex
			ctx.CallMethod("test", this);
		}
	}
};

void native_spawn(lethe::Stack &stk)
{
	lethe::ArgParser ap(stk);
	// this is the way to return script instances (i.e. add ref count)

	auto &ptr = ap.Get<lethe::SPtr>();
	ptr = new direct_native_class;
	// we need to script vtable this way (so that it will merge properly with script RTTI)
	ptr->scriptVtbl = (void **)stk.GetContext().GetEngine().GetClassVtable("direct_native_class");
}

void native_spawn2(lethe::Stack &stk)
{
	lethe::ArgParser ap(stk);
	// a nicer version using NewObject on context, but this involves extra script call
	// this way we can spawn any script class instance
	ap.Get<lethe::SPtr>() = stk.GetContext().NewObject("direct_native_class");
}

void native_pass_ptr(lethe::Stack &stk)
{
	lethe::ArgParser ap(stk);
	// unlike result, ass function arguments are passed as raw pointers in lethe (this is to reduce refcounting)
	// you have to explicitly hold a smart pointer on stack to force refcount
	auto *obj = ap.Get<lethe::ScriptBaseObject *>();
	xprintf("obj: %p strong ref: %u\n", (void *)obj, obj->strongRefCount);
}

void test_script_spawn(lethe::ScriptContext *ctx)
{
	ctx->GetStack().PushPtr(nullptr);
	ctx->Call("spawn_script_object");

	lethe::ArgParser ap(ctx->GetStack());
	lethe::SPtr &result = ap.Get<lethe::SPtr>();
	auto sptr = result;

	// clean up and pop result
	result.Clear();
	ctx->GetStack().Pop(1);

	// and finally execute test method
	static_cast<direct_native_class *>(sptr.Get())->exec_test(*ctx);
}

int main()
{
	lethe::Init();

	lethe::ScriptEngine engine(test_debug_server ? lethe::ENGINE_DEBUG : lethe::ENGINE_JIT);

	engine.EnableInlineExpansion(!test_debug_server);

	const char *source = R"src(

	native class direct_native_class
	{
		native string nativeString;
		native void test();
	}

	class A : direct_native_class
	{
		// default access for classes is public, unlike C++
		int x;
		int y;

		// class methods are virtual by default
		void test() override
		{
			super::test();
			"A::test() x=%d y=%d\n", x, y;
		}

		final void nonvirtual()
		{
			"A::nonvirtual()\n";
		}
	}

	class B : A
	{
		void test() override
		{
			"B::test()\n";
			super::test();
		}
	}

	class C
	{
	}

	B spawn_script_object()
	{
		return new B;
	}

	// wrapping std:: vector, sort of...

	namespace std
	{

	using size_t = uintptr;

	native nontrivial struct vector<T>
	{
		// because of almighty const west...
		using __const_T = const T;

		native vector();
		native ~vector();

		native static const T &operator [](const vector &v, size_t idx);
		native static T &operator [](vector &v, size_t idx);

		native int size() const;

		inline bool empty() const {return !size();}

		native void push_back(const T &val);
		native void pop_back();
		native void assign(const T[] aref);

		native T[] data();
		native __const_T [] cdata() const;

		// we don't have assign operator
		native static void __copy(vector &dst, const vector &src);
	}

	}

	__assert inline void assert(bool expr)
	{
		if (!expr)
		{
			"assertion failed!\n";
			0/0;
		}
	}

	native __format void printf(string fmt, ...);
	native int div(int a, int b);
	native int div2(int a, int b);

	noinit int script_div(int a, int b)
	{
		return a/b;
	}

	// structure layout must match native type, careful!
	native struct vec
	{
		native float x;
		native float y;
		native float z;

		native noinit float length() const;
	}

	// structure layout must match native type, careful!
	// this version doesn't need x, y and z explicitly specified, assuming it will match natural packing in C++ vs lethe
	// however, struct size and alignment is still required so that lethe can perform a check that packing does indeed match
	native struct vec_natural_packing
	{
		float x;
		float y;
		float z;
	}

	void __init()
	{
		"static init\n";
	}

	void __exit()
	{
		"static exit\n";
	}

	native direct_native_class spawn();
	native direct_native_class spawn2();
	native void pass_ptr(direct_native_class cls);

	struct props
	{
		constexpr int foo = cast int 45.5;
		int _x;

		int x:
		{
			inline int get() const
			{
				return 42;
			}

			// can be void to prevent assignment-chains
			inline int set(int value)
			{
				"setting x to %t\n", value;
				_x = value;
				return value;
			}
		}
	}

	struct tB<T>
	{
		T memberB;
	}

	struct tA<T> : tB<T>
	{
		T memberA;
	}

	void main()
	{
		tA<float> ta = {1.2, 3.4};
		"ta=%t\n", ta;

		"props::foo=%t\n", props::foo;
		props proptest;
		proptest.x += 55;

		object o = new object;
		array<int> iarr;
		iarr.add(42);

		// A is actually strong pointer to class A
		A a = new A;
		// the same goes for B
		B b = new B;

		a.x = 1;
		b.x = 2;

		a.test();
		a.nonvirtual();
		b.test();

		// the following line assigns null to c, because C and A are incompatible (class RTTI triggers here)
		C c = a;
		// %t in format string prints any type (note that for %t, args are passed by value)
		"c=%t\n", c;

		{
			std::vector<int> v;
			v.push_back(1);
			v.push_back(2);
			v.push_back(3);
			v.push_back(-7);

			"v=%t\n", v.cdata();

			const int[] tmp = {1, -1, 1, -1, 555};
			v.assign(tmp);

			"v=%t\n", v.cdata();
			"v_size=%t\n", v.size();

			auto w = v;

			// operator overloads need full match
			"w[4]=%t\n", w[cast std::size_t 4];

			w.pop_back();

			"w=%t\n", w.cdata();
		}

		vec v = {1, 2, 3};

		printf("v.length() = %f\n", v.length());

		vec_natural_packing vn = {-1, -2, -3};
		"vn=%t\n", vn;

		printf("Hello, world!\n");
		printf("125/3=%d\n", div(125, 3));
		printf("125/3=%d\n", div2(125, 3));

		auto spawned = spawn();
		"spawned: %t\n", spawned;

		auto spawned2 = spawn2();
		"spawned2: %t\n", spawned2;

		pass_ptr(spawned2);

		// simple incremental loop one billion times
		int loops = 0;

		for (int i=0; i<1'000'000'000; i++)
		{
			++loops;
		}
	}


)src";

	engine.BindNativeFunction("spawn", native_spawn);
	engine.BindNativeFunction("spawn2", native_spawn2);
	engine.BindNativeFunction("pass_ptr", native_pass_ptr);

	engine.BindNativeFunction("printf", native_printf);
	engine.BindNativeFunction("div", native_div);
	engine.BindNativeFunction("div2", native_div_2);

	engine.BindNativeStruct("vec_natural_packing", sizeof(vec_natural_packing), alignof(vec_natural_packing));

	auto ns = engine.BindNativeStruct("vec", sizeof(vec), alignof(vec));
	ns.Member("x", lethe::offset_of(&vec::x));
	ns.Member("y", lethe::offset_of(&vec::y));
	ns.Member("z", lethe::offset_of(&vec::z));

	engine.BindNativeFunction("vec::length", native_vec_length);

	bind_std_vector<int>::bind("int", engine);
	bind_std_vector<lethe::String>::bind("string", engine);

	auto nc = engine.BindNativeClass(
		"direct_native_class",
		sizeof(direct_native_class),
		alignof(direct_native_class),
		// since ScriptBaseObject overrides new and delete, placement new needs scope resolution (::)
		[](void *instPtr) {::new(instPtr) direct_native_class;},
		[](void *instPtr) {static_cast<direct_native_class *>(instPtr)->~direct_native_class();}
	);

	nc.Member("nativeString", lethe::offset_of(&direct_native_class::nativeString));

	engine.BindNativeFunction("direct_native_class::test",
		[](lethe::Stack &stk)
		{
			static_cast<direct_native_class *>(stk.GetThis())->test();
		}
	);

	engine.onError = [](const lethe::String &msg, const lethe::TokenLocation &loc)
	{
		xprintf("err [%d:%d %s] %s\n", loc.line, loc.column, loc.file.Ansi(), msg.Ansi());
	};

	engine.onWarning = [](const lethe::String &msg, const lethe::TokenLocation &loc, lethe::Int warnid)
	{
		xprintf("warn(%d) [%d:%d %s] %s\n", warnid, loc.line, loc.column, loc.file.Ansi(), msg.Ansi());
	};

	bool ok = engine.CompileBuffer(source, "my_source_buffer.lethe");

	// in debug server mode we want to keep AST so that goto definition works in the debugger
	ok = ok && engine.Link(test_debug_server ? lethe::LINK_CLONE_AST_FIND_DEFINITION : 0);

	if (!ok)
	{
		lethe::Done();
		return -1;
	}

	auto ctx = engine.CreateContext();

	if (test_debug_server)
	{
		// set context name to main so that we see it by name in the debugger
		ctx->SetName("main");
	}

	ctx->onRuntimeError = [](const char *msg)
	{
		// here we can route runtime errors (debug mode) elsewhere
		(void)msg;
	};

	// run global static constructor (=static init)
	ctx->RunConstructors();

	test_script_spawn(ctx);

	if (test_debug_server)
	{
		engine.CreateDebugServer();
		auto *dsrv = engine.GetDebugServer();

		g_dsrv = dsrv;

		if (dsrv)
		{
			// we fake ReadScriptFile here by always returning our source buffer
			dsrv->onReadScriptFile = [source](const lethe::String &)->lethe::String
			{
				return source;
			};
		}

		engine.StartDebugServer();

		// assuming the debugger is running, wait for up to 1 second and break main context if successful
		if (engine.WaitForDebugger(1000))
			ctx->Break();
	}

	lethe::PerfWatch pw;
	pw.Start();

	ctx->Call("main");

	auto &stk = ctx->GetStack();

	// pushing in reverse order:
	// result, second arg and finally first arg
	stk.PushRaw(1);
	stk.PushInt(3);
	stk.PushInt(125);
	ctx->Call("script_div");
	// in lethe, like in C/C++, the caller has to clean up stack after call
	stk.Pop(2);
	// retrieve result
	auto res = stk.GetSignedInt(0);
	stk.Pop(1);

	xprintf("script_div returns %d\n", res);

	if (test_debug_server)
	{
		g_dsrv = nullptr;
		engine.StopDebugServer();
	}

	// run global static destructor (=static done)
	ctx->RunDestructors();

	xprintf("exec took " LETHE_FORMAT_ULONG " usec\n", pw.Stop());

	lethe::Done();

	return 0;
}
