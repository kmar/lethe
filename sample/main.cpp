#include <Lethe/Lethe.h>

#include <stdio.h>
#include <cmath>

#include <vector>

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
	printf("%s", lethe::FormatStr(stk).Ansi());
}

// this must match script struct layout
struct vec
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

int main()
{
	lethe::Init();

	lethe::ScriptEngine engine(lethe::ENGINE_JIT);

	const char *source = R"src(

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

	void __init()
	{
		"static init\n";
	}

	void __exit()
	{
		"static exit\n";
	}

	void main()
	{
		object o = new object;
		array<int> iarr;
		iarr.add(42);

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

		printf("Hello, world!\n");
		printf("125/3=%d\n", div(125, 3));
		printf("125/3=%d\n", div2(125, 3));
		// empty loop one billion times
		for (int i=0; i<1'000'000'000; i++);
	}


	)src";

	engine.BindNativeFunction("printf", native_printf);
	engine.BindNativeFunction("div", native_div);
	engine.BindNativeFunction("div2", native_div_2);

	auto ns = engine.BindNativeStruct("vec", sizeof(vec), alignof(vec));
	ns.Member("x", offsetof(vec, x));
	ns.Member("y", offsetof(vec, y));
	ns.Member("z", offsetof(vec, z));

	engine.BindNativeFunction("vec::length", native_vec_length);

	bind_std_vector<int>::bind("int", engine);
	bind_std_vector<lethe::String>::bind("string", engine);

	engine.onError = [](const lethe::String &msg, const lethe::TokenLocation &loc)
	{
		printf("err [%d:%d %s] %s\n", loc.line, loc.column, loc.file.Ansi(), msg.Ansi());
	};

	engine.onWarning = [](const lethe::String &msg, const lethe::TokenLocation &loc, lethe::Int warnid)
	{
		printf("warn(%d) [%d:%d %s] %s\n", warnid, loc.line, loc.column, loc.file.Ansi(), msg.Ansi());
	};

	bool ok = engine.CompileBuffer(source, "my_source_buffer");

	ok = ok && engine.Link();

	if (!ok)
	{
		lethe::Done();
		return -1;
	}

	auto ctx = engine.CreateContext();

	ctx->onRuntimeError = [](const char *msg)
	{
		// here we can route runtime errors (debug mode) elsewhere
		(void)msg;
	};

	// run global static constructor (=static init)
	ctx->RunConstructors();

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

	printf("script_div returns %d\n", res);

	// run global static destructor (=static done)
	ctx->RunDestructors();

	printf("exec took " LETHE_FORMAT_ULONG " usec\n", pw.Stop());

	lethe::Done();

	return 0;
}
