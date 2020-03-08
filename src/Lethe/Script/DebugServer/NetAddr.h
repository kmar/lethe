#pragma once

#include "../Common.h"

#include <Lethe/Core/Sys/Types.h>
#include <Lethe/Core/Memory/Memory.h>

namespace lethe
{

class String;

class LETHE_API NetAddr
{
public:
	// raw data
	UInt data[4];

	inline NetAddr()
	{
		Clear();
	}

	inline NetAddr &Clear()
	{
		MemSet(data, 0, sizeof(data));
		return *this;
	}

	inline bool operator ==(const NetAddr &o) const
	{
		return MemCmp(data, o.data, sizeof(data)) == 0;
	}

	// compare ignoring port, I get non-matching ports after recvfrom
	// and it's probably ok...
	bool CompareNoPort(const NetAddr &o) const;

	void SetIP(const String &ip);
	String GetIP() const;
	Int GetPort() const;
	void SetPort(Int nport);
};

}
