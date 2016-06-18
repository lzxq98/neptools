#include "type_traits.hpp"
#include "../assert.hpp"
#include <cstring>

namespace Neptools
{
namespace Lua
{

char reftbl;

State::State(int) : StateRef{luaL_newstate()}
{
    if (!vm) NEPTOOLS_THROW(std::bad_alloc{});
}

// todo: do we need it?
int panic(lua_State* vm)
{
    size_t len;
    auto msg = lua_tolstring(vm, -1, &len);
    if (msg) NEPTOOLS_THROW(Error{{msg, len}});
    else NEPTOOLS_THROW(Error{"Lua PANIC"});
}

State::State() : State(0)
{
    Catch(
        [](StateRef vm)
        {
            lua_atpanic(vm, panic);
            luaL_openlibs(vm);

            // init reftable
            lua_newtable(vm);               // +1
            lua_createtable(vm, 0, 1);      // +2 metatbl
            lua_pushliteral(vm, "v");       // +3
            lua_setfield(vm, -2, "__mode"); // +2
            lua_setmetatable(vm, -2);       // +1
            lua_rawsetp(vm, LUA_REGISTRYINDEX, &reftbl); // 0

            // helper funs
            auto ret = luaL_loadstring(vm,
                "return{__call=function(t,...)return t.new(...)end}"); // +1
            NEPTOOLS_ASSERT(ret == 0);
            lua_call(vm, 0, 1); // +1
            lua_setfield(vm, LUA_REGISTRYINDEX, "neptools_new_mt"); // 0

            for (auto r : Registers())
                r(vm);
        }, *this);
}

State::~State()
{
    if (vm) lua_close(vm);
}

#ifdef _MSC_VER
int StateRef::SEHFilter(lua_State* vm, unsigned code)
{
    if ((code & 0xffffff00) != 0xe24c4a00)
        return EXCEPTION_CONTINUE_SEARCH;
    if (lua_gettop(vm) == 0 || !lua_isstring(vm, -1))
        return EXCEPTION_CONTINUE_SEARCH;

    error_msg = lua_tolstring(vm, -1, &error_len);
    NEPTOOLS_ASSERT(error_msg);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void StateRef::HandleDotdotdotCatch()
{
    if (lua_gettop(vm) == 0 || !lua_isstring(vm, -1)) throw;

    error_msg = lua_tolstring(vm, -1, &error_len);
    if (strcmp(error_msg, "C++ exception") == 0) throw;
}
#endif

void StateRef::TypeError(bool arg, const char* expected, int idx)
{
    auto actual = luaL_typename(vm, idx);
    std::stringstream ss;
    ss << expected << " expected, got " << actual;
    if (arg)
        luaL_argerror(vm, idx, ss.str().c_str());
    else
        luaL_error(vm, "invalid lua value: %s", ss.str().c_str());
    NEPTOOLS_UNREACHABLE("lua_error returned");
}

thread_local const char* StateRef::error_msg;
thread_local size_t StateRef::error_len;

}
}