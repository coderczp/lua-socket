#include "PackageSocket.h"

#if WiN32
#include <Winsock2.h>
#include <Wininet.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#endif

#define UDP_ADDRESS_SIZE 19	// ipv6 128bit + port 16bit + 1 byte type

struct write_buffer {
	struct write_buffer * next;
	void *buffer;
	char *ptr;
	int sz;
	bool userobject;
	uint8_t udp_address[UDP_ADDRESS_SIZE];
};

#define SIZEOF_TCPBUFFER (offsetof(struct write_buffer, udp_address[0]))
#define SIZEOF_UDPBUFFER (sizeof(struct write_buffer))

struct wb_list {
	struct write_buffer * head;
	struct write_buffer * tail;
};

static inline void
clear_wb_list(struct wb_list *list) {
	list->head = NULL;
	list->tail = NULL;
}

PackageSocket::PackageSocket()
	:_state(S_NONE)
	,_socket(0)
	,_host("")
	,_port(0)
	,_protocol(PROTOCOL_TCP)
	,_type(SOCKET_TYPE_INVALID)
{
	_ping[0] = 0;
	_ping[1] = 0;
}


PackageSocket::~PackageSocket()
{
}

int PackageSocket::ps_connect(std::string addr, uint16_t port) {
	_host = addr;
	_port = port;
	_protocol = PROTOCOL_TCP;
	_type = SOCKET_TYPE_RESERVE;
	_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_state != S_SOCK_CREATED) {
	
		return false;
	int iRet = 0;
	sockaddr_in SocketAddr;
	memset(&SocketAddr, 0, sizeof(SocketAddr));
	if (_host.length() < 1 || _port == 0) goto _failed;
	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_port = htons(_port);
	SocketAddr.sin_addr.s_addr = inet_addr(_host.c_str());
	memset(&(SocketAddr.sin_zero), 0, sizeof(SocketAddr.sin_zero));

	// connecting.
	_state = S_SYN;
	iRet = connect(_socket, (struct sockaddr*)&SocketAddr, sizeof(SocketAddr));
	if (iRet == -1)
	{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
		closesocket(_socket);
#else
		close(_socket);
#endif
		goto _failed;
	}
	_state = S_ESTABLISHED;
	return SOCKET_OPEN;
_failed:
	return SOCKET_ERROR;
}

int PackageSocket::ps_start() {

}

int PackageSocket::ps_close() {

}

int PackageSocket::ps_poll(netpack *pack, int *more) {

}

void PackageSocket::ps_sendMsg(netpack *pack)
{

}

void PackageSocket::ps_sendPing() {
	if (_state == S_ESTABLISHED) {
		cocos2d::log("send ping.");
		char head[2] = { 0 };
		int iRet = send(_socket, head, 2, 0);
		if (iRet == -1) {
			// handle error.
			_state = S_ERROR;
		}
	}
}

void ps_set_type(int type) {
	_type = type;
}

int PackageSocket::ps_get_type() {
	return _type;
}