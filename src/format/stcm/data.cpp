#include "data.hpp"
#include "gbnl.hpp"
#include "../context.hpp"
#include "../raw_item.hpp"
#include "../../sink.hpp"
#include <iostream>

namespace Neptools
{
namespace Stcm
{

void DataItem::Header::Validate(FilePosition chunk_size) const
{
#define VALIDATE(x) NEPTOOLS_VALIDATE_FIELD("Stcm::DataItem::Header", x)
    VALIDATE(type < 0xff);
    VALIDATE(length <= chunk_size);
#undef VALIDATE
}

DataItem::DataItem(Key k, Context* ctx, const Header& raw, size_t chunk_size)
    : ItemWithChildren{k, ctx}
{
    raw.Validate(chunk_size);

    type = raw.type;
    offset_unit = raw.offset_unit;
    field_8 = raw.field_8;
}

DataItem& DataItem::CreateAndInsert(ItemPointer ptr)
{
    auto x = RawItem::Get<Header>(ptr);

    auto& ret = x.ritem.SplitCreate<DataItem>(
        ptr.offset, x.t, x.ritem.GetSize() - ptr.offset - sizeof(Header));
    if (x.t.length > 0)
        ret.MoveNextToChild(x.t.length);

    NEPTOOLS_ASSERT(ret.GetSize() == sizeof(Header) + x.t.length);

    // hack
    if (!ret.GetChildren().empty())
    {
        auto child = dynamic_cast<RawItem*>(&ret.GetChildren().front());
        if (child && child->GetSize() > sizeof(Gbnl::Header))
        {
            char buf[4];
            child->GetSource().Pread(child->GetSize() - sizeof(Gbnl::Header), buf, 4);
            if (memcmp(buf, "GBNL", 4) == 0)
                GbnlItem::CreateAndInsert({child, 0});
        }
    }
    return ret;
}

void DataItem::Dump_(Sink& sink) const
{
    Header hdr;
    hdr.type = type;
    hdr.offset_unit = offset_unit;
    hdr.field_8 = field_8;
    hdr.length = GetSize() - sizeof(Header);
    sink.WriteGen(hdr);

    ItemWithChildren::Dump_(sink);
}

void DataItem::Inspect_(std::ostream& os) const
{
    Item::Inspect_(os);
    os << "data(" << type << ", " << offset_unit << ", " << field_8 << ") {";
    ItemWithChildren::Inspect_(os);
    os << '}';
}

FilePosition DataItem::GetSize() const noexcept
{
    return ItemWithChildren::GetSize() + sizeof(Header);
}

void DataItem::Fixup()
{
    ItemWithChildren::Fixup_(sizeof(Header));
}

}
}
