#include "Stack.h"

namespace lethe
{

// Stack

Stack::Stack(Int size)
	: thisPtr(nullptr)
	, cpool(nullptr)
	, context(nullptr)
	, insPtr(nullptr)
	, programCounter(-1)
	, nesting(0)
	, breakExecution(0)
{
	stack.Resize(size + 3, 0);
	top = stack.GetData() + size;
	// 16-byte align stack
	PushAlign(16);
	bottom = top;
}

void Stack::PushStruct(Int align, Int sizeBytes)
{
	// word align
	sizeBytes += sizeof(StackWord)-1;
	sizeBytes &= ~(UInt)(sizeof(StackWord)-1);
	// reserve aligned size in words
	top -= sizeBytes >> (sizeof(StackWord) >> 3);
	PushAlignForce(align);
}

void Stack::PushRawZero(Int words)
{
	PushRaw(words);
	MemSet(top, 0, (size_t)words*WORD_SIZE);
}

void Stack::PushEmptyString()
{
	LETHE_COMPILE_ASSERT(AlignOf<String>::align <= sizeof(StackWord));
	PushRawZero(STRING_WORDS);
}

void Stack::PushString(const String &str)
{
	PushEmptyString();
	*reinterpret_cast<String *>(top) = str;
}

void Stack::SetString(Int offset, const String &val)
{
	*reinterpret_cast<String *>(top + offset) = val;
}

const String &Stack::GetString(Int offset) const
{
	return *reinterpret_cast<const String *>(top + offset);
}

void Stack::DelString(Int offset)
{
	reinterpret_cast<String *>(top + offset)->~String();
}

void Stack::PopString()
{
	DelString(0);
	Pop(1);
}

}
