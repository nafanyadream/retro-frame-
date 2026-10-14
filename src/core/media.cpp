#include "media.h"

#include "utf.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace rf
{
    namespace
    {
        std::wstring Number(double v)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.3f", v);
            return FromUtf8(buf);
        }

        std::wstring Int(int v)
        {
            return std::to_wstring(v);
        }

        double ReadNumber(const std::string& s, std::size_t pos, std::size_t* end)
        {
            std::size_t i = pos;
            double v = 0.0;
            bool any = false;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9')
            {
                v = v * 10.0 + (s[i] - '0');
                ++i;
                any = true;
            }
            if (i < s.size() && s[i] == '.')
            {
                ++i;
                double scale = 0.1;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9')
                {
                    v += (s[i] - '0') * scale;
                    scale *= 0.1;
                    ++i;
                    any = true;
                }
            }
            if (end != nullptr)
            {
                *end = any ? i : pos;
            }
            return v;
        }

        double NumberBefore(const std::string& line, const std::string& suffix)
        {
            std::size_t at = line.find(suffix);
            while (at != std::string::npos)
            {
                std::size_t b = at;
                double mul = 1.0;
                if (b > 0 && line[b - 1] == 'k')
                {
                    mul = 1000.0;
                    --b;
                }
                std::size_t a = b;
                while (a > 0 && ((line[a - 1] >= '0' && line[a - 1] <= '9') || line[a - 1] == '.'))
                {
                    --a;
                }
                if (a < b)
                {
                    return ReadNumber(line, a, nullptr) * mul;
                }
                at = line.find(suffix, at + 1);
            }
            return 0.0;
        }

        bool FrameSize(const std::string& line, int& w, int& h)
        {
            for (std::size_t i = 1; i + 2 < line.size(); ++i)
            {
                if (line[i] != 'x' || !(line[i - 1] >= '0' && line[i - 1] <= '9'))
                {
                    continue;
                }
                std::size_t a = i;
                while (a > 0 && line[a - 1] >= '0' && line[a - 1] <= '9')
                {
                    --a;
                }
                if (a == 0 || (line[a - 1] != ' ' && line[a - 1] != ','))
                {
                    continue;
                }
                std::size_t end = 0;
                const double ww = ReadNumber(line, a, nullptr);
                const double hh = ReadNumber(line, i + 1, &end);
                if (end == i + 1 || ww < 2 || hh < 2 || ww > 20000 || hh > 20000)
                {
                    continue;
                }
                if (end < line.size() && line[end] != ' ' && line[end] != ',' && line[end] != '[')
                {
                    continue;
                }
                w = int(ww);
                h = int(hh);
                return true;
            }
            return false;
        }

        std::size_t Utf8Offset(const std::string& s, std::size_t chars)
        {
            std::size_t n = 0;
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
                {
                    if (n == chars)
                    {
                        return i;
                    }
                    ++n;
                }
            }
            return s.size();
        }

        std::wstring ExitCodeText(int code)
        {
            const unsigned status = static_cast<unsigned>(code);
            if (status <= 0xFFFFu)
            {
                return std::to_wstring(status);
            }
            wchar_t buf[16];
            std::swprintf(buf, 16, L"%08X", status);
            return buf;
        }

        std::wstring Extension(const std::wstring& path)
        {
            const std::wstring file = FileNameOf(path);
            const std::size_t dot = file.rfind(L'.');
            return dot == std::wstring::npos ? std::wstring() : ToLower(file.substr(dot));
        }

        std::wstring TwoDigits(int v)
        {
            return (v < 10 ? L"0" : L"") + std::to_wstring(v);
        }

        void Blit(Image& dst, const Image& src, int x0, int y0)
        {
            const Rect r = Rect{x0, y0, src.width, src.height}.Intersect(Rect{0, 0, dst.width, dst.height});
            for (int y = r.y; y < r.Bottom(); ++y)
            {
                const Color* s = src.Row(y - y0) + (r.x - x0);
                Color* d = dst.Row(y) + r.x;
                std::memcpy(d, s, std::size_t(r.w) * sizeof(Color));
            }
        }

        void BlitVideo(Image& dst, const Image& src, int x0, int y0)
        {
            const Rect r = Rect{x0, y0, src.width, src.height}.Intersect(Rect{0, 0, dst.width, dst.height});
            for (int y = r.y; y < r.Bottom(); ++y)
            {
                const Color* s = src.Row(y - y0) + (r.x - x0);
                Color* d = dst.Row(y) + r.x;
                for (int x = 0; x < r.w; ++x)
                {
                    const Color c = s[x];
                    d[x] = Alpha(c) == 255 ? c : Opaque(Over(Rgb(0, 0, 0), c));
                }
            }
        }
    }

    bool ParseProbe(const std::string& log, MediaInfo& info)
    {
        info = MediaInfo();
        std::size_t start = 0;
        int rotation = 0;
        bool tags = false;
        while (start < log.size())
        {
            std::size_t end = log.find('\n', start);
            if (end == std::string::npos)
            {
                end = log.size();
            }
            std::string line = log.substr(start, end - start);
            start = end + 1;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            const std::size_t d = line.find("Duration: ");
            if (d != std::string::npos && info.duration <= 0.0)
            {
                const std::size_t p = d + 10;
                std::size_t e1 = 0;
                std::size_t e2 = 0;
                std::size_t e3 = 0;
                const double hh = ReadNumber(line, p, &e1);
                if (e1 > p && e1 < line.size() && line[e1] == ':')
                {
                    const double mm = ReadNumber(line, e1 + 1, &e2);
                    if (e2 > e1 + 1 && e2 < line.size() && line[e2] == ':')
                    {
                        const double ss = ReadNumber(line, e2 + 1, &e3);
                        if (e3 > e2 + 1)
                        {
                            info.duration = hh * 3600.0 + mm * 60.0 + ss;
                        }
                    }
                }
            }
            if (line.find("Stream #") != std::string::npos)
            {
                tags = line.find("Audio:") != std::string::npos;
            }
            else if (line.rfind("Input #", 0) == 0)
            {
                tags = true;
            }
            else if (tags && line.size() > 4 && line.compare(0, 4, "    ") == 0)
            {
                const std::size_t colon = line.find(" : ");
                if (colon != std::string::npos)
                {
                    std::string key = line.substr(0, colon);
                    key.erase(0, key.find_first_not_of(' '));
                    key.erase(key.find_last_not_of(' ') + 1);
                    for (auto& c : key)
                    {
                        c = char(std::tolower(static_cast<unsigned char>(c)));
                    }
                    std::string value = line.substr(colon + 3);
                    value.erase(value.find_last_not_of(" \t") + 1);
                    if (key == "artist" && info.artist.empty())
                    {
                        info.artist = FromUtf8(value);
                    }
                    else if (key == "album_artist" && info.artist.empty())
                    {
                        info.artist = FromUtf8(value);
                    }
                    else if (key == "title" && info.title.empty())
                    {
                        info.title = FromUtf8(value);
                    }
                }
            }
            if (line.find("Stream #") != std::string::npos)
            {
                if (line.find("Video:") != std::string::npos && line.find("(attached pic)") != std::string::npos && !info.hasCover)
                {
                    int w = 0;
                    int h = 0;
                    if (FrameSize(line, w, h))
                    {
                        info.hasCover = true;
                        info.coverW = w;
                        info.coverH = h;
                    }
                }
                if (line.find("Video:") != std::string::npos && line.find("(attached pic)") == std::string::npos && !info.hasVideo)
                {
                    int w = 0;
                    int h = 0;
                    if (FrameSize(line, w, h))
                    {
                        info.hasVideo = true;
                        info.width = w;
                        info.height = h;
                        const std::size_t sar = line.find("[SAR ");
                        if (sar != std::string::npos)
                        {
                            std::size_t e1 = 0;
                            std::size_t e2 = 0;
                            const double a = ReadNumber(line, sar + 5, &e1);
                            if (e1 < line.size() && line[e1] == ':')
                            {
                                const double b = ReadNumber(line, e1 + 1, &e2);
                                if (a > 0 && b > 0 && std::fabs(a - b) > 1e-9)
                                {
                                    info.width = std::max(2, int(std::lround(w * a / b)));
                                }
                            }
                        }
                        info.fps = NumberBefore(line, " fps");
                        if (info.fps <= 0.0)
                        {
                            info.fps = NumberBefore(line, " tbr");
                        }
                    }
                }
                else if (line.find("Audio:") != std::string::npos)
                {
                    info.hasAudio = true;
                }
            }
            const std::size_t rot = line.find("rotation of ");
            if (rot != std::string::npos && info.hasVideo)
            {
                std::size_t p = rot + 12;
                const bool neg = p < line.size() && line[p] == '-';
                if (neg)
                {
                    ++p;
                }
                const double v = ReadNumber(line, p, nullptr);
                rotation = int(std::lround(neg ? -v : v));
            }
            const std::size_t rt = line.find("rotate ");
            if (rt != std::string::npos && line.find(':', rt) != std::string::npos && info.hasVideo)
            {
                std::size_t p = line.find(':', rt) + 1;
                while (p < line.size() && line[p] == ' ')
                {
                    ++p;
                }
                rotation = int(std::lround(ReadNumber(line, p, nullptr)));
            }
        }
        const int r = ((rotation % 360) + 360) % 360;
        if (r == 90 || r == 270)
        {
            std::swap(info.width, info.height);
        }
        if (info.fps <= 0.0 || info.fps > 240.0)
        {
            info.fps = 30.0;
        }
        return info.hasVideo || info.hasAudio;
    }

    std::wstring FormatClock(double seconds)
    {
        const long long total = std::max(0LL, (long long)std::floor(seconds + 1e-6));
        const int h = int(total / 3600);
        const int m = int((total / 60) % 60);
        const int s = int(total % 60);
        if (h > 0)
        {
            return std::to_wstring(h) + L":" + TwoDigits(m) + L":" + TwoDigits(s);
        }
        return TwoDigits(m) + L":" + TwoDigits(s);
    }

    std::wstring FormatPrecise(double seconds)
    {
        const long long tenths = std::max(0LL, (long long)std::floor(seconds * 10.0 + 1e-6));
        return FormatClock(double(tenths / 10)) + L"." + std::to_wstring(tenths % 10);
    }

    std::wstring FormatShort(double seconds)
    {
        const long long total = std::max(0LL, (long long)std::floor(seconds + 1e-6));
        const int h = int(total / 3600);
        const int m = int((total / 60) % 60);
        const int s = int(total % 60);
        if (h > 0)
        {
            return std::to_wstring(h) + L":" + TwoDigits(m) + L":" + TwoDigits(s);
        }
        return std::to_wstring(m) + L":" + TwoDigits(s);
    }

    bool ParseClock(const std::wstring& text, double& seconds)
    {
        const std::string s = ToUtf8(Trim(text));
        if (s.empty())
        {
            return false;
        }
        double total = 0.0;
        std::size_t pos = 0;
        int parts = 0;
        while (pos <= s.size())
        {
            std::size_t end = pos;
            std::string piece;
            while (end < s.size() && s[end] != ':')
            {
                piece.push_back(s[end] == ',' ? '.' : s[end]);
                ++end;
            }
            std::size_t used = 0;
            const double v = ReadNumber(piece, 0, &used);
            if (used != piece.size() || piece.empty())
            {
                return false;
            }
            total = total * 60.0 + v;
            ++parts;
            if (end >= s.size())
            {
                break;
            }
            pos = end + 1;
        }
        if (parts > 3)
        {
            return false;
        }
        seconds = total;
        return true;
    }

    bool IsVideoFile(const std::wstring& path)
    {
        const std::wstring ext = Extension(path);
        for (const wchar_t* e : {L".mp4", L".mov", L".avi", L".mkv", L".webm", L".m4v", L".wmv", L".mpg", L".mpeg", L".3gp", L".flv", L".ts"})
        {
            if (ext == e)
            {
                return true;
            }
        }
        return false;
    }

    bool IsAudioFile(const std::wstring& path)
    {
        const std::wstring ext = Extension(path);
        for (const wchar_t* e : {L".mp3", L".flac", L".wav", L".ogg", L".m4a", L".aac", L".wma", L".opus"})
        {
            if (ext == e)
            {
                return true;
            }
        }
        return false;
    }

    std::wstring FfmpegMissingText()
    {
        return L"Для видео и музыки нужна программа ffmpeg.\n\n"
               L"Скачайте ffmpeg-release-essentials.zip с сайта gyan.dev\n"
               L"(ссылка есть на ffmpeg.org в разделе Windows),\n"
               L"достаньте из папки bin файл ffmpeg.exe\n"
               L"и положите его рядом с RetroFrame.exe.\n\n"
               L"Фото работают и без неё.";
    }

    std::string LastLines(const std::string& log, int count)
    {
        std::vector<std::string> lines;
        std::string cur;
        for (char c : log)
        {
            if (c == '\n' || c == '\r')
            {
                if (!cur.empty())
                {
                    lines.push_back(cur);
                }
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
        {
            lines.push_back(cur);
        }
        std::string out;
        for (std::size_t i = lines.size() > std::size_t(count) ? lines.size() - std::size_t(count) : 0; i < lines.size(); ++i)
        {
            if (!out.empty())
            {
                out += "\n";
            }
            out += lines[i].size() > 120 ? lines[i].substr(0, 120) : lines[i];
        }
        return out;
    }

    std::string WrapText(const std::string& text, std::size_t width)
    {
        std::string out;
        std::size_t start = 0;
        for (;;)
        {
            std::size_t end = text.find('\n', start);
            const bool last = end == std::string::npos;
            if (last)
            {
                end = text.size();
            }
            std::string line = text.substr(start, end - start);
            for (std::size_t cut = Utf8Offset(line, width); width > 0 && cut < line.size(); cut = Utf8Offset(line, width))
            {
                const std::size_t space = line.rfind(' ', cut);
                if (space != std::string::npos && space > 0)
                {
                    cut = space;
                }
                out += line.substr(0, cut) + "\n";
                line.erase(0, cut);
                line.erase(0, line.find_first_not_of(' '));
            }
            out += line;
            if (last)
            {
                return out;
            }
            out += "\n";
            start = end + 1;
        }
    }

    std::wstring ProbeFailure(const std::wstring& ffmpeg, const std::string& log, int code)
    {
        if (log.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            const unsigned status = static_cast<unsigned>(code);
            std::wstring text = L"ffmpeg.exe не запускается";
            if (code != 0)
            {
                text += L" (код ошибки " + ExitCodeText(code) + L")";
            }
            text += L".\n\n";
            if (status == 0xC0000135u)
            {
                text += L"Похоже, это сборка «shared»: ей нужны DLL-файлы,\n"
                        L"которые лежали рядом с ffmpeg.exe в папке bin.\n"
                        L"Скопируйте их туда же, где сейчас ffmpeg.exe,\n"
                        L"или скачайте сборку, где ffmpeg.exe работает один:\n"
                        L"ffmpeg-release-essentials.zip с сайта gyan.dev.";
            }
            else
            {
                text += L"Попробуйте скачать его заново:\n"
                        L"ffmpeg-release-essentials.zip с сайта gyan.dev.";
            }
            return text + L"\n\nСейчас используется:\n" + ffmpeg;
        }
        const std::string tail = LastLines(log, 3);
        if (tail.find("No such file") != std::string::npos)
        {
            return L"файл не найден";
        }
        std::wstring reason = L"ffmpeg не смог прочитать файл.";
        if (tail.find("Permission denied") != std::string::npos)
        {
            reason = L"нет доступа к файлу.\nМожет быть, он ещё копируется или открыт в другой программе.";
        }
        else if (tail.find("moov atom not found") != std::string::npos || tail.find("Invalid data found") != std::string::npos)
        {
            reason = L"файл повреждён или записан не до конца.\nТак бывает, если он ещё скачивается или запись прервалась.";
        }
        std::string details;
        int streams = 0;
        std::size_t start = 0;
        while (start < log.size() && streams < 2)
        {
            std::size_t end = log.find('\n', start);
            if (end == std::string::npos)
            {
                end = log.size();
            }
            std::string line = log.substr(start, end - start);
            start = end + 1;
            if (line.find("Stream #") != std::string::npos)
            {
                line.erase(0, line.find_first_not_of(' '));
                while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                {
                    line.pop_back();
                }
                details += (details.empty() ? "" : "\n") + line;
                ++streams;
            }
        }
        if (details.empty())
        {
            details = LastLines(log, 2);
        }
        return reason + L"\n\nОтвет ffmpeg:\n" + FromUtf8(WrapText(details, 70));
    }

    bool ProbeMedia(Host& host, const std::wstring& path, MediaInfo& info, std::wstring& error)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            error = FfmpegMissingText();
            return false;
        }
        auto proc = host.StartProcess(ffmpeg, {L"-hide_banner", L"-nostdin", L"-i", path}, false, false);
        if (!proc)
        {
            error = L"не удалось запустить ffmpeg";
            return false;
        }
        int code = 0;
        proc->Finish(code, []() { return true; });
        const std::string log = proc->Log();
        if (!ParseProbe(log, info))
        {
            error = ProbeFailure(ffmpeg, log, code);
            return false;
        }
        if (!info.hasVideo)
        {
            error = L"в файле нет видео";
            return false;
        }
        return true;
    }

    bool ProbeAudio(Host& host, const std::wstring& path, MediaInfo& info, std::wstring& error)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            error = FfmpegMissingText();
            return false;
        }
        auto proc = host.StartProcess(ffmpeg, {L"-hide_banner", L"-nostdin", L"-i", path}, false, false);
        if (!proc)
        {
            error = L"не удалось запустить ffmpeg";
            return false;
        }
        int code = 0;
        proc->Finish(code, []() { return true; });
        const std::string log = proc->Log();
        if (!ParseProbe(log, info))
        {
            error = ProbeFailure(ffmpeg, log, code);
            return false;
        }
        if (!info.hasAudio)
        {
            error = L"в файле нет звука";
            return false;
        }
        return true;
    }

    bool ReadCover(Host& host, const std::wstring& path, const MediaInfo& info, int maxSide, Image& out)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty() || !info.hasCover || info.coverW < 2 || info.coverH < 2)
        {
            return false;
        }
        const double k = std::min(1.0, double(maxSide) / double(std::max(info.coverW, info.coverH)));
        const int w = std::max(2, int(std::lround(info.coverW * k)));
        const int h = std::max(2, int(std::lround(info.coverH * k)));
        auto proc = host.StartProcess(ffmpeg, {L"-v", L"error", L"-nostdin", L"-i", path, L"-map", L"0:v:0", L"-frames:v", L"1", L"-vf",
                                               L"scale=" + Int(w) + L":" + Int(h) + L":flags=area", L"-pix_fmt", L"bgra", L"-f", L"rawvideo", L"pipe:1"},
                                      false, true);
        if (!proc)
        {
            return false;
        }
        Image img(w, h);
        const bool ok = proc->Read(img.pixels.data(), img.pixels.size() * sizeof(Color));
        int code = 0;
        proc->Finish(code, []() { return true; });
        if (!ok)
        {
            return false;
        }
        out = std::move(img);
        return true;
    }

    std::vector<float> LevelsFromSamples(const std::vector<std::int16_t>& samples, int sampleRate)
    {
        std::vector<float> levels;
        const std::size_t window = std::size_t(std::max(1, sampleRate / LevelRate));
        for (std::size_t i = 0; i < samples.size(); i += window)
        {
            const std::size_t end = std::min(samples.size(), i + window);
            double sum = 0.0;
            for (std::size_t j = i; j < end; ++j)
            {
                const double v = samples[j] / 32768.0;
                sum += v * v;
            }
            const double rms = std::sqrt(sum / double(std::max<std::size_t>(1, end - i)));
            const double db = 20.0 * std::log10(std::max(rms, 1e-6));
            levels.push_back(float(std::clamp((db + 45.0) / 45.0, 0.0, 1.0)));
        }
        float hold = 0.0f;
        for (auto& l : levels)
        {
            hold = std::max(l, hold - 0.07f);
            l = hold;
        }
        return levels;
    }

    LevelReader::LevelReader(Host& host, const std::wstring& path)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (!ffmpeg.empty())
        {
            proc_ = host.StartProcess(ffmpeg, {L"-v", L"error", L"-nostdin", L"-i", path, L"-vn", L"-sn", L"-ac", L"1", L"-ar", Int(AnalysisRate), L"-f", L"s16le", L"pipe:1"},
                                      false, true);
        }
        done_ = proc_ == nullptr;
    }

    void LevelReader::Take(std::size_t n)
    {
        const std::size_t at = bytes_.size();
        bytes_.resize(at + n);
        if (!proc_->Read(bytes_.data() + at, n))
        {
            bytes_.resize(at);
            done_ = true;
        }
    }

    bool LevelReader::Poll()
    {
        if (done_)
        {
            return true;
        }
        for (int guard = 0; guard < 64; ++guard)
        {
            const std::size_t n = proc_->Available();
            if (n == 0)
            {
                if (!proc_->Running() && proc_->Available() == 0)
                {
                    done_ = true;
                }
                break;
            }
            Take(std::min<std::size_t>(n, 1u << 20));
            if (done_)
            {
                break;
            }
        }
        return done_;
    }

    bool LevelReader::Wait()
    {
        while (!done_)
        {
            const std::size_t at = bytes_.size();
            bytes_.resize(at + 65536);
            std::size_t got = 0;
            while (got < 65536)
            {
                const std::size_t n = proc_->Available();
                if (n == 0)
                {
                    if (!proc_->Running() && proc_->Available() == 0)
                    {
                        done_ = true;
                        break;
                    }
                    if (!proc_->Read(bytes_.data() + at + got, 1))
                    {
                        done_ = true;
                        break;
                    }
                    ++got;
                    continue;
                }
                const std::size_t take = std::min(n, std::size_t(65536) - got);
                if (!proc_->Read(bytes_.data() + at + got, take))
                {
                    done_ = true;
                    break;
                }
                got += take;
            }
            bytes_.resize(at + got);
        }
        return true;
    }

    std::vector<std::int16_t> LevelReader::Samples() const
    {
        std::vector<std::int16_t> samples(bytes_.size() / 2);
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            samples[i] = std::int16_t(std::uint16_t(bytes_[i * 2]) | (std::uint16_t(bytes_[i * 2 + 1]) << 8));
        }
        return samples;
    }

    std::vector<float> LevelReader::Levels() const
    {
        return LevelsFromSamples(Samples(), AnalysisRate);
    }

    std::vector<float> LevelReader::Spectrum() const
    {
        return SpectrumFromSamples(Samples(), AnalysisRate);
    }

    std::vector<float> SpectrumFromSamples(const std::vector<std::int16_t>& samples, int sampleRate)
    {
        constexpr int N = 1024;
        const int hop = std::max(1, sampleRate / LevelRate);
        const std::size_t frames = (samples.size() + std::size_t(hop) - 1) / std::size_t(hop);
        std::vector<float> out(frames * SpectrumBands, 0.0f);
        std::vector<double> window(N);
        for (int i = 0; i < N; ++i)
        {
            window[std::size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / (N - 1));
        }
        int edges[SpectrumBands + 1];
        for (int b = 0; b <= SpectrumBands; ++b)
        {
            const double f = 60.0 * std::pow(7000.0 / 60.0, double(b) / SpectrumBands);
            edges[b] = std::clamp(int(std::lround(f * N / sampleRate)), 1, N / 2 - 1);
        }
        for (int b = 1; b <= SpectrumBands; ++b)
        {
            edges[b] = std::max(edges[b], edges[b - 1] + 1);
        }
        std::vector<double> re(N);
        std::vector<double> im(N);
        float fall[SpectrumBands] = {};
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            const long long centre = (long long)(fr * std::size_t(hop)) + hop / 2;
            for (int i = 0; i < N; ++i)
            {
                const long long at = centre - N / 2 + i;
                const double v = at >= 0 && at < (long long)samples.size() ? samples[std::size_t(at)] / 32768.0 : 0.0;
                re[std::size_t(i)] = v * window[std::size_t(i)];
                im[std::size_t(i)] = 0.0;
            }
            for (int i = 1, j = 0; i < N; ++i)
            {
                int bit = N >> 1;
                for (; j & bit; bit >>= 1)
                {
                    j ^= bit;
                }
                j ^= bit;
                if (i < j)
                {
                    std::swap(re[std::size_t(i)], re[std::size_t(j)]);
                    std::swap(im[std::size_t(i)], im[std::size_t(j)]);
                }
            }
            for (int len = 2; len <= N; len <<= 1)
            {
                const double ang = -2.0 * 3.14159265358979 / len;
                const double wr = std::cos(ang);
                const double wi = std::sin(ang);
                for (int i = 0; i < N; i += len)
                {
                    double cr = 1.0;
                    double ci = 0.0;
                    for (int k = 0; k < len / 2; ++k)
                    {
                        const std::size_t a = std::size_t(i + k);
                        const std::size_t b = std::size_t(i + k + len / 2);
                        const double xr = re[b] * cr - im[b] * ci;
                        const double xi = re[b] * ci + im[b] * cr;
                        re[b] = re[a] - xr;
                        im[b] = im[a] - xi;
                        re[a] += xr;
                        im[a] += xi;
                        const double nr = cr * wr - ci * wi;
                        ci = cr * wi + ci * wr;
                        cr = nr;
                    }
                }
            }
            for (int b = 0; b < SpectrumBands; ++b)
            {
                double peak = 0.0;
                for (int k = edges[b]; k < edges[b + 1]; ++k)
                {
                    peak = std::max(peak, std::hypot(re[std::size_t(k)], im[std::size_t(k)]));
                }
                const double db = 20.0 * std::log10(std::max(peak / (N / 4.0), 1e-7)) + b * 0.9;
                const float v = float(std::clamp((db + 54.0) / 54.0, 0.0, 1.0));
                fall[b] = std::max(v, fall[b] - 0.09f);
                out[fr * SpectrumBands + std::size_t(b)] = fall[b];
            }
        }
        return out;
    }

    void SpectrumAt(const std::vector<float>& spectrum, double seconds, float* bars, float* peaks)
    {
        const long long frames = (long long)(spectrum.size() / SpectrumBands);
        for (int b = 0; b < SpectrumBands; ++b)
        {
            bars[b] = 0.0f;
            peaks[b] = 0.0f;
        }
        if (frames == 0)
        {
            return;
        }
        const long long i = std::clamp<long long>((long long)std::floor(seconds * LevelRate), 0, frames - 1);
        for (int b = 0; b < SpectrumBands; ++b)
        {
            bars[b] = spectrum[std::size_t(i) * SpectrumBands + std::size_t(b)];
            float peak = bars[b];
            for (long long d = 1; d <= 30 && i - d >= 0; ++d)
            {
                peak = std::max(peak, spectrum[std::size_t(i - d) * SpectrumBands + std::size_t(b)] - 0.0012f * float(d * d));
            }
            peaks[b] = peak;
        }
    }

    SoundPreview::SoundPreview(Host& host, const std::wstring& path, double start, double duration)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty() || duration <= 0.0)
        {
            return;
        }
        out_ = host.OpenAudio(Rate, Channels);
        if (!out_)
        {
            return;
        }
        std::vector<std::wstring> args = {L"-v", L"error", L"-nostdin"};
        if (start > 0.0)
        {
            args.insert(args.end(), {L"-ss", Number(start)});
        }
        args.insert(args.end(), {L"-t", Number(duration), L"-i", path, L"-vn", L"-sn", L"-ac", Int(Channels), L"-ar", Int(Rate), L"-f", L"s16le", L"pipe:1"});
        proc_ = host.StartProcess(ffmpeg, args, false, true);
        if (!proc_)
        {
            out_.reset();
        }
    }

    void SoundPreview::Fill(std::size_t limit)
    {
        while (!ended_ && buf_.size() < limit)
        {
            const std::size_t n = proc_->Available();
            if (n == 0)
            {
                if (!proc_->Running() && proc_->Available() == 0)
                {
                    ended_ = true;
                }
                break;
            }
            const std::size_t take = std::min(n, limit - buf_.size());
            const std::size_t at = buf_.size();
            buf_.resize(at + take);
            if (!proc_->Read(buf_.data() + at, take))
            {
                buf_.resize(at);
                ended_ = true;
            }
        }
    }

    void SoundPreview::Prime()
    {
        if (!out_)
        {
            return;
        }
        const std::size_t want = std::size_t(Rate) * Channels * 2 / 6;
        const std::size_t at = buf_.size();
        if (at < want)
        {
            buf_.resize(want);
            std::size_t got = at;
            while (got < want)
            {
                if (!proc_->Read(buf_.data() + got, 4))
                {
                    ended_ = true;
                    break;
                }
                got += 4;
                const std::size_t more = std::min(proc_->Available(), want - got) & ~std::size_t(3);
                if (more > 0)
                {
                    if (!proc_->Read(buf_.data() + got, more))
                    {
                        ended_ = true;
                        break;
                    }
                    got += more;
                }
            }
            buf_.resize(got);
        }
    }

    void SoundPreview::Pump(bool release)
    {
        if (!out_)
        {
            return;
        }
        Fill(std::size_t(Rate) * Channels * 2 * 2);
        if (!release || buf_.empty())
        {
            return;
        }
        const std::size_t n = std::min(out_->Space(), buf_.size()) & ~std::size_t(3);
        if (n > 0 && out_->Write(buf_.data(), n))
        {
            buf_.erase(buf_.begin(), buf_.begin() + std::ptrdiff_t(n));
        }
    }

    float MeterAt(const std::vector<float>& levels, double seconds)
    {
        if (levels.empty())
        {
            return 0.0f;
        }
        const long long i = (long long)std::floor(seconds * LevelRate);
        return levels[std::size_t(std::clamp<long long>(i, 0, (long long)levels.size() - 1))];
    }

    FrameStream::FrameStream(Host& host, const std::wstring& path, double start, double duration, double fps, int w, int h)
        : width(std::max(2, w)), height(std::max(2, h))
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            ended_ = true;
            return;
        }
        std::vector<std::wstring> args = {L"-v", L"error", L"-nostdin"};
        if (start > 0.0)
        {
            args.insert(args.end(), {L"-ss", Number(start)});
        }
        args.insert(args.end(), {L"-i", path});
        if (duration > 0.0)
        {
            args.insert(args.end(), {L"-t", Number(duration)});
        }
        args.insert(args.end(), {L"-an", L"-sn", L"-dn", L"-vf", L"scale=" + Int(width) + L":" + Int(height) + L":flags=area"});
        if (fps > 0.0)
        {
            args.insert(args.end(), {L"-r", Number(fps)});
        }
        else
        {
            args.insert(args.end(), {L"-frames:v", L"1"});
        }
        args.insert(args.end(), {L"-pix_fmt", L"bgra", L"-f", L"rawvideo", L"pipe:1"});
        proc_ = host.StartProcess(ffmpeg, args, false, true);
        if (!proc_)
        {
            ended_ = true;
            return;
        }
        buf_.resize(std::size_t(width) * std::size_t(height) * 4u);
    }

    FrameStream::~FrameStream()
    {
        Stop();
    }

    bool FrameStream::Finish(Image& out)
    {
        have_ = 0;
        out = Image(width, height);
        std::memcpy(out.pixels.data(), buf_.data(), buf_.size());
        return true;
    }

    bool FrameStream::Poll(Image& out)
    {
        if (ended_ || !proc_)
        {
            return false;
        }
        for (;;)
        {
            const std::size_t avail = proc_->Available();
            if (avail == 0)
            {
                if (!proc_->Running() && proc_->Available() == 0)
                {
                    ended_ = true;
                }
                return false;
            }
            const std::size_t take = std::min(avail, buf_.size() - have_);
            if (!proc_->Read(buf_.data() + have_, take))
            {
                ended_ = true;
                return false;
            }
            have_ += take;
            if (have_ == buf_.size())
            {
                return Finish(out);
            }
        }
    }

    bool FrameStream::Next(Image& out)
    {
        if (ended_ || !proc_)
        {
            return false;
        }
        if (!proc_->Read(buf_.data() + have_, buf_.size() - have_))
        {
            ended_ = true;
            return false;
        }
        return Finish(out);
    }

    void FrameStream::Stop()
    {
        if (proc_)
        {
            if (proc_->Running())
            {
                proc_->Kill();
            }
            int code = 0;
            proc_->Finish(code, []() { return true; });
        }
        ended_ = true;
    }

    std::string FrameStream::Log()
    {
        return proc_ ? proc_->Log() : std::string();
    }

    VideoPlan PlanVideo(const FrameSettings& frame, int sourceW, int sourceH, const VideoTarget& target)
    {
        VideoPlan p;
        FrameSettings f = frame;
        f.player = true;
        const FrameGeometry g = MeasureFrame(f, 100, 100);
        const int chromeW = g.width - 100;
        const int chromeH = g.height - 100;
        const int sw = std::max(2, sourceW);
        const int sh = std::max(2, sourceH);
        const double aspect = double(sw) / double(sh);
        const int fixed = frame.scale >= 1 && frame.scale <= 4 ? frame.scale : 0;
        const auto fit = [aspect](int maxW, int maxH, int& cw, int& ch) {
            cw = std::max(2, maxW);
            ch = std::max(2, int(std::lround(cw / aspect)));
            if (ch > maxH)
            {
                ch = std::max(2, maxH);
                cw = std::max(2, int(std::lround(ch * aspect)));
            }
        };
        const auto fill = [&p, aspect]() {
            const int k = p.scale;
            const int boxW = p.clientW * k;
            const int boxH = p.clientH * k;
            int vw = boxW;
            int vh = int(std::lround(vw / aspect));
            if (vh > boxH)
            {
                vh = boxH;
                vw = int(std::lround(vh * aspect));
            }
            const int slack = 2 * k + 2;
            p.videoW = std::max(2, boxW - vw <= slack ? boxW : vw);
            p.videoH = std::max(2, boxH - vh <= slack ? boxH : vh);
        };
        if (target.square)
        {
            int outW = std::max(2, target.canvasW);
            int outH = std::max(2, target.canvasH);
            if (target.width > 0 && target.width < outW)
            {
                outH = int(std::lround(double(outH) * target.width / outW));
                outW = target.width;
            }
            outW = std::max(120, outW & ~1);
            outH = std::max(120, outH & ~1);
            p.square = true;
            p.outW = outW;
            p.outH = outH;
            const int roomW = outW * 92 / 100;
            const int roomH = outH * 92 / 100;
            int k = fixed;
            const int tallEnough[] = {chromeH * 3 / 2, 40};
            const int wideEnough[] = {PlayerComfortWidth, PlayerMinWidth};
            for (int tier = 0; tier < 2; ++tier)
            {
                for (int cand = 4; cand >= 1 && k == 0; --cand)
                {
                    int cw = 0;
                    int ch = 0;
                    fit(roomW / cand - chromeW, roomH / cand - chromeH, cw, ch);
                    if (cw >= wideEnough[tier] && ch >= tallEnough[tier])
                    {
                        k = cand;
                    }
                }
            }
            k = std::max(1, k);
            p.scale = k;
            int cw = 0;
            int ch = 0;
            fit(roomW / k - chromeW, roomH / k - chromeH, cw, ch);
            if (cw < PlayerMinWidth)
            {
                cw = PlayerMinWidth;
                ch = std::min(std::max(2, int(std::lround(cw / aspect))), std::max(2, outH / k - chromeH));
            }
            p.clientW = cw;
            p.clientH = ch;
            fill();
            p.windowW = (p.clientW + chromeW) * k;
            p.windowH = (p.clientH + chromeH) * k;
            p.windowX = std::max(0, (outW - p.windowW) / 2);
            p.windowY = std::max(0, (outH - p.windowH) / 2);
            return p;
        }
        int k = fixed;
        int vw = sw;
        if (target.width > 0)
        {
            if (k == 0)
            {
                k = AutoScale(std::min(sw, target.width), std::min(sh, target.width));
            }
            vw = std::min(vw, std::max(PlayerMinWidth, target.width / k - chromeW) * k);
        }
        else if (k == 0)
        {
            k = AutoScale(sw, sh);
        }
        p.scale = k;
        p.clientW = std::max(PlayerMinWidth, int(std::lround(double(vw) / k)));
        p.clientH = std::max(2, int(std::lround(p.clientW / aspect)));
        if (((p.clientW + chromeW) * k) % 2 != 0)
        {
            ++p.clientW;
        }
        if (((p.clientH + chromeH) * k) % 2 != 0)
        {
            ++p.clientH;
        }
        fill();
        p.windowW = (p.clientW + chromeW) * k;
        p.windowH = (p.clientH + chromeH) * k;
        p.outW = p.windowW;
        p.outH = p.windowH;
        return p;
    }

    Image CanvasBackground(const Image& picture, Color color, int width, int height)
    {
        if (picture.Empty())
        {
            return Image(width, height, Opaque(color));
        }
        int cw = picture.width;
        int ch = int(std::lround(double(cw) * height / width));
        if (ch > picture.height)
        {
            ch = picture.height;
            cw = std::max(1, int(std::lround(double(ch) * width / height)));
        }
        const Image part = Crop(picture, Rect{(picture.width - cw) / 2, (picture.height - ch) / 2, cw, ch});
        Image out = ResizeSmooth(part, width, height);
        for (auto& c : out.pixels)
        {
            c = Alpha(c) == 255 ? c : Opaque(Over(Opaque(color), c));
        }
        return out;
    }

    PlayerComposer::PlayerComposer(const FrameSettings& frame, const FrameVars& vars, const VideoPlan& plan, const Image& background)
        : frame_(frame), vars_(vars), plan_(plan), background_(background)
    {
        frame_.player = true;
    }

    void PlayerComposer::SetEffects(const EffectSettings& effects)
    {
        effects_ = effects;
        const Rect window = plan_.square ? Rect{plan_.windowX, plan_.windowY, plan_.windowW, plan_.windowH} : Rect{0, 0, plan_.outW, plan_.outH};
        const int k = plan_.scale;
        const Rect client = MeasureFrame(frame_, plan_.clientW, plan_.clientH).client;
        const Rect video{window.x + client.x * k + (plan_.clientW * k - plan_.videoW) / 2, window.y + client.y * k + (plan_.clientH * k - plan_.videoH) / 2,
                         plan_.videoW, plan_.videoH};
        fx_ = EffectsLayer(effects, frame_, Stage{window, k, video});
        fxOn_ = HasOverlays(effects);
    }

    const Image& PlayerComposer::Intro(double t, int seed)
    {
        out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        fx_.DrawIntro(out_, plan_.square ? background_ : Image(), t);
        return out_;
    }

    const Image& PlayerComposer::Compose(const Image& video, double position, const std::wstring& time, double t, int seed, bool animate)
    {
        frame_.playerPosition = position;
        vars_.time = time;
        Rect client;
        const Image chrome = RenderFrame(frame_, plan_.clientW, plan_.clientH, vars_, &client);
        const int k = plan_.scale;
        Image window = ScaleNearest(chrome, k);
        const int vx = client.x * k + (plan_.clientW * k - video.width) / 2;
        const int vy = client.y * k + (plan_.clientH * k - video.height) / 2;
        BlitVideo(window, video, vx, vy);
        if (!plan_.square)
        {
            out_ = std::move(window);
            if (fxOn_)
            {
                fx_.Apply(out_, t, seed, animate);
            }
            return out_;
        }
        if (background_.width == plan_.outW && background_.height == plan_.outH)
        {
            out_ = background_;
        }
        else
        {
            out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        }
        Blit(out_, window, plan_.windowX, plan_.windowY);
        if (fxOn_)
        {
            fx_.Apply(out_, t, seed, animate);
        }
        return out_;
    }

    ExportResult Encode(Host& host, const EncoderSpec& spec, int total, const FrameSource& frames, const ExportStep& step, std::wstring& error)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            error = FfmpegMissingText();
            return ExportResult::Failed;
        }
        const double fps = std::clamp(spec.fps, 1.0, 60.0);
        std::vector<std::wstring> args = {L"-v", L"error", L"-nostdin", L"-y", L"-f", L"rawvideo", L"-pix_fmt", L"bgra",
                                          L"-s", Int(spec.width) + L"x" + Int(spec.height), L"-framerate", Number(fps), L"-i", L"pipe:0"};
        if (spec.gif)
        {
            args.insert(args.end(), {L"-filter_complex", L"[0:v]split[a][b];[a]palettegen=stats_mode=full[p];[b][p]paletteuse", L"-loop", L"0", L"-f", L"gif", spec.output});
        }
        else
        {
            if (spec.audio)
            {
                if (spec.start > 0.0)
                {
                    args.insert(args.end(), {L"-ss", Number(spec.start)});
                }
                args.insert(args.end(), {L"-t", Number(spec.duration), L"-i", spec.source, L"-map", L"0:v:0", L"-map", L"1:a:0?"});
                std::wstring filters;
                if (spec.fade > 0.0 && spec.duration > 0.2)
                {
                    const double f = std::min(spec.fade, spec.duration / 3.0);
                    filters = L"afade=t=in:st=0:d=" + Number(f) + L",afade=t=out:st=" + Number(spec.duration - f) + L":d=" + Number(f);
                }
                if (spec.delay > 0.0)
                {
                    filters += (filters.empty() ? L"" : L",") + std::wstring(L"adelay=") + std::to_wstring(int(std::lround(spec.delay * 1000.0))) + L":all=1";
                }
                if (!filters.empty())
                {
                    args.insert(args.end(), {L"-af", filters});
                }
            }
            const wchar_t* crf[] = {L"18", L"23", L"28"};
            args.insert(args.end(), {L"-c:v", L"libx264", L"-preset", L"medium", L"-crf", crf[std::clamp(spec.quality, 0, 2)], L"-pix_fmt", L"yuv420p"});
            if (spec.audio)
            {
                args.insert(args.end(), {L"-c:a", L"aac", L"-b:a", L"192k", L"-shortest"});
            }
            args.insert(args.end(), {L"-movflags", L"+faststart", L"-f", L"mp4", spec.output});
        }
        auto writer = host.StartProcess(ffmpeg, args, true, false);
        if (!writer)
        {
            error = L"не удалось запустить ffmpeg";
            return ExportResult::Failed;
        }
        const auto abort = [&](ExportResult result) {
            writer->Kill();
            int code = 0;
            writer->Finish(code, []() { return true; });
            host.RemoveFile(spec.output);
            return result;
        };
        const std::size_t bytes = std::size_t(spec.width) * std::size_t(spec.height) * sizeof(Color);
        int written = 0;
        for (int i = 0; i < total; ++i)
        {
            if (!step(i, total))
            {
                return abort(ExportResult::Cancelled);
            }
            const Image* frame = frames(i);
            if (frame == nullptr)
            {
                break;
            }
            if (frame->pixels.size() * sizeof(Color) != bytes || !writer->Write(frame->pixels.data(), bytes))
            {
                error = FromUtf8(LastLines(writer->Log(), 3));
                if (error.empty())
                {
                    error = L"ffmpeg прервал запись";
                }
                return abort(ExportResult::Failed);
            }
            ++written;
        }
        if (written == 0)
        {
            if (error.empty())
            {
                error = L"не удалось подготовить ни одного кадра";
            }
            return abort(ExportResult::Failed);
        }
        writer->CloseInput();
        int code = 0;
        if (!writer->Finish(code, [&]() { return step(total, total); }))
        {
            return abort(ExportResult::Cancelled);
        }
        if (code != 0)
        {
            error = FromUtf8(LastLines(writer->Log(), 3));
            if (error.empty())
            {
                error = L"ffmpeg завершился с ошибкой " + std::to_wstring(code);
            }
            host.RemoveFile(spec.output);
            return ExportResult::Failed;
        }
        return ExportResult::Done;
    }

    ExportResult ExportSound(Host& host, const SoundJob& job, const ExportStep& step, std::wstring& error)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            error = FfmpegMissingText();
            return ExportResult::Failed;
        }
        std::vector<std::wstring> args = {L"-v", L"error", L"-nostdin", L"-y"};
        if (job.start > 0.0)
        {
            args.insert(args.end(), {L"-ss", Number(job.start)});
        }
        args.insert(args.end(), {L"-t", Number(job.duration), L"-i", job.source, L"-vn", L"-sn"});
        if (job.fade > 0.0 && job.duration > 0.2)
        {
            const double f = std::min(job.fade, job.duration / 3.0);
            args.insert(args.end(), {L"-af", L"afade=t=in:st=0:d=" + Number(f) + L",afade=t=out:st=" + Number(job.duration - f) + L":d=" + Number(f)});
        }
        if (job.mp3)
        {
            args.insert(args.end(), {L"-c:a", L"libmp3lame", L"-q:a", L"2", L"-f", L"mp3"});
        }
        else if (job.retro)
        {
            args.insert(args.end(), {L"-ac", L"1", L"-ar", L"22050", L"-c:a", L"pcm_u8", L"-f", L"wav"});
        }
        else
        {
            args.insert(args.end(), {L"-c:a", L"pcm_s16le", L"-f", L"wav"});
        }
        args.push_back(job.output);
        auto proc = host.StartProcess(ffmpeg, args, false, false);
        if (!proc)
        {
            error = L"не удалось запустить ffmpeg";
            return ExportResult::Failed;
        }
        int code = 0;
        if (!proc->Finish(code, [&]() { return step(1, 1); }))
        {
            proc->Kill();
            proc->Finish(code, []() { return true; });
            host.RemoveFile(job.output);
            return ExportResult::Cancelled;
        }
        if (code != 0)
        {
            error = FromUtf8(LastLines(proc->Log(), 3));
            if (error.empty())
            {
                error = L"ffmpeg завершился с ошибкой " + std::to_wstring(code);
            }
            host.RemoveFile(job.output);
            return ExportResult::Failed;
        }
        return ExportResult::Done;
    }

    ExportResult ExportVideo(Host& host, const ExportJob& job, const ExportStep& step, std::wstring& error)
    {
        if (host.FfmpegPath().empty())
        {
            error = FfmpegMissingText();
            return ExportResult::Failed;
        }
        const double fps = std::clamp(job.fps, 1.0, 60.0);
        const int intro = job.effects.intro ? int(std::ceil(IntroSeconds * fps - 1e-6)) : 0;
        const int total = intro + std::max(1, int(std::ceil(job.duration * fps - 1e-6)));
        FrameStream reader(host, job.source, job.start, job.duration, fps, job.plan.videoW, job.plan.videoH);
        if (!reader.Started())
        {
            error = L"не удалось запустить ffmpeg";
            return ExportResult::Failed;
        }
        EncoderSpec spec;
        spec.output = job.output;
        spec.gif = job.gif;
        spec.quality = job.quality;
        spec.width = job.plan.outW;
        spec.height = job.plan.outH;
        spec.fps = fps;
        spec.audio = job.audio && !job.gif;
        spec.source = job.source;
        spec.start = job.start;
        spec.duration = job.duration;
        spec.delay = intro / fps;
        PlayerComposer composer(job.frame, job.vars, job.plan, job.background);
        composer.SetEffects(job.effects);
        const std::wstring length = FormatClock(job.duration);
        Image frame;
        const ExportResult result = Encode(host, spec, total, [&](int i) -> const Image* {
            if (i < intro)
            {
                return &composer.Intro(i / fps, i);
            }
            if (!reader.Next(frame))
            {
                if (i == intro)
                {
                    error = FromUtf8(LastLines(reader.Log(), 3));
                    if (error.empty())
                    {
                        error = L"не удалось прочитать кадры видео";
                    }
                }
                return nullptr;
            }
            const double t = (i - intro) / fps;
            return &composer.Compose(frame, job.duration > 0.0 ? t / job.duration : 0.0, FormatClock(t) + L" / " + length, t, i, true);
        }, step, error);
        reader.Stop();
        return result;
    }
}
