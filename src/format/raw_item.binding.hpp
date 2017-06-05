// Auto generated code, do not edit. See gen_binding in project root.
#include "lua/user_type.hpp"

namespace Neptools::Lua
{

// class neptools.raw_item
template<>
void TypeRegister::DoRegister<::Neptools::RawItem>(TypeBuilder& bld)
{
    bld.Inherit<::Neptools::RawItem, ::Neptools::Item>();

    bld.AddFunction<
        &::Neptools::Lua::TypeTraits<::Neptools::RawItem>::Make<LuaGetRef<::Neptools::Item::Key>, LuaGetRef<::Neptools::Context *>, LuaGetRef<::Neptools::Source>, LuaGetRef<::Neptools::FilePosition>>
    >("new");
    bld.AddFunction<
        static_cast<::Neptools::RawItem::GetSourceRet (*)(::Neptools::ItemPointer, ::Neptools::FilePosition)>(::Neptools::RawItem::GetSource),
        static_cast<const ::Neptools::Source & (::Neptools::RawItem::*)() const noexcept>(&::Neptools::RawItem::GetSource)
    >("get_source");
    bld.AddFunction<
        static_cast<::Neptools::Item & (::Neptools::RawItem::*)(::Neptools::FilePosition, ::Neptools::NotNull<::Neptools::RefCountedPtr<::Neptools::Item> >)>(&::Neptools::RawItem::Split<::Neptools::Item>),
        static_cast<::Neptools::RawItem & (::Neptools::RawItem::*)(::Neptools::FilePosition, ::Neptools::FilePosition)>(&::Neptools::RawItem::Split)
    >("split");

}
static TypeRegister::StateRegister<::Neptools::RawItem> reg_neptools_raw_item;

}


const char ::Neptools::RawItem::TYPE_NAME[] = "neptools.raw_item";
