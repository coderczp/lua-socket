#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


static int 
lconnect(lua_State *L) {
	// const char *addr = luaL_checkstring(L, 1);
	// uint16_t port = luaL_checkinteger(L, 2);
	// int ht = luaL_checkinteger(L, 2);
	// PackageSocket *sock = new PackageSocket();
	// sock->ps_connect(addr, port);
	// sock->ps_set_type(ht);
	// lua_pushlightuserdata(L, sock);
	return 1;
}

static int 
lsend(lua_State *L) {
	// PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	// size_t len;
	// const char *buffer = luaL_checklstring(L, 2, &len);
	// struct  netbuffer nb;
	// nb->src = buffer;
	// nb->len = len;
	// sock->ps_sendMsg(&nb);
	// lua_pushboolean(L, 1);
	return 1;
}

static int
lpoll(lua_State *L) {
	// PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	// netpack *pack = malloc(sizeof(*pack));
	// int more = 0;
	// sock->ps_poll(pack, &more)
	// lua_pushlstring(L, pack->src, pack->len);
	return 1;
}

static int 
lclose(lua_State *L) {
	// PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	// sock->close();
	// delete sock;
	// lua_pushboolean(L, 1)
	return 1;
}

static int 
ltest1(lua_State *L) {
	if (lua_type(L, 1) == LUA_TTABLE) {
		if (lua_getfield(L, 1, "vabc")) {
			printf("%s\n", "abc");	
			if (lua_getfield(L, -1, "mabc")) {
				const char * str = lua_tostring(L, -1);
				printf("%s\n", str);
			} else {
				printf("%s\n", "falses");
			}
		}
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
		{ "connect", lconnect },
		{ "send", lsend },
		{ "poll", lpoll },
		{ "close", lclose },
		{ "test1", ltest1 },
		{ "test2", ltest2 },
		{ "test3", ltest3 },
		{ "udp", ludp},
		{ "udp_send", ludp_send},
		{ "udp_poll", ludp_poll},
		{ "udp_close", ludp_close},
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	return 1;
}