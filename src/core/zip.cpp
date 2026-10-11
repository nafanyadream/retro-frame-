#include "zip.h"

#include "utf.h"

#include <algorithm>
#include <array>

namespace rf
{
    namespace
    {
        constexpr int MaxBits = 15;
        constexpr int MaxLengthCodes = 286;
        constexpr int MaxDistCodes = 30;
        constexpr int FixedLengthCodes = 288;

        const std::uint16_t LengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                              35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        const std::uint8_t LengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        const std::uint16_t DistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        const std::uint8_t DistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        const std::uint8_t CodeOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

        const char16_t Cp866High[128] = {
            0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
            0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
            0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
            0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
            0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
            0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
            0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
            0x0401, 0x0451, 0x0404, 0x0454, 0x0407, 0x0457, 0x040E, 0x045E, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x2116, 0x00A4, 0x25A0, 0x00A0};

        struct Huffman
        {
            std::array<std::uint16_t, MaxBits + 1> count{};
            std::vector<std::uint16_t> symbol;
        };

        class BitReader
        {
        public:
            BitReader(const unsigned char* data, std::size_t size) : data_(data), size_(size) {}

            bool failed = false;

            int Bits(int need)
            {
                std::uint32_t value = buffer_;
                while (count_ < need)
                {
                    if (pos_ >= size_)
                    {
                        failed = true;
                        return 0;
                    }
                    value |= std::uint32_t(data_[pos_++]) << count_;
                    count_ += 8;
                }
                buffer_ = value >> need;
                count_ -= need;
                return int(value & ((1u << need) - 1u));
            }

            void AlignToByte()
            {
                buffer_ = 0;
                count_ = 0;
            }

            bool Take(std::size_t n, const unsigned char*& at)
            {
                if (size_ - pos_ < n)
                {
                    failed = true;
                    return false;
                }
                at = data_ + pos_;
                pos_ += n;
                return true;
            }

        private:
            const unsigned char* data_;
            std::size_t size_;
            std::size_t pos_ = 0;
            std::uint32_t buffer_ = 0;
            int count_ = 0;
        };

        int Build(Huffman& h, const std::uint16_t* lengths, int n)
        {
            h.count.fill(0);
            for (int s = 0; s < n; ++s)
            {
                ++h.count[lengths[s]];
            }
            if (h.count[0] == n)
            {
                h.symbol.clear();
                return 0;
            }
            int left = 1;
            for (int len = 1; len <= MaxBits; ++len)
            {
                left <<= 1;
                left -= h.count[std::size_t(len)];
                if (left < 0)
                {
                    return left;
                }
            }
            std::array<std::uint16_t, MaxBits + 1> offs{};
            for (int len = 1; len < MaxBits; ++len)
            {
                offs[std::size_t(len + 1)] = std::uint16_t(offs[std::size_t(len)] + h.count[std::size_t(len)]);
            }
            h.symbol.assign(std::size_t(n), 0);
            for (int s = 0; s < n; ++s)
            {
                if (lengths[s] != 0)
                {
                    h.symbol[offs[lengths[s]]++] = std::uint16_t(s);
                }
            }
            return left;
        }

        int Decode(BitReader& in, const Huffman& h)
        {
            int code = 0;
            int first = 0;
            int index = 0;
            for (int len = 1; len <= MaxBits; ++len)
            {
                code |= in.Bits(1);
                if (in.failed)
                {
                    return -1;
                }
                const int count = h.count[std::size_t(len)];
                if (code - count < first)
                {
                    return h.symbol[std::size_t(index + (code - first))];
                }
                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
            }
            return -1;
        }

        bool Stored(BitReader& in, std::size_t expected, std::vector<unsigned char>& out)
        {
            in.AlignToByte();
            const unsigned char* head = nullptr;
            if (!in.Take(4, head))
            {
                return false;
            }
            const unsigned len = unsigned(head[0]) | (unsigned(head[1]) << 8);
            const unsigned nlen = unsigned(head[2]) | (unsigned(head[3]) << 8);
            if (len != (~nlen & 0xFFFFu))
            {
                return false;
            }
            const unsigned char* body = nullptr;
            if (!in.Take(len, body) || out.size() + len > expected)
            {
                return false;
            }
            out.insert(out.end(), body, body + len);
            return true;
        }

        bool Codes(BitReader& in, const Huffman& lencode, const Huffman& distcode, std::size_t expected, std::vector<unsigned char>& out)
        {
            for (;;)
            {
                int symbol = Decode(in, lencode);
                if (symbol < 0)
                {
                    return false;
                }
                if (symbol < 256)
                {
                    if (out.size() >= expected)
                    {
                        return false;
                    }
                    out.push_back(static_cast<unsigned char>(symbol));
                    continue;
                }
                if (symbol == 256)
                {
                    return true;
                }
                symbol -= 257;
                if (symbol >= 29)
                {
                    return false;
                }
                const std::size_t len = std::size_t(LengthBase[symbol]) + std::size_t(in.Bits(LengthExtra[symbol]));
                const int ds = Decode(in, distcode);
                if (ds < 0 || ds >= 30)
                {
                    return false;
                }
                const std::size_t dist = std::size_t(DistBase[ds]) + std::size_t(in.Bits(DistExtra[ds]));
                if (in.failed || dist > out.size() || out.size() + len > expected)
                {
                    return false;
                }
                std::size_t from = out.size() - dist;
                for (std::size_t k = 0; k < len; ++k)
                {
                    out.push_back(out[from++]);
                }
            }
        }

        bool Fixed(BitReader& in, std::size_t expected, std::vector<unsigned char>& out)
        {
            static Huffman lencode;
            static Huffman distcode;
            static bool ready = false;
            if (!ready)
            {
                std::uint16_t lengths[FixedLengthCodes];
                int s = 0;
                for (; s < 144; ++s)
                {
                    lengths[s] = 8;
                }
                for (; s < 256; ++s)
                {
                    lengths[s] = 9;
                }
                for (; s < 280; ++s)
                {
                    lengths[s] = 7;
                }
                for (; s < FixedLengthCodes; ++s)
                {
                    lengths[s] = 8;
                }
                Build(lencode, lengths, FixedLengthCodes);
                for (s = 0; s < MaxDistCodes; ++s)
                {
                    lengths[s] = 5;
                }
                Build(distcode, lengths, MaxDistCodes);
                ready = true;
            }
            return Codes(in, lencode, distcode, expected, out);
        }

        bool Dynamic(BitReader& in, std::size_t expected, std::vector<unsigned char>& out)
        {
            const int nlen = in.Bits(5) + 257;
            const int ndist = in.Bits(5) + 1;
            const int ncode = in.Bits(4) + 4;
            if (in.failed || nlen > MaxLengthCodes || ndist > MaxDistCodes)
            {
                return false;
            }
            std::uint16_t lengths[MaxLengthCodes + MaxDistCodes] = {};
            for (int i = 0; i < ncode; ++i)
            {
                lengths[CodeOrder[i]] = std::uint16_t(in.Bits(3));
            }
            Huffman lencode;
            Huffman distcode;
            if (in.failed || Build(lencode, lengths, 19) != 0)
            {
                return false;
            }
            int index = 0;
            while (index < nlen + ndist)
            {
                int symbol = Decode(in, lencode);
                if (symbol < 0)
                {
                    return false;
                }
                if (symbol < 16)
                {
                    lengths[index++] = std::uint16_t(symbol);
                    continue;
                }
                std::uint16_t value = 0;
                int repeat = 0;
                if (symbol == 16)
                {
                    if (index == 0)
                    {
                        return false;
                    }
                    value = lengths[index - 1];
                    repeat = 3 + in.Bits(2);
                }
                else if (symbol == 17)
                {
                    repeat = 3 + in.Bits(3);
                }
                else
                {
                    repeat = 11 + in.Bits(7);
                }
                if (in.failed || index + repeat > nlen + ndist)
                {
                    return false;
                }
                while (repeat-- > 0)
                {
                    lengths[index++] = value;
                }
            }
            if (lengths[256] == 0)
            {
                return false;
            }
            int err = Build(lencode, lengths, nlen);
            if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
            {
                return false;
            }
            err = Build(distcode, lengths + nlen, ndist);
            if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
            {
                return false;
            }
            return Codes(in, lencode, distcode, expected, out);
        }

        std::uint16_t U16(const unsigned char* p)
        {
            return std::uint16_t(p[0] | (p[1] << 8));
        }

        std::uint32_t U32(const unsigned char* p)
        {
            return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        }

        std::uint64_t U64(const unsigned char* p)
        {
            return std::uint64_t(U32(p)) | (std::uint64_t(U32(p + 4)) << 32);
        }

        bool LooksUtf8(const std::string& s)
        {
            bool multi = false;
            for (std::size_t i = 0; i < s.size();)
            {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                int extra = 0;
                if (c < 0x80)
                {
                    ++i;
                    continue;
                }
                if ((c & 0xE0) == 0xC0 && c >= 0xC2)
                {
                    extra = 1;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    extra = 2;
                }
                else if ((c & 0xF8) == 0xF0 && c <= 0xF4)
                {
                    extra = 3;
                }
                else
                {
                    return false;
                }
                if (i + std::size_t(extra) >= s.size())
                {
                    return false;
                }
                for (int k = 1; k <= extra; ++k)
                {
                    if ((static_cast<unsigned char>(s[i + std::size_t(k)]) & 0xC0) != 0x80)
                    {
                        return false;
                    }
                }
                multi = true;
                i += std::size_t(extra) + 1;
            }
            return multi;
        }
    }

    std::uint32_t Crc32(const unsigned char* data, std::size_t size)
    {
        static std::array<std::uint32_t, 256> table{};
        static bool ready = false;
        if (!ready)
        {
            for (std::uint32_t n = 0; n < 256; ++n)
            {
                std::uint32_t c = n;
                for (int k = 0; k < 8; ++k)
                {
                    c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                table[n] = c;
            }
            ready = true;
        }
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::size_t i = 0; i < size; ++i)
        {
            crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

    bool Inflate(const unsigned char* data, std::size_t size, std::size_t expected, std::vector<unsigned char>& out)
    {
        out.clear();
        out.reserve(expected);
        BitReader in(data, size);
        int last = 0;
        do
        {
            last = in.Bits(1);
            const int type = in.Bits(2);
            if (in.failed)
            {
                return false;
            }
            bool ok = false;
            if (type == 0)
            {
                ok = Stored(in, expected, out);
            }
            else if (type == 1)
            {
                ok = Fixed(in, expected, out);
            }
            else if (type == 2)
            {
                ok = Dynamic(in, expected, out);
            }
            if (!ok || in.failed)
            {
                return false;
            }
        } while (last == 0);
        return out.size() == expected;
    }

    std::wstring DecodeZipName(const std::string& raw, bool utf8)
    {
        if (utf8 || LooksUtf8(raw))
        {
            return FromUtf8(raw);
        }
        std::wstring out;
        out.reserve(raw.size());
        for (char ch : raw)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            out.push_back(c < 0x80 ? wchar_t(c) : wchar_t(Cp866High[c - 0x80]));
        }
        return out;
    }

    bool ZipReader::ReadAt(std::uint64_t offset, std::vector<unsigned char>& buf, std::size_t size)
    {
        if (offset > src_.Size() || src_.Size() - offset < size)
        {
            return false;
        }
        buf.resize(size);
        return size == 0 || src_.Read(offset, buf.data(), size);
    }

    ZipError ZipReader::Open()
    {
        entries_.clear();
        const std::uint64_t total = src_.Size();
        if (total < 22)
        {
            return ZipError::NotZip;
        }
        const std::size_t tailSize = std::size_t(std::min<std::uint64_t>(total, 65535 + 22));
        std::vector<unsigned char> tail;
        if (!ReadAt(total - tailSize, tail, tailSize))
        {
            return ZipError::Damaged;
        }
        std::size_t eocd = std::string::npos;
        for (std::size_t i = tailSize - 22 + 1; i-- > 0;)
        {
            if (U32(tail.data() + i) == 0x06054b50u)
            {
                eocd = i;
                break;
            }
        }
        if (eocd == std::string::npos)
        {
            return ZipError::NotZip;
        }
        const unsigned char* e = tail.data() + eocd;
        std::uint64_t count = U16(e + 10);
        std::uint64_t cdSize = U32(e + 12);
        std::uint64_t cdOffset = U32(e + 16);
        const std::uint64_t eocdPos = total - tailSize + eocd;
        if (count == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOffset == 0xFFFFFFFFu)
        {
            std::vector<unsigned char> loc;
            if (eocdPos >= 20 && ReadAt(eocdPos - 20, loc, 20) && U32(loc.data()) == 0x07064b50u)
            {
                std::vector<unsigned char> z;
                if (ReadAt(U64(loc.data() + 8), z, 56) && U32(z.data()) == 0x06064b50u)
                {
                    count = U64(z.data() + 32);
                    cdSize = U64(z.data() + 40);
                    cdOffset = U64(z.data() + 48);
                }
            }
        }
        if (cdSize > 256ull * 1024ull * 1024ull || cdOffset > total || total - cdOffset < cdSize)
        {
            return ZipError::Damaged;
        }
        std::vector<unsigned char> cd;
        if (!ReadAt(cdOffset, cd, std::size_t(cdSize)))
        {
            return ZipError::Damaged;
        }
        std::size_t p = 0;
        while (p + 46 <= cd.size() && U32(cd.data() + p) == 0x02014b50u)
        {
            const unsigned char* h = cd.data() + p;
            const std::uint16_t flags = U16(h + 8);
            const std::size_t nameLen = U16(h + 28);
            const std::size_t extraLen = U16(h + 30);
            const std::size_t commentLen = U16(h + 32);
            if (p + 46 + nameLen + extraLen + commentLen > cd.size())
            {
                break;
            }
            ZipEntry entry;
            entry.method = U16(h + 10);
            entry.crc = U32(h + 16);
            entry.packedSize = U32(h + 20);
            entry.size = U32(h + 24);
            entry.headerOffset = U32(h + 42);
            entry.encrypted = (flags & 1u) != 0;
            const std::string raw(reinterpret_cast<const char*>(h + 46), nameLen);
            entry.name = DecodeZipName(raw, (flags & 0x800u) != 0);
            const unsigned char* x = h + 46 + nameLen;
            std::size_t q = 0;
            while (q + 4 <= extraLen)
            {
                const std::uint16_t id = U16(x + q);
                const std::size_t len = U16(x + q + 2);
                if (q + 4 + len > extraLen)
                {
                    break;
                }
                const unsigned char* d = x + q + 4;
                if (id == 0x0001)
                {
                    std::size_t k = 0;
                    if (entry.size == 0xFFFFFFFFu && k + 8 <= len)
                    {
                        entry.size = U64(d + k);
                        k += 8;
                    }
                    if (entry.packedSize == 0xFFFFFFFFu && k + 8 <= len)
                    {
                        entry.packedSize = U64(d + k);
                        k += 8;
                    }
                    if (entry.headerOffset == 0xFFFFFFFFu && k + 8 <= len)
                    {
                        entry.headerOffset = U64(d + k);
                    }
                }
                else if (id == 0x7075 && len > 5 && d[0] == 1)
                {
                    entry.name = FromUtf8(std::string(reinterpret_cast<const char*>(d + 5), len - 5));
                }
                q += 4 + len;
            }
            for (auto& c : entry.name)
            {
                if (c == L'/')
                {
                    c = L'\\';
                }
            }
            entry.folder = !entry.name.empty() && entry.name.back() == L'\\';
            entries_.push_back(std::move(entry));
            p += 46 + nameLen + extraLen + commentLen;
            if (entries_.size() >= count && count != 0)
            {
                break;
            }
        }
        if (entries_.empty() && count != 0)
        {
            return ZipError::Damaged;
        }
        return ZipError::None;
    }

    ZipError ZipReader::Extract(const ZipEntry& entry, std::vector<unsigned char>& out)
    {
        out.clear();
        if (entry.encrypted)
        {
            return ZipError::Encrypted;
        }
        if (entry.method != 0 && entry.method != 8)
        {
            return ZipError::Method;
        }
        if (entry.size > MaxEntrySize || entry.packedSize > MaxEntrySize)
        {
            return ZipError::TooBig;
        }
        std::vector<unsigned char> head;
        if (!ReadAt(entry.headerOffset, head, 30) || U32(head.data()) != 0x04034b50u)
        {
            return ZipError::Damaged;
        }
        const std::uint64_t start = entry.headerOffset + 30 + U16(head.data() + 26) + U16(head.data() + 28);
        std::vector<unsigned char> packed;
        if (!ReadAt(start, packed, std::size_t(entry.packedSize)))
        {
            return ZipError::Damaged;
        }
        if (entry.method == 0)
        {
            if (entry.packedSize != entry.size)
            {
                return ZipError::Damaged;
            }
            out = std::move(packed);
        }
        else if (!Inflate(packed.data(), packed.size(), std::size_t(entry.size), out))
        {
            out.clear();
            return ZipError::Damaged;
        }
        if (Crc32(out.data(), out.size()) != entry.crc)
        {
            out.clear();
            return ZipError::Damaged;
        }
        return ZipError::None;
    }
}
