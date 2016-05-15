/*
** 1. 链接，connect函数，
** 2. 开始监听回收，recv函数
** 3. 发送
** 4. 发送封包
** 5. 接受封包
*/

#ifndef __PACKAGESOCKET_H_
#define __PACKAGESOCKET_H_
#include <stdint.h>
#include <string>

#define SOCKET_DATA 0
#define SOCKET_CLOSE 1
#define SOCKET_OPEN 2
#define SOCKET_ACCEPT 3
#define SOCKET_ERROR 4
#define SOCKET_EXIT 5
#define SOCKET_UDP 6

#define SOCKET_TYPE_INVALID 0
#define SOCKET_TYPE_RESERVE 1
#define SOCKET_TYPE_PLISTEN 2
#define SOCKET_TYPE_LISTEN 3
#define SOCKET_TYPE_CONNECTING 4
#define SOCKET_TYPE_CONNECTED 5
#define SOCKET_TYPE_HALFCLOSE 6
#define SOCKET_TYPE_PACCEPT 7
#define SOCKET_TYPE_BIND 8

#define PROTOCOL_TCP 0
#define PROTOCOL_UDP 1
#define PROTOCOL_UDPv6 2

struct netpack
{
	char *src;
	int len;
};

class PackageSocket
{
	enum STATE
	{
		S_NONE,
		S_SOCK_CREATED,
		S_SYN,
		S_ESTABLISHED,
		S_STOP,
		S_ERROR
	};

public:
	PackageSocket();
	~PackageSocket();

	int ps_connect(std::string addr, uint16_t port);
	int ps_start();
	int ps_close();
	int ps_poll(netpack *pack, int *more);
	int ps_sendMsg(netpack *pack);
	int ps_sendPing();

	void ps_set_type(int type);
	int ps_get_type();
private:

	friend class Gate;

	STATE       _state;
	uint32_t    _socket;
	std::string _host;
	uint16_t    _port;
	uint16_t    _protocol; 
	uint16_t    _type;    
	char        _ping[2];
};

#endif // !__PACKAGESOCKET_H_
