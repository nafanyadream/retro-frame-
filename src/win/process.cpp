#include "process.h"

#include "../core/utf.h"

namespace rf::win
{
    namespace
    {
        std::wstring TempLogPath()
        {
            wchar_t dir[MAX_PATH + 1] = {};
            wchar_t file[MAX_PATH + 1] = {};
            if (GetTempPathW(MAX_PATH + 1, dir) == 0 || GetTempFileNameW(dir, L"rfl", 0, file) == 0)
            {
                return L"";
            }
            return file;
        }

        class ChildProcess : public Process
        {
        public:
            ChildProcess() = default;
            ChildProcess(const ChildProcess&) = delete;
            ChildProcess& operator=(const ChildProcess&) = delete;

            ~ChildProcess() override
            {
                Kill();
                int code = 0;
                Finish(code, []() { return true; });
                CloseInput();
                if (output_ != nullptr)
                {
                    CloseHandle(output_);
                }
                if (process_ != nullptr)
                {
                    CloseHandle(process_);
                }
                if (!log_.empty())
                {
                    DeleteFileW(log_.c_str());
                }
            }

            bool Write(const void* data, std::size_t size) override
            {
                const auto* p = static_cast<const unsigned char*>(data);
                while (size > 0 && input_ != nullptr)
                {
                    const DWORD chunk = DWORD(std::min<std::size_t>(size, 1u << 20));
                    DWORD written = 0;
                    if (!WriteFile(input_, p, chunk, &written, nullptr) || written == 0)
                    {
                        return false;
                    }
                    p += written;
                    size -= written;
                }
                return size == 0;
            }

            void CloseInput() override
            {
                if (input_ != nullptr)
                {
                    CloseHandle(input_);
                    input_ = nullptr;
                }
            }

            std::size_t Available() override
            {
                DWORD avail = 0;
                if (output_ == nullptr || !PeekNamedPipe(output_, nullptr, 0, nullptr, &avail, nullptr))
                {
                    return 0;
                }
                return avail;
            }

            bool Read(void* data, std::size_t size) override
            {
                auto* p = static_cast<unsigned char*>(data);
                while (size > 0 && output_ != nullptr)
                {
                    const DWORD chunk = DWORD(std::min<std::size_t>(size, 1u << 20));
                    DWORD got = 0;
                    if (!ReadFile(output_, p, chunk, &got, nullptr) || got == 0)
                    {
                        return false;
                    }
                    p += got;
                    size -= got;
                }
                return size == 0;
            }

            bool Running() override
            {
                return process_ != nullptr && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
            }

            bool Finish(int& exitCode, const std::function<bool()>& keepGoing) override
            {
                if (process_ == nullptr)
                {
                    exitCode = -1;
                    return true;
                }
                bool ok = true;
                for (;;)
                {
                    if (WaitForSingleObject(process_, 30) == WAIT_OBJECT_0)
                    {
                        break;
                    }
                    if (!keepGoing())
                    {
                        TerminateProcess(process_, 255);
                        WaitForSingleObject(process_, 5000);
                        ok = false;
                        break;
                    }
                }
                DWORD code = 0;
                GetExitCodeProcess(process_, &code);
                exitCode = int(code);
                return ok;
            }

            void Kill() override
            {
                if (Running())
                {
                    TerminateProcess(process_, 255);
                    WaitForSingleObject(process_, 5000);
                }
            }

            std::string Log() override
            {
                std::string text;
                if (log_.empty())
                {
                    return text;
                }
                HANDLE f = CreateFileW(log_.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
                if (f == INVALID_HANDLE_VALUE)
                {
                    return text;
                }
                LARGE_INTEGER size{};
                GetFileSizeEx(f, &size);
                const LONGLONG keep = 256 * 1024;
                if (size.QuadPart > keep)
                {
                    LARGE_INTEGER at{};
                    at.QuadPart = size.QuadPart - keep;
                    SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
                }
                text.resize(std::size_t(std::min(size.QuadPart, keep)));
                DWORD got = 0;
                if (!text.empty() && ReadFile(f, text.data(), DWORD(text.size()), &got, nullptr))
                {
                    text.resize(got);
                }
                CloseHandle(f);
                return text;
            }

            HANDLE process_ = nullptr;
            HANDLE input_ = nullptr;
            HANDLE output_ = nullptr;
            std::wstring log_;
        };

        bool IsFile(const std::wstring& path)
        {
            const DWORD a = GetFileAttributesW(path.c_str());
            return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }
    }

    std::wstring QuoteArg(const std::wstring& s)
    {
        if (!s.empty() && s.find_first_of(L" \t\"") == std::wstring::npos)
        {
            return s;
        }
        std::wstring out = L"\"";
        std::size_t slashes = 0;
        for (wchar_t c : s)
        {
            if (c == L'\\')
            {
                ++slashes;
                out.push_back(c);
                continue;
            }
            if (c == L'"')
            {
                out.append(slashes + 1, L'\\');
            }
            slashes = 0;
            out.push_back(c);
        }
        out.append(slashes, L'\\');
        out.push_back(L'"');
        return out;
    }

    std::unique_ptr<Process> StartChild(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output)
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        auto child = std::make_unique<ChildProcess>();
        HANDLE childIn = nullptr;
        HANDLE childOut = nullptr;
        HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        if (input)
        {
            if (!CreatePipe(&childIn, &child->input_, &sa, 4u << 20))
            {
                CloseHandle(nul);
                return nullptr;
            }
            SetHandleInformation(child->input_, HANDLE_FLAG_INHERIT, 0);
        }
        if (output)
        {
            if (!CreatePipe(&child->output_, &childOut, &sa, 4u << 20))
            {
                if (childIn != nullptr)
                {
                    CloseHandle(childIn);
                }
                CloseHandle(nul);
                return nullptr;
            }
            SetHandleInformation(child->output_, HANDLE_FLAG_INHERIT, 0);
        }
        child->log_ = TempLogPath();
        HANDLE logFile = child->log_.empty() ? INVALID_HANDLE_VALUE
                                             : CreateFileW(child->log_.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, CREATE_ALWAYS,
                                                           FILE_ATTRIBUTE_TEMPORARY, nullptr);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = input ? childIn : nul;
        si.hStdOutput = output ? childOut : nul;
        si.hStdError = logFile != INVALID_HANDLE_VALUE ? logFile : nul;
        std::wstring cmd = QuoteArg(exe);
        for (const auto& a : args)
        {
            cmd += L" " + QuoteArg(a);
        }
        std::vector<wchar_t> line(cmd.begin(), cmd.end());
        line.push_back(L'\0');
        PROCESS_INFORMATION pi{};
        const BOOL started = CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (childIn != nullptr)
        {
            CloseHandle(childIn);
        }
        if (childOut != nullptr)
        {
            CloseHandle(childOut);
        }
        if (logFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(logFile);
        }
        if (nul != INVALID_HANDLE_VALUE)
        {
            CloseHandle(nul);
        }
        if (!started)
        {
            return nullptr;
        }
        CloseHandle(pi.hThread);
        child->process_ = pi.hProcess;
        return child;
    }

    std::wstring FindFfmpeg()
    {
        wchar_t buf[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            const std::wstring here = DirectoryOf(buf);
            for (const wchar_t* rel : {L"ffmpeg.exe", L"ffmpeg\\bin\\ffmpeg.exe", L"bin\\ffmpeg.exe"})
            {
                const std::wstring p = JoinPath(here, rel);
                if (IsFile(p))
                {
                    return p;
                }
            }
        }
        wchar_t found[MAX_PATH] = {};
        const DWORD m = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr);
        if (m > 0 && m < MAX_PATH)
        {
            return found;
        }
        for (const wchar_t* p : {L"C:\\ffmpeg\\bin\\ffmpeg.exe", L"C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe"})
        {
            if (IsFile(p))
            {
                return p;
            }
        }
        return L"";
    }
}
