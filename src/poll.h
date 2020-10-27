#ifndef POLL_H
#define POLL_H
/*=========================================================================*\
* Poll implementation
* LuaSocket toolkit
*
* Each object that can be passed to the poll function has to export
* method getfd() which returns the descriptor to be passed to the
* underlying poll function. Another method, dirty(), should return
* true if there is data ready for reading (required for buffered input).
\*=========================================================================*/

#ifndef _WIN32
#pragma GCC visibility push(hidden)
#endif

int poll_open(lua_State *L);

#ifndef _WIN32
#pragma GCC visibility pop
#endif

#endif /* POLL_H */
