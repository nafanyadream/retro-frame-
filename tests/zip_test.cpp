#include "../src/core/utf.h"
#include "../src/core/zip.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
    class FileSource : public rf::ByteSource
    {
    public:
        explicit FileSource(const char* path)
        {
            f_ = std::fopen(path, "rb");
            if (f_ != nullptr)
            {
                std::fseek(f_, 0, SEEK_END);
                size_ = std::uint64_t(std::ftell(f_));
            }
        }
        ~FileSource() override
        {
            if (f_ != nullptr)
            {
                std::fclose(f_);
            }
        }
        std::uint64_t Size() const override { return size_; }
        bool Read(std::uint64_t offset, void* dst, std::size_t size) override
        {
            return f_ != nullptr && std::fseek(f_, long(offset), SEEK_SET) == 0 && std::fread(dst, 1, size, f_) == size;
        }

    private:
        FILE* f_ = nullptr;
        std::uint64_t size_ = 0;
    };
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        return 1;
    }
    FileSource src(argv[1]);
    rf::ZipReader zip(src);
    const rf::ZipError open = zip.Open();
    if (open != rf::ZipError::None)
    {
        std::printf("open error %d\n", int(open));
        return 2;
    }
    int index = 0;
    for (const auto& e : zip.Entries())
    {
        std::vector<unsigned char> data;
        const rf::ZipError err = e.folder ? rf::ZipError::None : zip.Extract(e, data);
        std::printf("%d\t%s\t%d\t%d\t%llu\n", index, rf::ToUtf8(e.name).c_str(), int(err), e.folder ? 1 : 0, (unsigned long long)data.size());
        if (!e.folder && err == rf::ZipError::None)
        {
            const std::string out = std::string(argv[2]) + "/" + std::to_string(index) + ".bin";
            FILE* f = std::fopen(out.c_str(), "wb");
            if (f != nullptr)
            {
                if (!data.empty())
                {
                    std::fwrite(data.data(), 1, data.size(), f);
                }
                std::fclose(f);
            }
        }
        ++index;
    }
    return 0;
}
