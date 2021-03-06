#include "sink.hpp"
#include "low_io.hpp"
#include "except.hpp"
#include <boost/exception/errinfo_file_name.hpp>
#include <iostream>

#define NEPTOOLS_LOG_NAME "sink"
#include "logger_helper.hpp"

namespace Neptools
{
namespace
{

struct MmapSink : public Sink
{
    MmapSink(LowIo&& io, FilePosition size);
    ~MmapSink();
    void Write_(StringView data) override;
    void Pad_(FileMemSize len) override;

    void MapNext(FileMemSize len);

    LowIo io;
};

struct SimpleSink : public Sink
{
    SimpleSink(LowIo io, FilePosition size) : Sink{size}, io{std::move(io)}
    {
        Sink::buf = buf;
        buf_size = LowIo::MEM_CHUNK;
    }
    ~SimpleSink();

    void Write_(StringView data) override;
    void Pad_(FileMemSize len) override;
    void Flush() override;

    LowIo io;
    Byte buf[LowIo::MEM_CHUNK];
};

}

MmapSink::MmapSink(LowIo&& io, FilePosition size) : Sink{size}
{
    size_t to_map = size < LowIo::MMAP_LIMIT ? size : LowIo::MMAP_CHUNK;

    io.Truncate(size);
    io.PrepareMmap(true);
    buf_size = to_map;
    buf = static_cast<Byte*>(io.Mmap(0, to_map, true));

    this->io = std::move(io);
}

MmapSink::~MmapSink()
{
    if (buf)
        LowIo::Munmap(buf, buf_size);
}

void MmapSink::Write_(StringView data)
{
    NEPTOOLS_ASSERT(buf_put == buf_size && offset < size &&
                    buf_size == LowIo::MMAP_CHUNK);

    offset += buf_put;
    if (data.length() / LowIo::MMAP_CHUNK)
    {
        auto to_write = data.length() / LowIo::MMAP_CHUNK * LowIo::MMAP_CHUNK;
        io.Pwrite(data.udata(), to_write, offset);
        data.remove_prefix(to_write);
        offset += to_write;
        buf_put = 0;
    }

    MapNext(data.length());
    memcpy(buf, data.data(), data.length());
}

void MmapSink::Pad_(FileMemSize len)
{
    NEPTOOLS_ASSERT(buf_put == buf_size && offset < size &&
                    buf_size == LowIo::MMAP_CHUNK);

    offset += buf_put + len / LowIo::MMAP_CHUNK * LowIo::MMAP_CHUNK;
    NEPTOOLS_ASSERT_MSG(offset <= size, "sink overflow");
    MapNext(len % LowIo::MMAP_CHUNK);
}

void MmapSink::MapNext(FileMemSize len)
{
    // wine fails on 0 size
    // windows fails if offset+size > file_length...
    // (linux doesn't care...)
    if (offset < size)
    {
        auto nbuf_size = std::min<FileMemSize>(LowIo::MMAP_CHUNK, size-offset);
        NEPTOOLS_ASSERT(nbuf_size >= len);
        void* nbuf = io.Mmap(offset, nbuf_size, true);
        io.Munmap(buf, buf_size);
        buf = static_cast<Byte*>(nbuf);
        buf_put = len;
        buf_size = nbuf_size;
    }
    else
    {
        io.Munmap(buf, buf_size);
        buf = nullptr;
    }
}

SimpleSink::~SimpleSink()
{
    try { Flush(); }
    catch (std::exception& e)
    {
        ERR << "~SimpleSink " << ExceptionToString() << std::endl;
    }
}

void SimpleSink::Flush()
{
    if (buf_put)
    {
        io.Write(buf, buf_put);
        offset += buf_put;
        buf_put = 0;
    }
}

void SimpleSink::Write_(StringView data)
{
    NEPTOOLS_ASSERT(buf_size == LowIo::MEM_CHUNK &&
                    buf_put == LowIo::MEM_CHUNK);
    io.Write(buf, LowIo::MEM_CHUNK);
    offset += LowIo::MEM_CHUNK;

    if (data.length() >= LowIo::MEM_CHUNK)
    {
        io.Write(data.data(), data.length());
        offset += data.length();
        buf_put = 0;
    }
    else
    {
        memcpy(buf, data.data(), data.length());
        buf_put = data.length();
    }
}

void SimpleSink::Pad_(FileMemSize len)
{
    NEPTOOLS_ASSERT(buf_size == LowIo::MEM_CHUNK &&
                    buf_put == LowIo::MEM_CHUNK);
    io.Write(buf, LowIo::MEM_CHUNK);
    offset += LowIo::MEM_CHUNK;

    // assume we're not seekable (I don't care about not mmap-able but seekable
    // files)
    if (len >= LowIo::MEM_CHUNK)
    {
        memset(buf, 0, LowIo::MEM_CHUNK);
        size_t i;
        for (i = LowIo::MEM_CHUNK; i < len; i += LowIo::MEM_CHUNK)
            io.Write(buf, LowIo::MEM_CHUNK);
        offset += i - LowIo::MEM_CHUNK;
    }
    else
        memset(buf, 0, len);
    buf_put = len % LowIo::MEM_CHUNK;
}

NotNull<RefCountedPtr<Sink>> Sink::ToFile(
    boost::filesystem::path fname, FilePosition size, bool try_mmap)
{
    return AddInfo(
        [&]() -> NotNull<RefCountedPtr<Sink>>
        {
            LowIo io{fname.c_str(), true};
            if (!try_mmap)
                return MakeRefCounted<SimpleSink>(std::move(io), size);

            try { return MakeRefCounted<MmapSink>(std::move(io), size); }
            catch (const std::system_error& e)
            {
                WARN << "Mmmap failed, falling back to normal writing: "
                     << ExceptionToString() << std::endl;
                return MakeRefCounted<SimpleSink>(std::move(io), size);
            }
        },
        [&](auto& e) { e << boost::errinfo_file_name{fname.string()}; });
}

NotNull<RefCountedPtr<Sink>> Sink::ToStdOut()
{
    return MakeRefCounted<SimpleSink>(LowIo::OpenStdOut(), -1);
}

void MemorySink::Write_(StringView)
{ NEPTOOLS_UNREACHABLE("MemorySink::Write_ called"); }
void MemorySink::Pad_(FileMemSize)
{ NEPTOOLS_UNREACHABLE("MemorySink::Pad_ called"); }

}
