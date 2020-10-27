/*=========================================================================*\
* Poll implementation
* LuaSocket toolkit
\*=========================================================================*/
#include "luasocket.h"

#include <stdlib.h>
#include <string.h>

#include "socket.h"
#include "poll.h"

/*=========================================================================*\
* Internal function prototypes.
\*=========================================================================*/
static t_socket getfd(lua_State *L);
static void unpackpfd(lua_State *L, t_pollfd* pfd);
static int eventbit(char const *str);
static int unpackevents(lua_State *L);
static void packevents(lua_State *L, int events);
static int global_poll(lua_State *L);

/* functions in library namespace */
static luaL_Reg func[] = {
    {"poll", global_poll},
    {NULL,     NULL}
};

/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
int poll_open(lua_State *L) {
    luaL_setfuncs(L, func, 0);
    return 0;
}

/*=========================================================================*\
* Global Lua functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Waits for a set of descriptors for an event or timeout.
\*-------------------------------------------------------------------------*/
static int global_poll(lua_State *L) {
    int k, t, res, nfds, timeout;
    t_pollfd *pfds = NULL;

    luaL_checktype(L, 1, LUA_TTABLE);
    timeout = luaL_optnumber(L, 2, -1);

    /* first iterate to count and validate input data */
    nfds = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        ++nfds;
        if (!lua_isnumber(L, -2) || lua_tointeger(L, -2) != nfds) {
            luaL_argerror(L, 1, "table is not an array");
        }

        if (lua_type(L, -1) != LUA_TTABLE) {
            luaL_argerror(L, 1, "item is not a table");
        }

        lua_pushstring(L, "socket");
        lua_gettable(L, -2);
        if (getfd(L) == SOCKET_INVALID) {
            luaL_argerror(L, 1, "invalid socket");
        }
        lua_pop(L, 1);

        lua_pushstring(L, "events");
        lua_gettable(L, -2);
        t = lua_type(L, -1);
        if (t != LUA_TTABLE && t != LUA_TNIL) {
            luaL_argerror(L, 1, "argument format incorrect");
        }
        lua_pop(L, 2);
    }

    if (nfds == 0) {
        lua_createtable(L, 0, 0);
        return 1;
    }

    pfds = malloc(sizeof(t_pollfd) * nfds);
    if (!pfds) {
        luaL_error(L, "malloc failure");
    }

    /* now iterate again to build the descriptor list */
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        unpackpfd(L, &pfds[lua_tointeger(L, -2) - 1]);
        lua_pop(L, 1);
    }

    res = socket_poll(pfds, (t_nfds) nfds, timeout);

    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "poll failed");

        free(pfds);
        return 2;
    }

    if (res == 0) {
        lua_pushnil(L);
        lua_pushstring(L, "timeout");

        free(pfds);
        return 2;
    }

    lua_createtable(L, res, 0);
    t = 1;
    for (k = 0; k < nfds; ++k) {
        if (pfds[k].revents) {
            lua_pushnumber(L, k + 1);
            lua_pushnumber(L, k + 1);
            lua_gettable(L, 1);

            packevents(L, pfds[k].revents);
            lua_settable(L, -3);
            ++t;
        }
    }

    free(pfds);
    return 1;
}

/*=========================================================================*\
* Internal functions
\*=========================================================================*/
static t_socket getfd(lua_State *L) {
    t_socket fd = SOCKET_INVALID;
    lua_pushstring(L, "getfd");
    lua_gettable(L, -2);
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, -2);
        lua_call(L, 1, 1);
        if (lua_isnumber(L, -1)) {
            double numfd = lua_tonumber(L, -1);
            fd = (numfd >= 0.0)? (t_socket) numfd: SOCKET_INVALID;
        }
    }
    lua_pop(L, 1);
    return fd;
}

static void unpackpfd(lua_State *L, t_pollfd* pfd) {
    pfd->events = 0;
    pfd->revents = 0;

    lua_pushstring(L, "socket");
    lua_gettable(L, -2);
    pfd->fd = getfd(L);
    lua_pop(L, 1);

    lua_pushstring(L, "events");
    lua_gettable(L, -2);
    pfd->events = unpackevents(L);
    lua_pop(L, 1);
}

static struct {
    int bits;
    char const * name;
} const eventbits[] = {
    {POLLIN, "POLLIN"},
    {POLLOUT, "POLLOUT"},
    {POLLPRI, "POLLPRI"},
    {POLLERR, "POLLERR"},
    {POLLHUP, "POLLHUP"},
    {POLLNVAL, "POLLNVAL"},
};

static int eventbit(char const *str) {
    for (unsigned i = 0; i < sizeof(eventbits)/sizeof(eventbits[0]); ++i) {
        if (strcmp(str, eventbits[i].name) == 0) {
            return eventbits[i].bits;
        }
    }

    return 0;
}

static int unpackevents(lua_State *L) {
    int events = 0;

    if (lua_isnil(L, -1)) {
        return POLLIN;
    }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_isstring(L, -1)) {
            events = events | eventbit(lua_tostring(L, -1));
        } else if (lua_toboolean(L, -1) == 1 && lua_isstring(L, -2)) {
            events = events | eventbit(lua_tostring(L, -2));
        }

        lua_pop(L, 1);
    }

    return events;
}

static void packevents(lua_State *L, int events) {
    int bits;

    lua_pushstring(L, "revents");
    lua_createtable(L, 2, 0);

    for (unsigned i = 0; i < sizeof(eventbits)/sizeof(eventbits[0]); ++i) {
        bits = eventbits[i].bits;
        if ((bits & events) == bits) {
            lua_pushstring(L, eventbits[i].name);
            lua_pushboolean(L, 1);
            lua_settable(L, -3);
        }
    }

    lua_settable(L, -3);
}
