
# Lethe scripting language

#### (draft)

* lexical structure
	* numbers
	* string literals
	* name literals
	* character literals

* data types
	* [null](#null_type)
	* [numeric](#numeric_types)
	* [enum](#enum_type)
	* [string](#string_type)
	* [name](#name_type)
	* [virtual property](#virtual_props)
	* [struct](#struct_type)
	* [class](#class_type)
	* [pointer](#pointer_type)
	* [arrays / array references](#array_type)
	* [function pointers / delegates](#func_type)

* [macro preprocessor](#preprocessor)

* [operators](#operators)

* [statements](#statements)

* [missing features](#missing_features)

* [limitations](#limitations)

<a id="null_type"></a>
#### null type
**null** simply represents a null pointer to object/function ptr/delegate

<a id="numeric_types"></a>
#### numeric data types
* **bool** (8-bit boolean)
* **sbyte** (signed 8-bit integer)
* **byte** (unsigned 8-bit integer)
	* byte/ubyte would be tempting (consistency), but `byte = unsigned` was a clear choice
* **short** (signed 16-bit integer)
* **ushort** (unsigned 16-bit integer)
* **char** (signed 32-bit integer)
	* basically the same as int, but used as a separate type to be able to handle character literals
	* character literals are similar to name literals but start with the u prefix:
		* u'A' // this is a character literal
* **int** (signed 32-bit integer)
* **uint** (unsigned 32-bit integer)
* **long** (signed 64-bit integer)
* **ulong** (unsigned 64-bit integer)
* **float** (32-bit floating point)
* **double** (64-bit floating point)

<a id="enum_type"></a>
#### enums
```cpp
	enum MyEnum
	{
		A,
		B
	}
```
enums are always of type int and enum items injected directly into enclosing scope. scoped enums have to be emulated with namespaces.

<a id="string_type"></a>
#### strings
**string** is an internal data type (reference counted copy-on-write) in UTF-8 format.

Individual bytes can be indexed as bytes of type char. This is of course wrong for Unicode characters where one has to handle multi-byte sequences manually.
```cpp
"this is a string literal"
```
_triple quotes can be used to produce a raw string literal:_
```cpp
"""this is a raw string literal"""
```
<a id="name_type"></a>
#### names
**name** is global unique string stored as a 32-bit integer (this may change in the future to 64-bit)
internally they are used for class names but are exposed to the scripting language
```cpp
'this is a name literal'
```
<a id="virtual_props"></a>
#### virtual properties
virtual properties are disguised as member/global variables with custom logic implemented in getters/setters
```cpp
struct S
{
	// backing store
	int _vprop;

	// virtual property
	int vprop:
	{
		inline int get() const
		{
			return _vprop;
		}
		// note: returning void will be faster but we won't be able to chain assignments
		inline int set(int value)
		{
			_vprop = value;
			return value;
		}
	}
}
```
<a id="struct_type"></a>
#### structs
**struct** is a lightweight, POD-like data type. it can contain methods but has no vtable, so all methods are _final_.
they are not objects so structs don't work with pointers or delegates

structs also have a unique ability to act as arrays if all its members are of the same type with empty base struct
this is very useful for vectors:
```cpp
	struct my_vector
	{
		float x, y, z;
	}

	...

	my_vector v;
	v[0] = 42;
```
simple generics can be used with structs, but no specialization and no nested generics
```cpp
	struct generic<T>
	{
		T x, y;
	}

	...

	generic<int> ivec2;
	ivec2.x = 33;
```
<a id="class_type"></a>
#### classes
**class** is a heavyweight, heap-allocated data type. it has implied vtable and all methods not marked as final are virtual by default
class instances must be created using the new keyword or passed in externally as pointers
only single inheritance is support and unlike C++, the default accessibility in a class is public instead of private

intrinsic class methods:

	final bool is(name base_class_name) const;

simply returns if the class type of an instance is based on class of name base_class_name (there is no _is_ operator, but it should be more flexible as you can test against variables)
note than class name must be fully qualified if namespace is used

	final name class_name() const

return name of instance class

	void vtable(name new_class_name)

allows to change vtable to compatible class type (must not define any new member variables to succeed)
useful to change behavior on the fly; however it's not thread-safe so one should be careful with this.
vtable method is virtual and can be overridden

<a id="pointer_type"></a>
#### pointers
Lethe doesn't use garbage collection so class instances (objects) are held in smart pointers.
two types of smart pointers are available: strong pointers (default) and weak pointers:
```cpp
	class MyClass
	{
		int x;
	}

	...

	// mc is a strong pointer to MyClass
	MyClass mc = new MyClass;
	mc.x += 3;
	// mcw is a weak pointer to MyClass
	weak MyClass mcw = mc;
```
weak pointers must be converted to strong pointers before we can dereference the underlying object:
```cpp
	MyClass locked = mcw;
	locked.x *= 10;
```
alternatively (using auto):
```cpp
	auto locked = mcw;
	locked.x *= 10;
```
smart pointers are implemented as intrusive. in order to reduce reference counting pressure, smart pointers passed by value are virtually converted raw pointers.

a third type is supported, unsafe raw pointer (bypasses reference counting)
```cpp
	raw auto raw_ptr = mcw;
```
there's also a special numeric type `pointer` aliasing the integer type necessary to hold native pointers. it can be used to store wrapped C++ objects.

<a id="array_type"></a>
#### arrays / array references
Lethe supports two types or arrays (static and dynamic) and a reference-only type (array reference/view).
dynamic arrays are allocated on heap and are basically identical to std::vector in C++.
internally a dynamic array consist of data pointer, int size and int capacity (in elements)
```cpp
	int[5] my_static_array = {1,2,3,4,5};
	int my_static_array_1[5] = {1,2,3,4,5};
	int my_static_array_2[] = {1,2,3,4,5};
	int[] my_static_array_3 = {1,2,3,4,5};
	array<int> my_dynamic_array;
```
array references are special and deserve native support in the language:
```cpp
	int[] my_array_ref;
	my_array_ref = my_static_array;
	int[] my_slice = my_dynamic_array.slice(2, 4);
```
internally, an array reference is represented as data pointer and size

native properties can be used to determine array size:
```cpp
	my_array_ref.size
	my_dynamic_array.size
	my_static_array.size
```
<a id="func_type"></a>
#### function pointers / delegates
function pointers simply hold a pointer to non-methods
```cpp
	void function(int x) my_func;

	void test(int x)
	{
		printf("testing...%d\n", x);
	}

	void main()
	{
		my_func = test;
		// prints "testing...123"
		my_func(123);
	}
```
they must be checked for null before calling, the same holds for delegates (unless you're 100% sure it can't be null of course)
this may change in the future so that the check is automatic, doing nothing if null (or simply returning default value), but it would prevent catching null function pointers
```cpp
	if (my_func_ptr) my_func_ptr()
```
a delegate points to a method within an object/struct (but struct delegates cannot be manually serialized).
```cpp
	void delegate() dg;

	class A
	{
		void init()
		{
			dg = test;
		}

		void test()
		{
			printf("test\n");
		}
	}

	...

	A a = new A;
	a.init();
	dg();
```
delegates are internally represented as two pointers, raw instance pointer (this may change to weak pointer in the future; but this way it's faster and easier) and method pointer.
the method pointer is also used to encode vtable index for virtual methods
in the above example, init binds dg to virtual method `test` (meaning vtable index is stored instead of method pointer). if instead
```cpp
	dg = A::test;
```
then dg would bind to A::test directly, storing method code pointer.

<a id="operators"></a>
#### operators

Lethe supports standard set of C/C++ operators, listing from highest to lowest:

operator | description                 | associativity
---------|-----------------------------|--------------
::       | scope resolution            | left to right
a++ a-- func() a[] .  | post-increment/decrement, function call, subscript, member access |
++a --a +a -a cast new| pre-inc/dec, unary, cast, new| right to left
a*b a/b a%b | multiplication, division, remainder | left to right
a+b a-b | addition, subtraction |
<< >> | bitwise shifts |
< <= > >= | relational inequality |
== != | relational equality |
& | bitwise and |
^ | bitwise xor |
\| | bitwise or |
&& |logical and |
\|\| |logical or |
? : = += <-> | ternary, assignment, swap | right to left
,|comma|

there was a temptation to fix the precedence of bitwise operators and place them just before relational,
but this would break translation to/from C and C++

note that pre/post increment doesn't work with floating point types and += style operators cannot be chained like in C/C++

<a id="preprocessor"></a>
#### macro preprocessor

integrated support for a simple, scoped, token-based macro preprocessor

* defining macros
	* macro macro_name=token_list;
	* macro macro name token_list endmacro
		(this one can span multiple lines)
	note that macros are scoped and cannot be redefined/undefined, they are automatically undefined as they
	leave the scope (i.e. inside a block {})
	global macros stay, but depend on compilation order => put global macros in a file that's compiled first

	* macros with parameters:
		macro_name(arg1, arg2)
		macro_name(arg1, arg2, ...)

		special keywords: __VA_ARGS couple ellipsis, __VA_COUNT holds ellipsis argument count

* stringizing, concatenating
	* simply __stringize x, where x is a macro argument name (equivalent to #x in C preprocessor)
	* concatenation: a __concat b __concat c (equivalent to a ## b ## c)

* conditional compilation
	* macro if (macro_expression)
	* macro else
	* macro else if (macro_expression)
	* macro endif

	note that there's no equivalent to #ifdef / #if defined() => everything is treated as #if

* predefined macros
	* __JIT is defined to be 1 or 0, if JIT is enabled and supported on target platform
	* __DEBUG is defined to be 1 or 0 if debug mode is enabled
	* __ASSERT(x)

note that there's no support for macro includes

example:
```cpp
		macro if (__DEBUG)

		void test()
		{
			"debug\n";
		}

		macro else
		
		void test()
		{
			"release\n";
		}

		macro endif


		void my_func(int count, some_struct c)
		{
			macro var1 = c.var1[i];
			macro var2 = c.var2[i];

			for (int i : count)
				var1.value += var2.value;

			// note that here var1 and var2 leave the scope and will be removed
		}
```
<a id="statements"></a>
#### statements

* control flow
	* **if**/**else**, **while**, **do**/**while**, **for**, **break**/**continue** just like C/C++
	* **switch**/**case**/**default:** ditto
		* but can switch on strings and names as well
		* cannot declare variables inside cases except when using a new block
	* **goto**/**label:** similar to C++ with some restrictions, can only target labels in same/parent scope,
can't skip over variable declarations
* variable declarations, pretty much standard:
```cpp
		int myvar;
		string mystring = "abcd";
```
* expressions, also nothing special
	* function calls **func(..)**
* blocks **{** .. **}**
* function return: **return** _expr_
* **defer** _statement_, defer execution of statement to the end of the block; inspired by Go
	* this would be a killer feature for C, eliminating bailouts

<a id="missing_features"></a>
#### missing features

* there is no exception handling in Lethe
* no garbage collection (using smart pointers instead)
* no function overloads (except for limited overload support for operators)
* very limited support for generic structs
* no memory safety/sandboxing (it's possible for the script to crash the host process)
* no anonymous functions (lambdas)
* no unions
* no bitfields
* only a limited scoped macro preprocessor
* no coroutines, just simple states that require manual support

there's no plan to implement any of those, certainly not exceptions or garbage collection
in fact the primary reason for a new scripting language was state support (similar to what old UnrealScript did), but in the end I simply didn't implement this

<a id="limitations"></a>
#### limitations

* no support for big endian machines (actually I attempted to fix this some time ago)
	* this is something that's bothering me; it should be possible but I couldn't find a fast enough PPC emulator. the closest I got was qemu with some ancient debian distro (but without C++11 support, a no-go unfortunately).
* bool type larger than a single byte (this is also a dumb limitation, but necessary for consistent binding)
* C++ char type must be 1 byte long
* no JIT support for ARM; this would be very nice for Android but on iOS we're out of luck. maybe it would be worthwile to invest the time into some kind of a C++ transpiler instead? The JIT code for x86 is a horrible mess anyway!
* no multi-threaded compilation
* no easy binding to std types like vector and so on
