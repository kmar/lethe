#include <Lethe/Core/String/String.h>
#include "NetAddr.h"

#include "Inline/NetOsIncludes.inl"

namespace lethe
{

// NetAddr

void NetAddr::SetIP(const String &ip)
{
	struct sockaddr_in local;
	MemSet(&local, 0, sizeof(local));
	local.sin_addr.s_addr = inet_addr(ip.Ansi());
	Clear();
	*reinterpret_cast<sockaddr_in *>(data) = local;
}

String NetAddr::GetIP() const
{
	const sockaddr_in *saddr = reinterpret_cast<const sockaddr_in *>(data);

#if LETHE_OS_WINDOWS
	// this is non-portable unfortunately
	Int b1 = saddr->sin_addr.S_un.S_un_b.s_b1;
	Int b2 = saddr->sin_addr.S_un.S_un_b.s_b2;
	Int b3 = saddr->sin_addr.S_un.S_un_b.s_b3;
	Int b4 = saddr->sin_addr.S_un.S_un_b.s_b4;
	return String::Printf("%d.%d.%d.%d", b1, b2, b3, b4);
#else
	char ipAddr[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &saddr->sin_addr, ipAddr, INET_ADDRSTRLEN);
	return ipAddr;
#endif
}

Int NetAddr::GetPort() const
{
	const sockaddr_in *saddr = reinterpret_cast<const sockaddr_in *>(data);
	return saddr->sin_port;
}

void NetAddr::SetPort(Int nport)
{
	sockaddr_in *saddr = reinterpret_cast<sockaddr_in *>(data);
	saddr->sin_port = ntohs((UShort)nport);
}

bool NetAddr::CompareNoPort(const NetAddr &o) const
{
	const sockaddr_in *saddr = reinterpret_cast<const sockaddr_in *>(data);
	const sockaddr_in *saddr2 = reinterpret_cast<const sockaddr_in *>(o.data);
	return MemCmp(&saddr->sin_addr, &saddr2->sin_addr, sizeof(saddr->sin_addr)) == 0;
}

}
