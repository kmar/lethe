#pragma once

#include "Opcodes.h"

#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/Sys/Inline.h>
#include <Lethe/Core/String/String.h>

namespace lethe
{

class ScriptContext;
class CompiledProgram;

// in script VM, stack grows up
// stack granularity is 4 bytes (uint)
// why is this beneficial? for accessing locals with int offset
// so fetching last pushed int means GetInt(-1) and so on
// since it's compact, stack has almost the same layout in 64-bit mode [of course pointers use different layout]
// no => since we need to maintain consistency, references will be pushed as 64-bit quantities; also strings will
// have be pushed to occupy the same amount of stack as in 64-bit mode so as a result stack must have the same layout
// (this means a penalty in 32-bit mode)
// I'm not sure at all; naturally 32-bit bytecode won't be compatible with 64-bit bytecode [is this REALLY a good idea?!]
// hmm... what about a stack growing down:
// + advantage: natural struct layout in memory (otherwise would have to reverse it)
// + advantage: stack top points directly to newly pushed variable
// + advantage: stack alignment becomes easier
// - disadvantage: sequential vars grow "downwards"
// => going for top-down stack
class LETHE_API Stack
{
public:
	typedef UIntPtr StackWord;

	enum
	{
		WORD_SIZE = (int)sizeof(StackWord)
	};

private:
	// top of stack, grows down
	StackWord *top;
	// only one register here: this pointer
	const void *thisPtr;
	// return addresses is actually code indices
	Array< StackWord > stack;
	// bottom of stack
	StackWord *bottom;

public:
	enum Constants
	{
		INT_WORDS = 1,
		LONG_WORDS = (sizeof(Long) + sizeof(StackWord)-1)/sizeof(StackWord),
		DOUBLE_WORDS = (sizeof(Double) + sizeof(StackWord)-1)/sizeof(StackWord),
		STRING_WORDS = (sizeof(String) + sizeof(StackWord)-1)/sizeof(StackWord),
		POINTER_WORDS = (sizeof(void *) + sizeof(StackWord)-1)/sizeof(StackWord),
		INTS_IN_WORD = WORD_SIZE / sizeof(Int)
	};

	// default to 64k-entry stack (512kB in 64-bit mode, 256kB in 32-bit mode [this may change to 512kB in the future])
	Stack(Int size = 64*1024);

	// align size up to stack word size
	static inline Int AlignSize(Int size)
	{
		return (size + WORD_SIZE-1) & ~(WORD_SIZE-1);
	}

	// use this if align is constant
	inline void PushAlign(Int align);
	// use this if align is variable
	inline void PushAlignForce(Int align);

	inline void PushStruct(Int align, Int sizeBytes);

	inline void PushRaw(Int words);
	void PushRawZero(Int words);

	void PushEmptyString();
	void PushString(const String &str);
	void PopString();
	void DelString(Int offset);
	void SetString(Int offset, const String &val);
	const String &GetString(Int offset) const;

	inline UInt GetInt(Int offset) const;
	inline Int GetSignedInt(Int offset) const;
	inline ULong GetLong(Int offset) const;
	inline Long GetSignedLong(Int offset) const;
	inline void *GetPtr(Int offset) const;
	inline Float GetFloat(Int offset) const;
	inline Double GetDouble(Int offset) const;
	inline void SetInt(Int offset, UInt value);
	inline void SetLong(Int offset, ULong value);
	inline void SetFloat(Int offset, Float value);
	inline void SetDouble(Int offset, Double value);
	inline void SetPtr(Int offset, const void *value);
	inline void PushBool(bool value);
	inline void PushInt(UInt value);
	inline void PushLong(ULong value);
	inline void PushFloat(Float value);
	inline void PushDouble(Double value);
	inline void PushPtr(const void *value);
	inline void PushName(Name value);

	template<typename T>
	inline void PushArrayRef(const ArrayRef<T> &aref);
	inline void PopArrayRef() {Pop(2);}

	inline const void *GetThis() const;
	inline void *GetThis();
	inline void SetThis(const void *ptr);

	// check if we can push this many words without overflow
	inline bool Check(Int words) const;

	// pop n stack words
	inline void Pop(Int words);

	inline StackWord *GetTop() const
	{
		return top;
	}

	inline void SetTop(StackWord *ntop)
	{
		top = ntop;
	}

	inline const StackWord *GetBottom() const
	{
		return bottom;
	}

	inline const StackWord *GetBase() const
	{
		return stack.GetData();
	}

	inline ScriptContext &GetContext() const
	{
		LETHE_ASSERT(context);
		return *context;
	}

	inline const ConstPool &GetConstantPool() const
	{
		LETHE_ASSERT(cpool);
		return *cpool;
	}

	inline const CompiledProgram &GetProgram() const
	{
		LETHE_ASSERT(prog);
		return *prog;
	}

	inline void SetInsPtr(const Instruction *iptr) {insPtr = iptr;}

	inline const Instruction *GetInsPtr() const {return insPtr;}

	inline Int GetNesting() const {return nesting;}

private:
	friend class Vm;
	friend class VmJitX86;
	friend class ScriptContext;

	// FIXME: hack but necessary for some builtins
	ConstPool *cpool;
	CompiledProgram *prog;
	ScriptContext *context;

	// extra state for debugging
	// FIXME: this really belongs in ScriptContext
	// instruction ptr for extracting call stack
	const Instruction *insPtr;
	Int programCounter;
	// breakpoint hit flag
	Int breakpointHit;
	// nested script call flag
	Int nesting;
	// break program flag
	AtomicInt breakExecution;
};

struct ArgParser
{
	inline ArgParser(Stack &stk, bool thisCall = 0)
		: ptr(stk.GetTop())
	{
		if (thisCall)
			Get<const void *>();
	}

	template<typename T>
	inline T &Get()
	{
		T &res = *reinterpret_cast<T *>(ptr);
		ptr += (sizeof(T) + Stack::WORD_SIZE-1)/Stack::WORD_SIZE;
		return res;
	}
private:
	Stack::StackWord *ptr;
};

struct ArgParserMethod : public ArgParser
{
	inline ArgParserMethod(Stack &stk)
		: ArgParser(stk, true)
	{
	}
};

inline void Stack::PushAlignForce(Int align)
{
	UIntPtr itop = (UIntPtr)top;
	itop &= ~((UIntPtr)align-1);
	top = reinterpret_cast<StackWord *>(itop);
}

inline void Stack::PushAlign(Int align)
{
	if ((Int)sizeof(StackWord) < align)
		PushAlignForce(align);
}

inline void Stack::PushRaw(Int words)
{
	top -= words;
}

inline UInt Stack::GetInt(Int offset) const
{
	return *reinterpret_cast<const UInt *>(top + offset);
}

inline Int Stack::GetSignedInt(Int offset) const
{
	return *reinterpret_cast<const Int *>(top + offset);
}

inline ULong Stack::GetLong(Int offset) const
{
	return *reinterpret_cast<const ULong *>(top + offset);
}

inline Long Stack::GetSignedLong(Int offset) const
{
	return *reinterpret_cast<const Long *>(top + offset);
}

inline void *Stack::GetPtr(Int offset) const
{
	return reinterpret_cast<void *>(top[offset]);
}

inline void Stack::SetPtr(Int offset, const void *value)
{
	*reinterpret_cast<const void **>(top + offset) = value;
}

inline void Stack::SetInt(Int offset, UInt value)
{
	*reinterpret_cast<UInt *>(top + offset) = value;
}

inline void Stack::SetLong(Int offset, ULong value)
{
	*reinterpret_cast<ULong *>(top + offset) = value;
}

inline void Stack::PushInt(UInt value)
{
	*reinterpret_cast<UInt *>(--top) = value;
}

inline void Stack::PushBool(bool value)
{
	*reinterpret_cast<UInt *>(--top) = (UInt)value;
}

inline void Stack::PushLong(ULong value)
{
	top -= 1 + (WORD_SIZE <= 4);
	*reinterpret_cast<ULong *>(top) = value;
}

inline Float Stack::GetFloat(Int offset) const
{
	return *reinterpret_cast<const Float *>(top + offset);
}

inline void Stack::SetFloat(Int offset, Float value)
{
	*reinterpret_cast<Float *>(top + offset) = value;
}

inline void Stack::PushFloat(Float value)
{
	*reinterpret_cast<Float *>(--top) = value;
}

inline void Stack::PushName(Name name)
{
	*reinterpret_cast<Name *>(--top) = name;
}

inline Double Stack::GetDouble(Int offset) const
{
	return *reinterpret_cast<const Double *>(top + offset);
}

inline void Stack::SetDouble(Int offset, Double value)
{
	*reinterpret_cast<Double *>(top + offset) = value;
}

inline void Stack::PushDouble(Double value)
{
	top -= 1 + (WORD_SIZE <= 4);
	*reinterpret_cast<Double *>(top) = value;
}

inline void Stack::PushPtr(const void *value)
{
	*--top = (StackWord)(value);
}

inline void Stack::Pop(Int words)
{
	top += words;
}

inline bool Stack::Check(Int words) const
{
	return top - stack.GetData() >= words;
}

inline const void *Stack::GetThis() const
{
	return thisPtr;
}

inline void *Stack::GetThis()
{
	return const_cast<void *>(thisPtr);
}

inline void Stack::SetThis(const void *ptr)
{
	thisPtr = ptr;
}

template<typename T>
inline void Stack::PushArrayRef(const ArrayRef<T> &aref)
{
	PushRaw(2);
	MemCpy(top, &aref, sizeof(aref));
}

}
