#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rf
{
    class ByteSource
    {
    public:
        virtual ~ByteSource() = default;
        virtual std::uint64_t Size() const = 0;
        virtual bool Read(std::uint64_t offset, void* dst, std::size_t size) = 0;
    };

    std::uint32_t Crc32(const unsigned char* data, std::size_t size);
    bool Inflate(const unsigned char* data, std::size_t size, std::size_t expected, std::vector<unsigned char>& out);
    std::wstring DecodeZipName(const std::string& raw, bool utf8);

    struct ZipEntry
    {
        std::wstring name;
        std::uint64_t packedSize = 0;
        std::uint64_t size = 0;
        std::uint64_t headerOffset = 0;
        std::uint32_t crc = 0;
        int method = 0;
        bool encrypted = false;
        bool folder = false;
    };

    enum class ZipError
    {
        None,
        NotZip,
        Damaged,
        Encrypted,
        Method,
        TooBig
    };

    class ZipReader
    {
    public:
        explicit ZipReader(ByteSource& source) : src_(source) {}

        ZipError Open();
        const std::vector<ZipEntry>& Entries() const { return entries_; }
        ZipError Extract(const ZipEntry& entry, std::vector<unsigned char>& out);

        static bool Supported(const ZipEntry& entry) { return !entry.encrypted && (entry.method == 0 || entry.method == 8); }
        static constexpr std::uint64_t MaxEntrySize = 64ull * 1024ull * 1024ull;

    private:
        bool ReadAt(std::uint64_t offset, std::vector<unsigned char>& buf, std::size_t size);

        ByteSource& src_;
        std::vector<ZipEntry> entries_;
    };
}
