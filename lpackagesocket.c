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
#include <netdb.h>
#include <sys/select.h>
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

#define HEADER_LINE 0
#define HEADER_PG 1

#define UDP_ADDRESS_SIZE 19	// ipv6 128bit + port 16bit + 1 byte type

#define MALLOC malloc
#define FREE   free

#define COMPAT_LUA

static void
init_lib() {
#if WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		//lua_error(L);
		//luaL_error(L, "init win32 failture.");
	}
#endif // WIN32
}

static void
close_lib() {
#if WIN32
	WSACleanup();
#endif // WIN32
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
	struct write_buffer *first = list->head;
	while (first) {
		struct write_buffer *tmp = first;
		first = first->next;
		write_buffer_free(tmp);
	}
}

static inline void
clear_wb_list(struct wb_list *list) {
	list->head = NULL;
	list->tail = NULL;
}

typedef struct lua_socket {
	struct lua_socket *next;
	uint32_t        fd;
	uint16_t        protocol;
	uint16_t        type;
	char            ping[2];
	int             header;
	struct wb_list *wl;
	struct sockaddr local;
	struct sockaddr remote;
	char            buffer[1024];
	int len;
	char           *bhead;
	char           *btail;
} lua_socket;

typedef struct lua_gate
{
	fd_set             fds;
	struct lua_socket *head;
	struct lua_socket *tail;
} lua_gate;


static int  
colse_sock(struct lua_gate *g, struct lua_socket *so) {
	struct lua_socket *ptr = g->head;
	while (ptr) {
		if (ptr == so) {
			ptr = ptr->next;
		}
		if (g->head == so) {
			g->head = ptr;
		}
	}
	free_wb_list(so->wl);
#if WIN32
	closesocket(so->fd);
#else
	close(so->fd);
#endif // WIN32
	FREE(so);
	return 1;
}

static int
lnew(lua_State *L) {
	init_lib();
	struct lua_gate *g = (struct lua_gate *)MALLOC(sizeof(*g));
	g->head = NULL;
	g->tail = NULL;
	lua_pushlightuserdata(L, g);
	return 1;
}

static int
lsocket(lua_State *L) {
	struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	struct lua_socket *so = (struct lua_socket*)MALLOC(sizeof(*so));
	so->protocol = luaL_checkinteger(L, 2);
	so->type = luaL_checkinteger(L, 3);
	so->header = luaL_checkinteger(L, 4);
	if (so->protocol == PROTOCOL_TCP) {
		so->fd = socket(AF_INET, SOCK_STREAM, 0);
	} else if (so->protocol == PROTOCOL_UDP) {
		so->fd = socket(AF_INET, SOCK_DGRAM, 0);
		// addrinfo
		// getaddrinfo
		size_t sz = 0;
		const char *laddr = luaL_checklstring(L, 5, &sz);
		int lport = luaL_checkinteger(L, 6);	
		struct sockaddr_in * local = (struct sockaddr_in *)&so->local;
		(local)->sin_family = AF_INET;
		(local)->sin_port = htons(lport);	
		inet_pton(AF_INET, laddr, &local->sin_addr);
	} else if (so->protocol == PROTOCOL_UDPv6) {
		so->fd = socket(AF_INET6, SOCK_DGRAM, 0);
		size_t sz = 0;
		const char *laddr = luaL_checklstring(L, 5, &sz);
		int lport = luaL_checkinteger(L, 6);
		struct sockaddr_in6 *local = (struct sockaddr_in6 *)&so->local;
		(local)->sin6_family = AF_INET6;
		(local)->sin6_port = htons(lport);	
		inet_pton(AF_INET6, laddr, &local->sin6_addr);
	}
	so->wl = (struct wb_list *)MALLOC(sizeof(*so->wl));
	clear_wb_list(so->wl);
	so->ping[0] = 0;
	so->ping[1] = 0;
	memset(so->buffer, 0, 1024);
	so->len = 1024;
	so->bhead = so->buffer;
	so->btail = so->buffer;
	so->next = NULL;
	if (g->tail == NULL) {
		g->tail = so;
	} else {
		g->tail->next = so;
		g->tail = so;
	}
	if (g->head == NULL) {
		g->head = so;
	}
	lua_pushlightuserdata(L, so);
	return 1;
}

static int
lconnect(lua_State *L) {
	// struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 2);
	size_t sz;
	const char *addr = luaL_checklstring(L, 3, &sz);
	uint16_t port = luaL_checkinteger(L, 4);
	if (so->protocol == PROTOCOL_TCP) {
		struct sockaddr_in * remote = (struct sockaddr_in *)&so->remote;
		remote->sin_family = AF_INET;
		remote->sin_port = htons(port);	
		inet_pton(AF_INET, addr, &remote->sin_addr);
		int res = connect(so->fd, (const struct sockaddr*)&so->remote, sizeof(so->remote));
		if (res == -1) {
	#if WIN32
			closesocket(so->fd);
	#else
			close(so->fd);
	#endif
			goto _failed;
		}
	} else if (so->protocol == PROTOCOL_UDP) {
		struct sockaddr_in *remote = (struct sockaddr_in *)&so->remote;
		remote->sin_family = AF_INET;
		remote->sin_port = htons(port);
		inet_pton(AF_INET, addr, &remote->sin_addr);
	} else if (so->protocol == PROTOCOL_UDPv6) {
		struct sockaddr_in6 *remote = (struct sockaddr_in6 *)&so->remote;
		remote->sin6_family = AF_INET6;
		remote->sin6_port = htons(port);	
		inet_pton(AF_INET6, addr, &remote->sin6_addr);
	}
	lua_pushinteger(L, SOCKET_OPEN);
	return 1;
_failed:
	lua_pushinteger(L, SOCKET_ERROR);
	return 1;
}

static int
send_list(struct lua_socket *so, struct wb_list *list) {
	struct write_buffer *tmp = list->head;
	if (tmp) {
		if (so->protocol == PROTOCOL_TCP) {
			int sz = send(so->fd, tmp->ptr, tmp->sz, 0);
			if (sz == -1) {
				#if WIN32 
				int e = WSAGetLastError();
				if (e == WSAEINTR || e == WSAEINPROGRESS) {
					return SOCKET_DATA;
				}
				#else
				if (errno == EINTR) {
					return SOCKET_DATA;
				}
				#endif
			} else {
				if (sz != tmp->sz) {
					tmp->ptr += sz;
					tmp->sz -= sz;
				} else {
					list->head = tmp->next;
					write_buffer_free(tmp);
				}
			}
		} else if (so->protocol == PROTOCOL_UDP) {
			int sz = sendto(so->fd, tmp->ptr, tmp->sz, 0, &so->remote, sizeof(so->remote));
			if (sz == -1) {
#if WIN32
				int e = WSAGetLastError();
				if (e == WSAEINTR || e == WSAEINPROGRESS) {
					return SOCKET_DATA;
				} else {
					return SOCKET_ERROR;
				}
#else

#endif // WIN32
			} else {
				if (sz != tmp->sz) {
					tmp->ptr += sz;
					tmp->sz -= sz;
				} else {
					list->head = tmp->next;
					write_buffer_free(tmp);
				}	
			}
		} else if (so->protocol == PROTOCOL_UDPv6) {
			int sz = sendto(so->fd, tmp->ptr, tmp->sz, 0, &so->remote, sizeof(so->remote));
			if (sz == -1) {

			} else {
				if (sz != tmp->sz) {
					tmp->ptr += sz;
					tmp->sz -= sz;
				} else {
					list->head = tmp->next;
					write_buffer_free(tmp);
				}	
			}
		}
	} else {
	}
	return SOCKET_DATA;
}

static int
append_sendbuffer(struct lua_socket * so, struct write_buffer *wb) {
	so->wl->tail->next = wb;
	wb->next = NULL;
	so->wl->tail = wb;
	return 1;
}

static int
lsend(lua_State *L) {
	// struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 2);
	assert(so->header == HEADER_PG);
	size_t sz;
	const char *addr = luaL_checklstring(L, 3, &sz);
	char *buffer = (char *)MALLOC(sz + 2);
	buffer[0] = sz & 0xff;
	buffer[1] = (sz >> 8) & 0xff;
	memcpy(buffer+2, addr, sz);
	struct write_buffer *wb = (struct write_buffer *)MALLOC(sizeof(*wb));
	wb->buffer = buffer;
	wb->ptr = buffer;
	wb->sz = sz + 2;
	wb->next = NULL;
	wb->userobject = false;
	append_sendbuffer(so, wb);
	int r = send_list(so, so->wl);
	lua_pushinteger(L, r);
	return 1;
}

static int 
lsendline(lua_State *L) {
	// struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 2);
	assert(so->header == HEADER_LINE);
	size_t sz;
	const char *addr = luaL_checklstring(L, 3, &sz);
	char *buffer = (char *)MALLOC(sz + 1);
	memcpy(buffer, addr, sz);
	buffer[sz] = '\n';
	struct write_buffer *wb = (struct write_buffer *)MALLOC(sizeof(*wb));
	wb->buffer = buffer;
	wb->ptr = buffer;
	wb->sz = sz + 2;
	wb->next = NULL;
	wb->userobject = false;
	append_sendbuffer(so, wb);
	int r = send_list(so, so->wl);
	lua_pushinteger(L, r);
	return 1;	
}

static int 
readc(lua_State *L, struct lua_gate *g, struct lua_socket *so) {
	if (so->bhead == so->btail) {
		return 1;
	}
	if (so->header == HEADER_PG) {
		int h = so->buffer[0];
		h = h & so->buffer[1] << 8;
		if (so->btail - so->bhead >= h+ 2) {
			lua_pushlightuserdata(L, so);		
			lua_pushlstring(L, so->bhead+2, h);
			lua_rawset(L, -3);
			so->bhead = so->bhead + h + 2;
		}
	} else if (so->header == HEADER_LINE) {
		char *p = so->bhead;
		while (p != so->btail) {
			if ((*p) == '\n') {
				lua_pushlightuserdata(L, so);
				lua_pushlstring(L, so->bhead, p - so->bhead);
				lua_rawset(L, -3);
				so->bhead = p + 1;
			} else {
				p++;
			}
		}
	}
	return 1;
}

static int 
rebase(lua_State *L, struct lua_gate *g, struct lua_socket *so) {
	if (so->len - (so->buffer - so->btail) <= 40) {
		int count = so->bhead - so->btail;
		memmove(so->buffer, so->bhead, count);
		so->bhead = so->buffer;
		so->btail = so->buffer + count;
	}
	return 1;
}

static int
lpoll(lua_State *L) {
	struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	lua_newtable(L);
	FD_ZERO(&g->fds);
	uint32_t max = 0;
	struct lua_socket *ptr = g->head;
	while (ptr) {
		if (ptr->fd > max) 
			max = ptr->fd;
		FD_SET(ptr->fd, &g->fds);
		ptr = ptr->next;
	}
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int r = select(max + 1, &g->fds, NULL, NULL, &timeout);
	if (r > 0) {
		ptr = g->head;
		while (ptr) {
			send_list(ptr, ptr->wl);		
			if (FD_ISSET(ptr->fd, &g->fds)) {
				rebase(L, g, ptr);
				if (ptr->protocol == PROTOCOL_TCP) {
					int res = recv(ptr->fd, ptr->btail, ptr->len - (ptr->btail - ptr->buffer + 1), 0);
					if (res == -1) {
						// error

					} else if (res == 0) {
						// 
						colse_sock(g, ptr);
					} else if (res > 0) {
						ptr->btail = ptr->btail + res;
						readc(L, g, ptr);
					}
				} else if (ptr->protocol == PROTOCOL_UDP) {
					socklen_t len = sizeof(ptr->remote);
					int res = recvfrom(ptr->fd, ptr->btail, ptr->len - (ptr->btail - ptr->buffer + 1), 0, &ptr->remote, &len);
					if (res > 0) {
						ptr->btail = ptr->btail + res;
						readc(L, g, ptr);
					}
				} else if (ptr->protocol == PROTOCOL_UDPv6) {
					socklen_t len = sizeof(ptr->remote);
					int res = recvfrom(ptr->fd, ptr->btail, ptr->len - (ptr->btail - ptr->buffer + 1), 0, &ptr->remote, &len);
					if (res > 0) {
						ptr->btail = ptr->btail + res;
						readc(L, g, ptr);
					}
				}
			}
			ptr = ptr->next;
		}
	}
	lua_pushinteger(L, SOCKET_DATA);
	lua_gettable(L, -2);
	return 2;
}

static int
lclosesocket(lua_State *L) {
	struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 2);
	colse_sock(g, so);
	lua_pushinteger(L, SOCKET_EXIT);
	return 1;
}

static int
lkeepalive(lua_State *L) {
	// struct lua_gate *g = (struct lua_gate *)lua_touserdata(L, 1);
	// struct lua_socket * so = (struct lua_socket*)lua_touserdata(L, 2);
	//setsockopt(so->so)
	return 0;
}

static int 
lclose(lua_State *L) {
	close_lib();
	lua_pushinteger(L, SOCKET_EXIT);
	return 1;
}

int
luaopen_packagesocket(lua_State *L) {
	//luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", lnew },
		{ "socket", lsocket },
		{ "connect", lconnect },
		{ "send", lsend },
		{ "sendline", lsendline},
		{ "poll", lpoll },
		{ "closesocket", lclosesocket},
		{ "close", lclose },
		{ "keepalive", lkeepalive },
		{ NULL, NULL },
	};
#if LUA_VERSION_NUM < 503
	luaL_openlib(L, "packagesocket", l, 0);
#else
	luaL_newlib(L, l);
#endif
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

	lua_pushinteger(L, HEADER_LINE);
	lua_setfield(L, -2, "HEADER_LINE");
	lua_rawset(L, -3);
	lua_pushinteger(L, HEADER_PG);
	lua_setfield(L, -2, "HEADER_PG");
	lua_rawset(L, -3);

	lua_pushinteger(L, PROTOCOL_TCP);
	lua_setfield(L, -2, "PROTOCOL_TCP");
	lua_rawset(L, -3);
	lua_pushinteger(L, PROTOCOL_UDP);
	lua_setfield(L, -2, "PROTOCOL_UDP");
	lua_rawset(L, -3);
	lua_pushinteger(L, PROTOCOL_UDPv6);
	lua_setfield(L, -2, "PROTOCOL_UDPv6");
	lua_rawset(L, -3);
	return 1;
}