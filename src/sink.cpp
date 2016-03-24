#include "sink.hpp"
#include "low_io.hpp"
#include "except.hpp"
#include <boost/exception/errinfo_file_name.hpp>
#include <iostream>

namespace Neptools
{
namespace
{

struct MmapSink : public Sink
{
    MmapSink(LowIo&& io, FilePosition size);
    ~MmapSink();
    void Write_(const Byte* buf, FileMemSize len) override;
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

    void Write_(const Byte* buf, FileMemSize len) override;
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

void MmapSink::Write_(const Byte* data, FileMemSize len)
{
    NEPTOOLS_ASSERT(buf_put == buf_size && offset < size &&
                    buf_size == LowIo::MMAP_CHUNK);

    offset += buf_put;
    if (len / LowIo::MMAP_CHUNK)
    {
        auto to_write = len / LowIo::MMAP_CHUNK * LowIo::MMAP_CHUNK;
        io.Pwrite(data, to_write, offset);
        data += to_write;
        offset += to_write;
        len -= to_write;
        buf_put = 0;
    }

    MapNext(len);
    memcpy(buf, data, len);
    buf_put = len;
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
        std::cerr << "Mayday: ~SimpleSink";
        PrintException(std::cerr);
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

void SimpleSink::Write_(const Byte* data, FileMemSize len)
{
    NEPTOOLS_ASSERT(buf_size == LowIo::MEM_CHUNK &&
                    buf_put == LowIo::MEM_CHUNK);
    io.Write(buf, LowIo::MEM_CHUNK);
    offset += LowIo::MEM_CHUNK;

    if (len >= LowIo::MEM_CHUNK)
    {
        io.Write(data, len);
        offset += len;
        buf_put = 0;
    }
    else
    {
        memcpy(buf, data, len);
        buf_put = len;
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

std::unique_ptr<Sink> Sink::ToFile(
    boost::filesystem::path fname, FilePosition size, bool try_mmap)
{
    return AddInfo(
        [&]() -> std::unique_ptr<Sink>
        {
            LowIo io{fname.c_str(), true};
            if (!try_mmap)
                return std::make_unique<SimpleSink>(std::move(io), size);

            try { return std::make_unique<MmapSink>(std::move(io), size); }
            catch (const std::system_error& e)
            {
                std::cerr << "Mmmap failed: ";
                PrintException(std::cerr);
                return std::make_unique<SimpleSink>(std::move(io), size);
            }
        },
        [&](auto& e) { e << boost::errinfo_file_name{fname.string()}; });
}

std::unique_ptr<Sink> Sink::ToStdOut()
{
    return std::make_unique<SimpleSink>(LowIo::OpenStdOut(), -1);
}

void MemorySink::Write_(const Byte*, FileMemSize)
{ NEPTOOLS_UNREACHABLE("MemorySink::Write_ called"); }
void MemorySink::Pad_(FileMemSize)
{ NEPTOOLS_UNREACHABLE("MemorySink::Pad_ called"); }

}