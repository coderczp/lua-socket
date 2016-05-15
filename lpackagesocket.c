#include "PackageSocket.h"
#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>

static int 
lconnect(lua_State *L) {
	const char *addr = luaL_checkstring(L, 1);
	uint16_t port = luaL_checkinteger(L, 2);
	int ht = luaL_checkinteger(L, 2);
	PackageSocket *sock = new PackageSocket();
	sock->ps_connect(addr, port);
	sock->ps_set_type(ht);
	lua_pushlightuserdata(L, sock);
	return 1
}

static int 
lsend(lua_State *L) {
	PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	int len;
	const char *buffer = luaL_checklstring(L, 2, &len);
	struct  netbuffer nb;
	nb->src = buffer;
	nb->len = len;
	sock->ps_sendMsg(&nb);
	lua_pushboolean(L, 1);
	return 1
}

static int
lpoll(lua_State *L) {
	PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	netpack *pack = malloc(sizeof(*pack));
	int more = 0;
	sock->ps_poll(pack, &more)
	lua_pushlightuserdata(L, pack->src);
	lua_pushinteger(L, pack->len);
	return 2;
}

static int 
lclose(lua_State *L) {
	PackageSocket *sock = (PackageSocket*)lua_touserdata(L, 1);
	sock->close();
	delete sock;
	lua_pushboolean(L, 1)
	return 1;
}

int
luaopen_packagesocket() {
	luaL_Reg l[] = {
		{ "connect", lconnect },
		{ "send", lsend },
		{ "poll", lpoll },
		{ "close", lclose },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	return 1;
}