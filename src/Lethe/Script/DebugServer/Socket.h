#pragma once

#include "../Common.h"

#include <Lethe/Core/String/String.h>
#include <Lethe/Core/Sys/Platform.h>
#include "NetAddr.h"

namespace lethe
{

class LETHE_API Socket
{
public:
	Socket(bool ntcp = 0);
	~Socket();

	// for TCP sockets: bind to addr
	bool Bind(const String &ip, Int port);
	// for UDP server listener
	bool BindAny(Int port, bool keepAddr = 0);
	// for both types of sockets: connect to addr (for UDP addr only stored internally)
	bool Connect(const String &ip, Int port);
	bool Connect(const NetAddr &naddr);
	// TCP-only
	bool Listen() const;
	// TCP-only, accept listening socket (iirc)
	Socket *Accept();

	// send/receive
	bool Send(const void *buf, Int len) const;
	Int Recv(void *buf, Int len, NetAddr &naddr) const;
	// TCP only
	Int Recv(void *buf, Int len) const;

	// special TCP function to act as a datagram over TCP
	bool SendData(const Array<Byte> &buf);
	bool SendData(const StringRef &sr);
	bool RecvData(String &buf);
	bool RecvData(Array<Byte> &buf);

	String GetIP() const;
	Int GetPort() const;

	inline const NetAddr &GetAddr() const
	{
		return addr;
	}

	// close socket
	void Close();

private:
#if LETHE_OS_WINDOWS
	typedef void *SocketType;
#else
	typedef int SocketType;
#endif
	SocketType handle;
	NetAddr addr;
	bool tcp;
	bool connected;

	template<typename BufType>
	bool RecvDataInternal(BufType &buf);
};

}
