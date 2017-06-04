#ifndef UUID_B8B05EAA_92F1_4605_AE60_0067F9F66406
#define UUID_B8B05EAA_92F1_4605_AE60_0067F9F66406
#pragma once

#include "function_call.hpp"
#include "../shared_ptr.hpp"

namespace Neptools::Lua
{

template <auto Fun,
          typename Args = typename Detail::FunctionTraits<decltype(Fun)>::Arguments>
struct OwnedSharedPtrWrap;

template <auto Fun, typename Class, typename... Args>
struct OwnedSharedPtrWrap<Fun, Detail::List<Class, Args...>>
{
    using OrigRet = std::remove_reference_t<
        typename Detail::FunctionTraits<decltype(Fun)>::Return>;
    using NewRet = NotNull<SharedPtr<OrigRet>>;

    static NewRet Wrap(Class&& thiz, Args&&... args)
    {
        return NewRet{
            &thiz,
            &Invoke(Fun, std::forward<Class>(thiz), std::forward<Args>(args)...),
            true};
    }
};

}

#endif
