

# lethe
a statically typed, unsafe scripting language

note: experimental, work in progress, there will most likely be bugs => use at your own risk

* requirements:
	* C++11 compiler
	* IEEE floating point
	* 2's complement integers
	* sizeof(bool) == 1
	* sizeof(char) == 1
	* sizeof(int) == 4
	* little endian CPU
	* script structs must be trivially movable (=no self-refs, implies no small buffer opts)

* features:
	* unsafe
	* no user-defined conversions
	* no function overloads
	* no exceptions
	* no garbage collector (uses intrusive reference counting)
	* no REPL
	* no multi-threaded compilation of scripts
	* no bytecode serialization
	* optional dumb JIT for x86/x64
	* [debugger - win64/linux - binary only](https://github.com/kmar/lethe_debugger/releases)

* todo:
	* clean up/simplify/modernize
	* better documentation, more samples/libs

hello world (makes use of Terry A. Davis' (RIP) implied printf for expressions starting with a string literal):
```cpp
		void main()
		{
			"Hello, world!\n";
		}
```
