#pragma once

#include "../src/core/host.h"
#include "../src/core/utf.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace rftest
{
    namespace fs = std::filesystem;

    class PosixProcess : public rf::Process
    {
    public:
        static std::unique_ptr<rf::Process> Start(const std::string& exe, const std::vector<std::wstring>& args, bool input, bool output)
        {
            int in[2] = {-1, -1};
            int out[2] = {-1, -1};
            if ((input && pipe(in) != 0) || (output && pipe(out) != 0))
            {
                return nullptr;
            }
            char logName[] = "/tmp/rftest-log-XXXXXX";
            const int logFd = mkstemp(logName);
            std::vector<std::string> argv8 = {exe};
            for (const auto& a : args)
            {
                argv8.push_back(rf::ToUtf8(a));
            }
            std::vector<char*> argv;
            for (auto& a : argv8)
            {
                argv.push_back(a.data());
            }
            argv.push_back(nullptr);
            const pid_t pid = fork();
            if (pid == 0)
            {
                const int nul = open("/dev/null", O_RDWR);
                dup2(input ? in[0] : nul, 0);
                dup2(output ? out[1] : nul, 1);
                dup2(logFd, 2);
                for (int fd = 3; fd < 256; ++fd)
                {
                    close(fd);
                }
                execvp(argv[0], argv.data());
                _exit(127);
            }
            auto p = std::unique_ptr<PosixProcess>(new PosixProcess());
            p->pid_ = pid;
            p->log_ = logName;
            close(logFd);
            if (input)
            {
                close(in[0]);
                p->in_ = in[1];
            }
            if (output)
            {
                close(out[1]);
                p->out_ = out[0];
            }
            return p;
        }

        ~PosixProcess() override
        {
            Kill();
            int code = 0;
            Finish(code, []() { return true; });
            CloseInput();
            if (out_ >= 0)
            {
                close(out_);
            }
            unlink(log_.c_str());
        }

        bool Write(const void* data, std::size_t size) override
        {
            const char* p = static_cast<const char*>(data);
            while (size > 0)
            {
                const ssize_t n = write(in_, p, size);
                if (n <= 0)
                {
                    return false;
                }
                p += n;
                size -= std::size_t(n);
            }
            return true;
        }

        void CloseInput() override
        {
            if (in_ >= 0)
            {
                close(in_);
                in_ = -1;
            }
        }

        std::size_t Available() override
        {
            int n = 0;
            if (out_ < 0 || ioctl(out_, FIONREAD, &n) != 0)
            {
                return 0;
            }
            return std::size_t(n);
        }

        bool Read(void* data, std::size_t size) override
        {
            char* p = static_cast<char*>(data);
            while (size > 0)
            {
                const ssize_t n = read(out_, p, size);
                if (n <= 0)
                {
                    return false;
                }
                p += n;
                size -= std::size_t(n);
            }
            return true;
        }

        bool Running() override
        {
            if (done_)
            {
                return false;
            }
            int status = 0;
            const pid_t r = waitpid(pid_, &status, WNOHANG);
            if (r == pid_)
            {
                done_ = true;
                code_ = WIFEXITED(status) ? WEXITSTATUS(status) : 255;
                return false;
            }
            return true;
        }

        bool Finish(int& exitCode, const std::function<bool()>& keepGoing) override
        {
            while (Running())
            {
                if (!keepGoing())
                {
                    Kill();
                    while (Running())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                    exitCode = code_;
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            exitCode = code_;
            return true;
        }

        void Kill() override
        {
            if (!done_ && Running())
            {
                kill(pid_, SIGKILL);
            }
        }

        std::string Log() override
        {
            std::ifstream f(log_, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        }

    private:
        PosixProcess() = default;
        pid_t pid_ = -1;
        int in_ = -1;
        int out_ = -1;
        bool done_ = false;
        int code_ = 0;
        std::string log_;
    };

    std::string Narrow(const std::wstring& w)
    {
        std::string s = rf::ToUtf8(w);
        for (auto& c : s)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        return s;
    }

    bool DecodePpm(const std::vector<unsigned char>& data, rf::Image& out)
    {
        if (data.size() < 2 || data[0] != 'P' || data[1] != '6')
        {
            return false;
        }
        int w = 0, h = 0, maxv = 0, consumed = 0;
        const std::string head(data.begin(), data.begin() + std::min<std::size_t>(data.size(), 64));
        if (std::sscanf(head.c_str(), "P6 %d %d %d%n", &w, &h, &maxv, &consumed) != 3 || w <= 0 || h <= 0)
        {
            return false;
        }
        const std::size_t start = std::size_t(consumed) + 1;
        if (data.size() < start + std::size_t(w) * std::size_t(h) * 3u)
        {
            return false;
        }
        out = rf::Image(w, h);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const unsigned char* p = data.data() + start + (std::size_t(y) * std::size_t(w) + std::size_t(x)) * 3u;
                const bool key = p[0] == 255 && p[1] == 0 && p[2] == 255;
                out.Set(x, y, key ? 0 : rf::Rgb(p[0], p[1], p[2]));
            }
        }
        return true;
    }

    std::vector<unsigned char> Slurp(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    class FileSource : public rf::ByteSource
    {
    public:
        explicit FileSource(std::vector<unsigned char> data) : data_(std::move(data)) {}
        std::uint64_t Size() const override { return data_.size(); }
        bool Read(std::uint64_t offset, void* dst, std::size_t size) override
        {
            if (offset + size > data_.size())
            {
                return false;
            }
            std::copy(data_.begin() + long(offset), data_.begin() + long(offset + size), static_cast<unsigned char*>(dst));
            return true;
        }

    private:
        std::vector<unsigned char> data_;
    };

    class FakeAudio : public rf::AudioOut
    {
    public:
        FakeAudio(long long& bytes, int& open) : bytes_(bytes), open_(open) { ++open_; }
        ~FakeAudio() override { --open_; }
        std::size_t Space() override { return 1u << 16; }
        bool Write(const void*, std::size_t size) override
        {
            bytes_ += (long long)size;
            return true;
        }

    private:
        long long& bytes_;
        int& open_;
    };

    class TestHost : public rf::Host
    {
    public:
        long long audioBytes = 0;
        int audioOpen = 0;
        bool audioDevice = true;
        std::wstring folderAnswer;
        std::wstring archiveAnswer;
        std::wstring videoAnswer;
        std::wstring lyricsAnswer;
        std::wstring saveAnswer;
        std::wstring ffmpeg;
        std::wstring written;
        rf::Image writtenImage;
        rf::Image clipboard;
        std::wstring sevenZip;
        int modalCalls = 0;
        std::function<int(rf::View&)> onModal;

        std::vector<std::wstring> AskOpenImages(const std::wstring&) override { return {}; }
        std::wstring AskOpenIcon(const std::wstring&) override { return L""; }
        std::wstring AskSavePath(const std::wstring&, const std::wstring&, rf::ImageFormat) override { return saveAnswer; }
        std::wstring AskFolder(const std::wstring&, const std::wstring&) override { return folderAnswer; }
        std::wstring AskOpenArchive(const std::wstring&) override { return archiveAnswer; }
        std::wstring AskOpenVideo(const std::wstring&) override { return videoAnswer; }
        std::wstring AskOpenAudio(const std::wstring&) override { return videoAnswer; }
        std::wstring AskOpenLyrics(const std::wstring&) override { return lyricsAnswer; }
        std::unique_ptr<rf::AudioOut> OpenAudio(int, int) override
        {
            if (!audioDevice)
            {
                return nullptr;
            }
            return std::make_unique<FakeAudio>(audioBytes, audioOpen);
        }
        std::wstring AskSaveVideo(const std::wstring&, const std::wstring&, bool) override { return saveAnswer; }
        bool AskColor(rf::Color, rf::Color&) override { return false; }
        bool ReadImage(const std::wstring& path, rf::Image& out, std::wstring& e) override { return ReadIcon(path, out, e); }
        bool ReadIcon(const std::wstring& path, rf::Image& out, std::wstring& e) override
        {
            if (!DecodePpm(Slurp(Narrow(path)), out))
            {
                e = L"плохой файл";
                return false;
            }
            return true;
        }
        bool WriteImage(const std::wstring& path, const rf::Image& image, rf::ImageFormat, int, std::wstring&) override
        {
            written = path;
            writtenImage = image;
            return true;
        }
        bool DecodeIcon(const std::vector<unsigned char>& data, rf::Image& out, std::wstring& e) override
        {
            if (!DecodePpm(data, out))
            {
                e = L"плохой файл";
                return false;
            }
            return true;
        }
        bool ReadFileIcons(const std::wstring&, std::vector<rf::Image>&, std::wstring&) override { return false; }
        bool ClipboardImage(rf::Image&) override { return false; }
        std::vector<std::wstring> ClipboardFiles() override { return {}; }
        bool SetClipboardImage(const rf::Image& image) override
        {
            clipboard = image;
            return true;
        }
        bool FileExists(const std::wstring& path) override { return fs::exists(Narrow(path)); }
        std::wstring SettingsFolder() override { return L""; }
        bool ReadText(const std::wstring& path, std::string& out) override
        {
            if (!fs::exists(Narrow(path)))
            {
                return false;
            }
            const auto bytes = Slurp(Narrow(path));
            out.assign(bytes.begin(), bytes.end());
            return true;
        }
        bool WriteText(const std::wstring&, const std::string&) override { return true; }
        bool RemoveFile(const std::wstring& path) override
        {
            std::error_code ec;
            return fs::remove(Narrow(path), ec);
        }
        bool ListFolder(const std::wstring& path, std::vector<rf::FolderItem>& out) override
        {
            std::error_code ec;
            fs::directory_iterator it(Narrow(path), ec);
            if (ec)
            {
                return false;
            }
            for (const auto& e : it)
            {
                rf::FolderItem item;
                item.name = rf::FromUtf8(e.path().filename().string());
                item.folder = e.is_directory();
                item.size = item.folder ? 0 : std::uint64_t(e.file_size());
                out.push_back(item);
            }
            return true;
        }
        std::unique_ptr<rf::ByteSource> OpenFile(const std::wstring& path) override
        {
            if (!fs::exists(Narrow(path)))
            {
                return nullptr;
            }
            return std::make_unique<FileSource>(Slurp(Narrow(path)));
        }
        std::wstring SevenZipPath() override { return sevenZip; }
        bool ExtractArchive(const std::wstring&, const std::wstring&, const std::function<bool()>&, std::wstring& e) override
        {
            e = L"нет 7-Zip";
            return false;
        }
        std::wstring NewTempFolder() override { return L""; }
        void RemoveFolder(const std::wstring&) override {}
        std::wstring FfmpegPath() override { return ffmpeg; }
        std::unique_ptr<rf::Process> StartProcess(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output) override
        {
            return PosixProcess::Start(Narrow(exe), args, input, output);
        }
        int RunModal(rf::View& view) override
        {
            ++modalCalls;
            return onModal ? onModal(view) : 0;
        }
        void* OpenModeless(rf::View&) override { return nullptr; }
        void CloseModeless(void*) override {}
        void Pump() override {}
        void Minimize() override {}
        void Quit() override {}
        unsigned long long Ticks() override { return 0; }
    };
}
