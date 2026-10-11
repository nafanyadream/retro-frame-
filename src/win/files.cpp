#include "files.h"

#include "../core/utf.h"

namespace rf::win
{
    namespace
    {
        class WinFileSource : public ByteSource
        {
        public:
            WinFileSource(HANDLE file, std::uint64_t size) : file_(file), size_(size) {}
            ~WinFileSource() override { CloseHandle(file_); }
            WinFileSource(const WinFileSource&) = delete;
            WinFileSource& operator=(const WinFileSource&) = delete;

            std::uint64_t Size() const override { return size_; }

            bool Read(std::uint64_t offset, void* dst, std::size_t size) override
            {
                auto* out = static_cast<unsigned char*>(dst);
                while (size > 0)
                {
                    const DWORD chunk = DWORD(std::min<std::size_t>(size, 16u * 1024u * 1024u));
                    OVERLAPPED ov{};
                    ov.Offset = DWORD(offset & 0xFFFFFFFFull);
                    ov.OffsetHigh = DWORD(offset >> 32);
                    DWORD read = 0;
                    if (!ReadFile(file_, out, chunk, &read, &ov) || read != chunk)
                    {
                        return false;
                    }
                    out += chunk;
                    offset += chunk;
                    size -= chunk;
                }
                return true;
            }

        private:
            HANDLE file_;
            std::uint64_t size_;
        };

        std::wstring ModuleFolder()
        {
            wchar_t buf[MAX_PATH] = {};
            const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            if (n == 0 || n >= MAX_PATH)
            {
                return L"";
            }
            return DirectoryOf(buf);
        }

        bool IsFile(const std::wstring& path)
        {
            const DWORD a = GetFileAttributesW(path.c_str());
            return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        std::wstring RegistryFolder(HKEY root, DWORD view, const wchar_t* value)
        {
            wchar_t buf[MAX_PATH] = {};
            DWORD size = sizeof(buf);
            if (RegGetValueW(root, L"SOFTWARE\\7-Zip", value, RRF_RT_REG_SZ | view, nullptr, buf, &size) != ERROR_SUCCESS)
            {
                return L"";
            }
            return buf;
        }

        std::wstring EnvFolder(const wchar_t* name)
        {
            wchar_t buf[MAX_PATH] = {};
            const DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
            return n > 0 && n < MAX_PATH ? std::wstring(buf) : std::wstring();
        }

        std::wstring Quote(const std::wstring& s)
        {
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

        std::wstring TempRoot()
        {
            wchar_t buf[MAX_PATH + 1] = {};
            const DWORD n = GetTempPathW(MAX_PATH + 1, buf);
            if (n == 0 || n > MAX_PATH)
            {
                return L"";
            }
            return buf;
        }

        void DeleteTree(const std::wstring& path, int depth)
        {
            if (depth > 64)
            {
                return;
            }
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(JoinPath(path, L"*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE)
            {
                do
                {
                    const std::wstring name = fd.cFileName;
                    if (name == L"." || name == L"..")
                    {
                        continue;
                    }
                    const std::wstring child = JoinPath(path, name);
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                    {
                        DeleteTree(child, depth + 1);
                    }
                    else if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    {
                        RemoveDirectoryW(child.c_str());
                    }
                    else
                    {
                        SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                        DeleteFileW(child.c_str());
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            RemoveDirectoryW(path.c_str());
        }
    }

    bool ListFolderItems(const std::wstring& path, std::vector<FolderItem>& out)
    {
        out.clear();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW(JoinPath(path, L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE)
        {
            const DWORD a = GetFileAttributesW(path.c_str());
            return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
        }
        do
        {
            FolderItem item;
            item.name = fd.cFileName;
            if (item.name == L"." || item.name == L"..")
            {
                continue;
            }
            const bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (dir && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                continue;
            }
            item.folder = dir;
            item.size = (std::uint64_t(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            out.push_back(std::move(item));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return true;
    }

    std::unique_ptr<ByteSource> OpenFileSource(const std::wstring& path)
    {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
        {
            return nullptr;
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(f, &size))
        {
            CloseHandle(f);
            return nullptr;
        }
        return std::make_unique<WinFileSource>(f, std::uint64_t(size.QuadPart));
    }

    std::wstring FindSevenZip()
    {
        std::vector<std::wstring> candidates;
        const std::wstring here = ModuleFolder();
        if (!here.empty())
        {
            candidates.push_back(JoinPath(here, L"7za.exe"));
            candidates.push_back(JoinPath(here, L"7z.exe"));
        }
        for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE})
        {
            for (DWORD view : {DWORD(RRF_SUBKEY_WOW6464KEY), DWORD(RRF_SUBKEY_WOW6432KEY)})
            {
                for (const wchar_t* value : {L"Path64", L"Path"})
                {
                    const std::wstring dir = RegistryFolder(root, view, value);
                    if (!dir.empty())
                    {
                        candidates.push_back(JoinPath(dir, L"7z.exe"));
                    }
                }
            }
        }
        for (const wchar_t* env : {L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)"})
        {
            const std::wstring dir = EnvFolder(env);
            if (!dir.empty())
            {
                candidates.push_back(JoinPath(JoinPath(dir, L"7-Zip"), L"7z.exe"));
            }
        }
        for (const auto& c : candidates)
        {
            if (IsFile(c))
            {
                return c;
            }
        }
        for (const wchar_t* name : {L"7z.exe", L"7za.exe"})
        {
            wchar_t buf[MAX_PATH] = {};
            const DWORD n = SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr);
            if (n > 0 && n < MAX_PATH)
            {
                return buf;
            }
        }
        return L"";
    }

    bool RunSevenZip(const std::wstring& tool, const std::wstring& archive, const std::wstring& folder, const std::function<bool()>& keepGoing, std::wstring& error)
    {
        std::wstring cmd = Quote(tool) + L" x -y -bd -r -pretroframe " + Quote(L"-o" + folder) + L" " + Quote(archive) + L" *.ico *.png";
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = nul;
        si.hStdOutput = nul;
        si.hStdError = nul;
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> line(cmd.begin(), cmd.end());
        line.push_back(L'\0');
        const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, folder.c_str(), &si, &pi);
        if (nul != INVALID_HANDLE_VALUE)
        {
            CloseHandle(nul);
        }
        if (!started)
        {
            error = L"не удалось запустить 7-Zip";
            return false;
        }
        CloseHandle(pi.hThread);
        bool cancelled = false;
        for (;;)
        {
            const DWORD r = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 50, QS_ALLINPUT);
            if (r == WAIT_OBJECT_0)
            {
                break;
            }
            if (!keepGoing())
            {
                TerminateProcess(pi.hProcess, 255);
                WaitForSingleObject(pi.hProcess, 5000);
                cancelled = true;
                break;
            }
        }
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        if (cancelled)
        {
            error = L"распаковка отменена";
            return false;
        }
        if (code > 1)
        {
            error = code == 2 ? L"7-Zip не смог распаковать архив (повреждён или защищён паролем)" : L"7-Zip завершился с ошибкой " + std::to_wstring(code);
            return false;
        }
        return true;
    }

    std::wstring CreateTempFolder()
    {
        const std::wstring root = TempRoot();
        if (root.empty())
        {
            return L"";
        }
        for (int i = 0; i < 100; ++i)
        {
            const std::wstring dir = JoinPath(root, L"RetroFrame-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64() % 1000000ull) + L"-" + std::to_wstring(i));
            if (CreateDirectoryW(dir.c_str(), nullptr))
            {
                return dir;
            }
        }
        return L"";
    }

    void DeleteTempFolder(const std::wstring& path)
    {
        const std::wstring root = TempRoot();
        if (path.empty() || root.empty() || ToLower(path).rfind(ToLower(root), 0) != 0 || FileNameOf(path).rfind(L"RetroFrame-", 0) != 0)
        {
            return;
        }
        DeleteTree(path, 0);
    }
}
