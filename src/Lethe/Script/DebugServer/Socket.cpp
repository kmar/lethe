#include <Lethe/Core/String/StringRef.h>
#include "Socket.h"
#include "NetAddr.h"

#include "Inline/NetOsIncludes.inl"

namespace lethe
{

// Socket

static inline SOCKET InvalidSocketHandle()
{
#if LETHE_OS_WINDOWS
	return INVALID_SOCKET;
#else
	return -1;
#endif
}

static inline bool InvalidSocket(SOCKET s)
{
#if LETHE_OS_WINDOWS
	return s == INVALID_SOCKET;
#else
	return s < 0;
#endif
}

Socket::Socket(bool ntcp)
	: handle(reinterpret_cast<SocketType>(InvalidSocketHandle()))
	, tcp(ntcp)
	, connected(false)
{
	LETHE_COMPILE_ASSERT(sizeof(sockaddr_in) <= sizeof(NetAddr));

	if (tcp)
		handle = reinterpret_cast<SocketType>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	else
		handle = reinterpret_cast<SocketType>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
}

Socket::~Socket()
{
	Close();
}

void Socket::Close()
{
	auto shandle = reinterpret_cast<SOCKET>(handle);

	if (!InvalidSocket(shandle))
	{
#if LETHE_OS_WINDOWS
		::shutdown(shandle, SD_BOTH);
#else
		::shutdown(shandle, SHUT_RDWR);
#endif
		::closesocket(shandle);
		handle = reinterpret_cast<SocketType>(InvalidSocketHandle());
	}
}

bool Socket::Bind(const String &ip, Int port)
{
	addr.Clear();

	LETHE_RET_FALSE(tcp);

	sockaddr_in local;
	MemSet(&local, 0, sizeof(local));
	local.sin_family=AF_INET;

	local.sin_addr.s_addr=INADDR_ANY;
	local.sin_port = htons((UShort)port);

	if (inet_addr(ip.Ansi()) == INADDR_NONE)
	{
		hostent *hp = gethostbyname(ip.Ansi());
		LETHE_RET_FALSE(hp);
		local.sin_addr.s_addr = *reinterpret_cast<in_addr_t *>(hp->h_addr);
	}
	else
		local.sin_addr.s_addr = inet_addr(ip.Ansi());

	*reinterpret_cast<sockaddr_in *>(addr.data) = local;

	bool bindFailed = ::bind(reinterpret_cast<SOCKET>(handle), reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0;

	return !bindFailed;
}

bool Socket::BindAny(Int port, bool keepAddr)
{
	connected = 1;
	sockaddr_in local;
	MemSet(&local, 0, sizeof(local));
	local.sin_family=AF_INET;

	local.sin_addr.s_addr=INADDR_ANY;
	local.sin_port = htons((UShort)port);

	if (!keepAddr)
	{
		addr.Clear();
		*reinterpret_cast<sockaddr_in *>(addr.data) = local;
	}

	int rad = 1;
	::setsockopt(reinterpret_cast<SOCKET>(handle), SOL_SOCKET, SO_REUSEADDR,
				 reinterpret_cast<char *>(&rad), sizeof(int));
	bool bindFailed = ::bind(reinterpret_cast<SOCKET>(handle), reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0;

	return !bindFailed;
}

bool Socket::Listen() const
{
	LETHE_RET_FALSE(tcp);
	return ::listen(reinterpret_cast<SOCKET>(handle), 10) == 0;
}

Socket *Socket::Accept()
{
	LETHE_RET_FALSE(tcp);
	sockaddr_in local;
	MemSet(&local, 0, sizeof(local));
	socklen_t len = sizeof(local);
	SOCKET cli;
	cli = ::accept(reinterpret_cast<SOCKET>(handle), reinterpret_cast<sockaddr *>(&local), &len);

	if (InvalidSocket(cli))
		return nullptr;

	Socket *res = new Socket;
	res->handle = reinterpret_cast<SocketType>(cli);
	addr.Clear();
	*reinterpret_cast<sockaddr_in *>(res->addr.data) = local;

	int nod = 1;
	setsockopt(cli, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&nod), sizeof(int));
	return res;
}

bool Socket::Connect(const NetAddr &naddr)
{
	if (tcp)
	{
		// FIXME: implement!!!
		return 0;
	}

	addr = naddr;
	connected = 1;
	return 1;
}

bool Socket::Connect(const String &ip, Int port)
{
	struct sockaddr_in server;
	MemSet(&server, 0, sizeof(server));

	if (inet_addr(ip.Ansi()) == INADDR_NONE)
	{
		hostent *hp = gethostbyname(ip.Ansi());
		LETHE_RET_FALSE(hp);
		server.sin_addr.s_addr = *reinterpret_cast<in_addr_t *>(hp->h_addr);
	}
	else
		server.sin_addr.s_addr = inet_addr(ip.Ansi());

	server.sin_family = AF_INET;
	server.sin_port = htons((UShort)port);

	addr.Clear();
	*reinterpret_cast<sockaddr_in *>(addr.data) = server;

	if (tcp)
	{
		if(::connect(reinterpret_cast<SOCKET>(handle),
					 reinterpret_cast<struct sockaddr *>(&server), sizeof(server)))
			return 0;

		int nod = 1;
		::setsockopt(reinterpret_cast<SOCKET>(handle), IPPROTO_TCP, TCP_NODELAY,
					 reinterpret_cast<char *>(&nod), sizeof(int));
	}
	else
	{
		// we cannot bind to a specific remote address so we bind to any here. this requires to open a specific port for clients!
		BindAny(port, 1);
		connected = 1;
	}

	return 1;
}

bool Socket::Send(const void *buf, Int len) const
{
	if (connected)
	{
		return ::sendto(reinterpret_cast<SOCKET>(handle),
						static_cast<const char *>(buf), len, 0, reinterpret_cast<const sockaddr *>(addr.data),
						sizeof(sockaddr_in)) == len;
	}

	return ::send(reinterpret_cast<SOCKET>(handle), static_cast<const char *>(buf), len, 0) == len;
}

Int Socket::Recv(void *buf, Int len) const
{
	return (Int)::recv(reinterpret_cast<SOCKET>(handle), static_cast<char *>(buf), len, 0);
}

Int Socket::Recv(void *buf, Int len, NetAddr &naddr) const
{
	if (!connected)
	{
		naddr = addr;
		return (Int)::recv(reinterpret_cast<SOCKET>(handle), static_cast<char *>(buf), len, 0);
	}

	socklen_t froml = sizeof(sockaddr_in);
	naddr.Clear();
	return (Int)::recvfrom(reinterpret_cast<SOCKET>(handle), static_cast<char *>(buf), len, 0,
						   reinterpret_cast<sockaddr *>(naddr.data), &froml);
}

String Socket::GetIP() const
{
	return addr.GetIP();
}

Int Socket::GetPort() const
{
	return addr.GetPort();
}

bool Socket::SendData(const StringRef &sr)
{
	auto sz = sr.GetLength();
	return Send(&sz, sizeof(sz)) && (sz == 0 ? true : Send(sr.GetData(), sr.GetLength()));
}

bool Socket::SendData(const Array<Byte> &buf)
{
	auto sz = buf.GetSize();
	return Send(&sz, sizeof(sz)) && (sz == 0 ? true : Send(buf.GetData(), sizeof(buf)));
}

bool Socket::RecvData(String &buf)
{
	buf.Clear();
	Array<Byte> tbuf;
	LETHE_RET_FALSE(RecvData(tbuf));

	buf = String(reinterpret_cast<const char *>(tbuf.GetData()), tbuf.GetSize());

	return true;
}

bool Socket::RecvData(Array<Byte> &buf)
{
	return RecvDataInternal(buf);
}

template<typename BufType>
bool Socket::RecvDataInternal(BufType &buf)
{
	buf.Clear();

	Int len;
	LETHE_RET_FALSE(Recv(&len, sizeof(len)) == sizeof(len));

	buf.Resize(len);

	if (!len)
		return true;

	auto *dst = buf.GetData();
	auto dlen = buf.GetSize();

	// loop in case TCP can't accomodate large data
	while (dlen > 0)
	{
		auto rlen = Recv(dst, dlen);

		LETHE_RET_FALSE(rlen > 0);

		dst += rlen;
		dlen -= rlen;

		LETHE_ASSERT(dlen >= 0);
	}

	return true;
}

}
