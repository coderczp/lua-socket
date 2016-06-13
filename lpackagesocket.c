#if WIN32
#include <Winsock2.h>
#include <Wininet.h>
#include <ws2tcpip.h>
#include <Windows.h>
//#pragma comment (lib, "Ws2_32.lib")

#else
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/timeb.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>


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

#define UDP_ADDRESS_SIZE 19	// ipv6 128bit + port 16bit + 1 byte type

#define MALLOC malloc
#define FREE   free


static int 
init_lib(lua_State *L) {
#if WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		//lua_error(L);
		luaL_error(L, "init win32 failture.");
	}
#endif // WIN32
	return 1;
}

static int
close_lib(lua_State *L) {
#if WIN32
	WSACleanup();
#endif // WIN32
	return 1;
}

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
write_buffer_free(struct write_buffer *wb) {
	if (wb->userobject) {
		//ss->soi.free(wb->buffer);
	} else {
		FREE(wb->buffer);
	}
	FREE(wb);
}

static inline void
free_wb_list(struct wb_list *list) {
	struct write_buffer *tmp = list->head;
	list->head = list->head->next;
	while (tmp) {
		write_buffer_free(tmp);
		struct write_buffer *tmp = list->head;
		list->head = list->head->next;
	}
}

static inline void
clear_wb_list(struct wb_list *list) {
	list->head = NULL;
	list->tail = NULL;
}

typedef struct lua_socket {
	struct fd_set   fds;
	uint32_t        socket;
	char            host[32];
	uint16_t        port;
	uint16_t        protocol;
	uint16_t        type;
	char            ping[2];
	struct wb_list *wl;
} lua_socket;

static int
lnew(lua_State *L) {
	init_lib(L);
	struct lua_socket *so = (struct lua_socket*)malloc(sizeof(*so));
	so->protocol = PROTOCOL_TCP;
	so->type = SOCKET_TYPE_RESERVE;
	so->socket = socket(AF_INET, SOCK_STREAM, 0);
	so->wl = malloc(sizeof(*so->wl));
	clear_wb_list(so->wl);
	lua_pushlightuserdata(L, so);
	return 1;
}

static int
lconnect(lua_State *L) {
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 1);
	size_t sz;
	const char *addr = luaL_checklstring(L, 2, &sz);
	uint16_t port = luaL_checkinteger(L, 3);
	memcpy(so->host, addr, sz);
	so->port = port;

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(addr);
	int r = connect(so->socket, (const struct sockaddr*)&sa, sizeof(sa));
	if (r == -1) {
#if WIN32
		closesocket(so->socket);
#else
		close(so->socket);
#endif
		goto _failed;
	}
	lua_pushinteger(L, SOCKET_OPEN);
	return 1;
_failed:
	lua_pushinteger(L, SOCKET_ERROR);
	return 1;
}

static int
send_list_tcp(struct lua_socket * s, struct wb_list *list) {
	if (list->head) {
		struct write_buffer * tmp = list->head;
		int sz = send(s->socket, tmp->ptr, tmp->sz, 0);
		if (sz < 0) {
			int werrno = 0;
#if WIN32
			werrno = WSAGetLastError();
#else
			switch (errno) {
			case EINTR:
				return
			case AGAIN_WOULDBLOCK:
				return -1;
			}
#endif // WIN32
		} else {
			if (sz != tmp->sz) {
				tmp->ptr += sz;
				tmp->sz -= sz;
			} else {
				if (list->tail == tmp) {
					list->tail = NULL;
				}
				list->head = tmp->next;
				write_buffer_free(tmp);
			}
		}
	}
	return SOCKET_DATA;
}

static int
append_sendbuffer(struct lua_socket * s, struct write_buffer *wb) {
	s->wl->tail->next = wb;
	wb->next = NULL;
	s->wl->tail = wb;
	return 1;
}

static int
lsend(lua_State *L) {
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 1);
	size_t sz;
	const char *addr = luaL_checklstring(L, 2, &sz);
	struct write_buffer *wb = (struct write_buffer *)MALLOC(sizeof(*wb));
	wb->buffer = (void*)addr;
	wb->ptr = (char*)addr;
	wb->sz = sz;
	append_sendbuffer(so, wb);
	if (so->protocol == PROTOCOL_TCP) {
		int r = send_list_tcp(so, so->wl);
		lua_pushinteger(L, r);
		return 1;
	} else {
		lua_pushinteger(L, SOCKET_ERROR);
	}
	return 1;
}

static int
lpoll(lua_State *L) {
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 1);
	send_list_tcp(so, so->wl);
	FD_ZERO(&so->fds);
	FD_SET(so->socket, &so->fds);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int r = select(so->socket + 1, &so->fds, NULL, NULL, &timeout);
	if (r > 0) {
		if (FD_ISSET(so->socket, &so->fds)) {
			char tmp[512] = { 0 };
			int r = recv(so->socket, (char *)tmp, 512, 0);
			if (r == -1) {

			} else if (r == 0) {
#if WIN32
				closesocket(so->socket);
#else
				close(so->socket);
#endif // WIN32
				free_wb_list(so->wl);
				FREE(so);
				close_lib(L);
				lua_pushinteger(L, SOCKET_EXIT);
				return 1;
			} else if (r > 0) {
				lua_pushinteger(L, SOCKET_DATA);
				lua_pushlstring(L, tmp, r);
				return 2;
			}
		}
	} else {
		lua_pushinteger(L, SOCKET_DATA);
		lua_pushstring(L, "");
		return 2;
	}
	return 1;
}

static int
lclose(lua_State *L) {
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 1);
#if WIN32
	closesocket(so->socket);
#else
	close(so->socket);
#endif // WIN32
	free_wb_list(so->wl);
	FREE(so);
	close_lib(L);
	lua_pushinteger(L, SOCKET_EXIT);
	return 1;
}

static int
lkeepalive(lua_State *L) {
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 1);
	//setsockopt(so->so)
	return 0;
}

static int
ltest1(lua_State *L) {
	if (lua_type(L, 1) == LUA_TTABLE) {
		/*if (lua_getfield(L, 1, "vabc")) {
			printf("%s\n", "abc");
			if (lua_getfield(L, -1, "mabc")) {
				const char * str = lua_tostring(L, -1);
				printf("%s\n", str);
			} else {
				printf("%s\n", "falses");
			}
		}*/
		// if (lua_type(L, -1) == LUA_TTABLE) {
		// 	if (lua_getfield(L, -1, "kabcd")) {
		// 		const char * str = lua_tostring(L, -1);
		// 		printf("%s\n", str);
		// 	} else {
		// 		printf("%s\n", "falses");
		// 	}


			// lua_pushnil(L);  /* first key */
   //   		while (lua_next(L, -2) != 0) {
		 //       /* uses 'key' (at index -2) and 'value' (at index -1) */
		 //       printf("%s - %s\n",
		 //              lua_typename(L, lua_type(L, -2)),
		 //              lua_typename(L, lua_type(L, -1)));
		 //       /* removes 'value'; keeps 'key' for next iteration */
		 //       lua_pop(L, 1);
		 //     }

			// if (lua_getfield(L, -1, "mabc")) {
			// 	const char * str = lua_tostring(L, -1);
			// 	printf("%s\n", str);
			// }
			// if (lua_getmetatable(L, -1) == 1) {
			// 	printf("%s\n", "ok");
			// } else {
			// 	printf("%s\n", "false");
			// }
			// lua_getfield(L, 1, "mabc");
			// const char *str = lua_tostring(L, -1);
			// printf("%s\n", str);
		// }
		// lua_getfield(L, -1, "test");
		// lua_pcall(L, 0, 0, 0);
	}
	return 0;
}

// static int 
// test(lua_State *L) {
// 	const char * str = lua_tostring(L, lua_upvalueindex(1));
// 	printf("%s\n", str);
// 	return 0;
// }

static int
ltest2(lua_State *L) {
	if (lua_type(L, 1) == LUA_TTABLE) {
		lua_pushstring(L, "vabc");

		lua_newtable(L);
		lua_pushstring(L, "kkabcd");
		lua_pushstring(L, "vvabcd");
		lua_rawset(L, -3);

		// new metatable
		if (luaL_newmetatable(L, "TTEST")) {
			// // new table 
			lua_newtable(L);
			lua_pushstring(L, "mabc");
			lua_pushstring(L, "mabcd");
			lua_rawset(L, -3);
			lua_setfield(L, -2, "__index");
		}

		lua_setmetatable(L, -2);


		// // new cc
		// lua_pushcclosure(L, test, 2);
		// lua_pushstring(L, "uvabc");
		// lua_setupvalue(L, -2, 1);
		// lua_pushstring(L, "uvabcd");
		// lua_setupvalue(L, -2, 2);

		// lua_setfield(L, -2, "test");


		// lua_pop(L, 1);

		// if (luaL_getmetatable(L, "test") == 1) {
		// 	printf("%s\n", "meta ok");
		// } else {
		// 	printf("%s\n", "meta false");
		// }
		// luaL_setmetatable(L, "test");


		lua_rawset(L, 1);
		lua_pushstring(L, "kabcd");
		lua_pushstring(L, "vabcd");
		lua_rawset(L, 1);
	}
	return 0;
}

static int
ltest3(lua_State *L) {
	lua_createtable(L, 0, 2);
	return 1;
}

struct packagesocketudp
{
	int fd;
	struct sockaddr local;
	struct sockaddr remote;
	char buffer[1024];
	int len;
};

static int
ludp(lua_State *L) {
	struct packagesocketudp *ps = malloc(sizeof(*ps));
	ps->fd = 0;
	memset(&ps->local, 0, sizeof(ps->local));
	memset(&ps->remote, 0, sizeof(ps->remote));
	memset(&ps->buffer, 0, 1024);
	ps->len = 1024;

	int t = luaL_checkinteger(L, 1);
	if (t == 1) {
		size_t sz = 0;
		const char *laddr = luaL_checklstring(L, 2, &sz);
		int lport = luaL_checkinteger(L, 3);	
		struct sockaddr_in * local = (struct sockaddr_in *)&ps->local;	
		(local)->sin_family = AF_INET;
		(local)->sin_port = htons(lport);	
		inet_pton(AF_INET, laddr, &local->sin_addr);	
		
		const char *raddr = luaL_checklstring(L, 4, &sz);
		int rport = luaL_checkinteger(L, 5);

		struct sockaddr_in * remote = (struct sockaddr_in *)&ps->remote;
		(remote)->sin_family = AF_INET;
		(remote)->sin_port = htons(rport);
		inet_pton(AF_INET, laddr, &remote->sin_addr);

		int fd = socket(PF_INET, SOCK_DGRAM, 0);
		ps->fd = fd;
	} else if (t == 2) {
		size_t sz = 0;
		const char *laddr = luaL_checklstring(L, 2, &sz);
		int lport = luaL_checkinteger(L, 3);
		struct sockaddr_in6 *local = (struct sockaddr_in6 *)&ps->local;
		local->sin_family = AF_INET6;
		local->sin_port = htons(lport);
		inet_pton(AF_INET6, laddr, &local->sin6_addr)
		
	}
	if strlen(laddr) 

	int r = bind(fd, &ps->local, sizeof(ps->local));

	lua_pushlightuserdata(L, (void*)ps);
	return 1;
}

static int
ludp_connect(lua_State *L) {
	struct packagesocketudp *ps  = (struct packagesocketudp *)lua_touserdata(L, 1);
}

static int
ludp_send(lua_State *L) {
	struct packagesocketudp *ps  = (struct packagesocketudp *)lua_touserdata(L, 1);
	size_t sz = 0;
	const char * buffer = luaL_checklstring(L, 2, &sz);
	sendto(ps->fd, buffer, sz, 0, &ps->remote, sizeof(ps->remote));
	return 0;
}

static int
ludp_poll(lua_State *L) {
	struct packagesocketudp *ps  = (struct packagesocketudp *)lua_touserdata(L, 1);
	int r = recvfrom(ps->fd, ps->buffer, 256, 0, &ps->remote, sizeof(ps->remote));
	if (r == -1) {
		if (errno == EAGAIN || errno == EINTR) {
		}
		lua_pushinteger(L, r);
		return 1;
	} else {
		lua_pushstring(L, ps->buffer);
		lua_pushinteger(L, r);
		return 2;
	}
}

ludp_close(lua_State *L) {
	struct packagesocketudp *ps  = (struct packagesocketudp *)lua_touserdata(L, 1);
	close(ps->fd);
	free(ps);
}

int
luaopen_packagesocket(lua_State *L) {
	luaL_Reg l[] = {
		{ "new", lnew },
		{ "connect", lconnect },
		{ "send", lsend },
		{ "poll", lpoll },
		{ "close", lclose },
		{ "keepalive", lkeepalive },
		{ "test1", ltest1 },
		{ "test2", ltest2 },
		{ "test3", ltest3 },
		{ "udp", ludp},
		{ "udp_send", ludp_send},
		{ "udp_poll", ludp_poll},
		{ "udp_close", ludp_close},
		{ NULL, NULL },
	};
	luaL_openlib(L, "packagesocket", l, 0);
	lua_pushstring(L, "SOCKET_DATA");
	lua_pushinteger(L, SOCKET_DATA);
	lua_rawset(L, -3);
	lua_pushstring(L, "SOCKET_CLOSE");
	lua_pushinteger(L, SOCKET_CLOSE);
	lua_rawset(L, -3);

	lua_pushstring(L, "SOCKET_OPEN");
	lua_pushinteger(L, SOCKET_OPEN);
	lua_rawset(L, -3);

	lua_pushstring(L, "SOCKET_ACCEPT");
	lua_pushinteger(L, SOCKET_ACCEPT);
	lua_rawset(L, -3);

	lua_pushstring(L, "SOCKET_ERROR");
	lua_pushinteger(L, SOCKET_ERROR);
	lua_rawset(L, -3);

	lua_pushstring(L, "SOCKET_EXIT");
	lua_pushinteger(L, SOCKET_EXIT);
	lua_rawset(L, -3);

	lua_pushstring(L, "SOCKET_UDP");
	lua_pushinteger(L, SOCKET_UDP);
	lua_rawset(L, -3);
	return 1;
}