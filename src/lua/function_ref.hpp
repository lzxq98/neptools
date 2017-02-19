#ifndef UUID_D13BEDEB_B9E0_4D88_A07E_03C11564BE1C
#define UUID_D13BEDEB_B9E0_4D88_A07E_03C11564BE1C
#pragma once

#include "value.hpp"

namespace Neptools::Lua
{

template <typename Value = StackValue>
class FunctionRef
{
public:
    template <typename... Args>
    FunctionRef(Args&&... args) : fun{std::forward<Args>(args)...} {}

    template <typename Ret, typename... Args>
    Ret Call(StateRef vm, Args&&... args)
    {
        NEPTOOLS_LUA_GETTOP(vm, top);

        fun.Push(vm);
        NEPTOOLS_ASSERT(lua_type(vm, -1) == LUA_TFUNCTION);
        vm.PushAll(std::forward<Args>(args)...);
        lua_call(vm, sizeof...(Args), 1);

        auto ret = vm.Get<Ret>();
        lua_pop(vm, 1);
        NEPTOOLS_LUA_CHECKTOP(vm, top);
        return ret;
    }

private:
    Value fun;
};

template <typename Ret, typename Value = StackValue>
class FunctionWrapBase : public FunctionRef<Value>
{
public:
    template <typename... Args>
    FunctionWrapBase(StateRef vm, Args&&... args)
        : FunctionRef<Value>{vm, std::forward<Args>(args)...}, vm{vm} {}

protected:
    StateRef vm;
};

template <typename Ret, typename Value = StackValue>
struct FunctionWrapGen : public FunctionWrapBase<Ret, Value>
{
    using FunctionWrapBase<Ret, Value>::FunctionWrapBase;

    template <typename... Args>
    decltype(auto) operator()(Args&&... args)
    { return this->template Call<Ret>(this->vm, std::forward<Args>(args)...); }
};

template <typename Func, typename Value = StackValue> struct FunctionWrap;

template <typename Ret, typename... Args, typename Value>
struct FunctionWrap<Ret(Args...), Value> : public FunctionWrapBase<Ret, Value>
{
    using FunctionWrapBase<Ret, Value>::FunctionWrapBase;

    decltype(auto) operator()(Args... args)
    { return this->template Call<Ret>(this->vm, args...); }
};

}

#endif
