#if LETHE_OS_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	if !defined(_WINSOCK_DEPRECATED_NO_WARNINGS)
// FIXME: ...
#		define _WINSOCK_DEPRECATED_NO_WARNINGS
#	endif
#	include <windows.h>
#	include <winsock2.h>
#	include <ws2tcpip.h>
typedef unsigned long in_addr_t;
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <netdb.h>
#	include <arpa/inet.h>
#	include <unistd.h>
typedef int SOCKET;
#	define closesocket close
#endif
