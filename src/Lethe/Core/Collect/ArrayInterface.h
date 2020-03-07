#pragma once

#include "Array.h"
#include "../Delegate/Delegate.h"

namespace lethe
{

// array interface commands
enum ArrayIntfCommand
{
	ACMD_GET_SIZE,
	ACMD_GET_ELEM,
	ACMD_GET_DATA,
	ACMD_CLEAR,
	ACMD_INSERT,
	ACMD_ADD,
	ACMD_RESIZE,
	ACMD_ERASE,
	ACMD_ERASE_FAST,
	ACMD_POP
};

typedef Delegate<Int(ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam)> ArrayCommandFunc;

template<typename T>
struct ArrayInterface
{
	static Int Command(ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam)
	{
		Int res = 0;
		auto &arr = *static_cast<Array<T> *>(aptr);

		switch(cmd)
		{
		case ACMD_GET_SIZE:
			res = arr.GetSize();
			break;

		case ACMD_GET_DATA:
			*static_cast<T **>(pparam) = arr.GetData();
			break;

		case ACMD_GET_ELEM:
			*static_cast<T *>(pparam) = arr[iparam];
			break;

		case ACMD_CLEAR:
			arr.Clear();
			break;

		case ACMD_INSERT:
			arr.Insert(iparam, *static_cast<const T *>(pparam));
			break;

		case ACMD_ADD:
			res = arr.Add(*static_cast<const T *>(pparam));
			break;

		case ACMD_RESIZE:
			arr.Resize(iparam);
			break;

		case ACMD_ERASE:
			arr.EraseIndex(iparam);
			break;

		case ACMD_ERASE_FAST:
			arr.EraseIndexFast(iparam);
			break;

		case ACMD_POP:
			arr.Pop();
			break;
		}

		return res;
	}
};

}
