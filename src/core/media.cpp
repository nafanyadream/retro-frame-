#include "media.h"

#include "utf.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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
        return L"Для видео нужна программа ffmpeg.\n\n"
               L"Скачайте её для Windows по ссылкам на сайте ffmpeg.org\n"
               L"и положите файл ffmpeg.exe рядом с RetroFrame.exe.\n\n"
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
            const std::string tail = LastLines(log, 1);
            error = tail.find("No such file") != std::string::npos ? L"файл не найден" : L"ffmpeg не смог прочитать файл";
            return false;
        }
        if (!info.hasVideo)
        {
            error = L"в файле нет видео";
            return false;
        }
        return true;
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
        if (target.square)
        {
            int side = target.width > 0 ? std::min(target.width, target.squareSize) : target.squareSize;
            side = std::max(120, side & ~1);
            p.square = true;
            p.outW = side;
            p.outH = side;
            const int room = side * 92 / 100;
            int k = fixed;
            if (k == 0)
            {
                k = 1;
                for (int cand = 4; cand >= 1; --cand)
                {
                    if (room / cand - chromeW >= PlayerMinWidth && room / cand - chromeH >= 90)
                    {
                        k = cand;
                        break;
                    }
                }
            }
            p.scale = k;
            const int maxW = std::max(PlayerMinWidth, room / k - chromeW);
            const int maxH = std::max(40, room / k - chromeH);
            int cw = maxW;
            int ch = int(std::lround(cw / aspect));
            if (ch > maxH)
            {
                ch = maxH;
                cw = std::max(2, int(std::lround(ch * aspect)));
            }
            p.clientW = std::max(cw, std::min(maxW, PlayerMinWidth));
            p.clientH = ch;
            int vw = p.clientW * k;
            int vh = int(std::lround(vw / aspect));
            if (vh > p.clientH * k)
            {
                vh = p.clientH * k;
                vw = int(std::lround(vh * aspect));
            }
            p.videoW = std::max(2, vw);
            p.videoH = std::max(2, vh);
            p.windowW = (p.clientW + chromeW) * k;
            p.windowH = (p.clientH + chromeH) * k;
            p.windowX = std::max(0, (side - p.windowW) / 2);
            p.windowY = std::max(0, (side - p.windowH) / 2);
            return p;
        }
        int vw = sw;
        int vh = sh;
        int k = fixed;
        if (target.width > 0)
        {
            if (k == 0)
            {
                k = AutoScale(std::min(sw, target.width), std::min(sh, target.width));
            }
            const int limit = target.width / k - chromeW;
            if (limit * k < vw)
            {
                vw = std::max(PlayerMinWidth, limit) * k;
                vh = int(std::lround(vw / aspect));
                if (vw > limit * k)
                {
                    vw = limit * k;
                    vh = int(std::lround(vw / aspect));
                }
            }
        }
        else if (k == 0)
        {
            k = AutoScale(sw, sh);
        }
        p.scale = k;
        p.videoW = std::max(2, vw);
        p.videoH = std::max(2, vh);
        p.clientW = std::max((p.videoW + k - 1) / k, PlayerMinWidth);
        p.clientH = (p.videoH + k - 1) / k;
        if (((p.clientW + chromeW) * k) % 2 != 0)
        {
            ++p.clientW;
        }
        if (((p.clientH + chromeH) * k) % 2 != 0)
        {
            ++p.clientH;
        }
        p.windowW = (p.clientW + chromeW) * k;
        p.windowH = (p.clientH + chromeH) * k;
        p.outW = p.windowW;
        p.outH = p.windowH;
        return p;
    }

    Image SquareBackground(const Image& picture, Color color, int size)
    {
        if (picture.Empty())
        {
            return Image(size, size, Opaque(color));
        }
        const int side = std::min(picture.width, picture.height);
        const Image square = Crop(picture, Rect{(picture.width - side) / 2, (picture.height - side) / 2, side, side});
        Image out = ResizeSmooth(square, size, size);
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

    const Image& PlayerComposer::Compose(const Image& video, double position, const std::wstring& time)
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
        return out_;
    }

    ExportResult ExportVideo(Host& host, const ExportJob& job, const ExportStep& step, std::wstring& error)
    {
        const std::wstring ffmpeg = host.FfmpegPath();
        if (ffmpeg.empty())
        {
            error = FfmpegMissingText();
            return ExportResult::Failed;
        }
        const double fps = std::clamp(job.fps, 1.0, 60.0);
        const int total = std::max(1, int(std::ceil(job.duration * fps - 1e-6)));
        FrameStream reader(host, job.source, job.start, job.duration, fps, job.plan.videoW, job.plan.videoH);
        if (!reader.Started())
        {
            error = L"не удалось запустить ffmpeg";
            return ExportResult::Failed;
        }
        std::vector<std::wstring> args = {L"-v", L"error", L"-nostdin", L"-y", L"-f", L"rawvideo", L"-pix_fmt", L"bgra",
                                          L"-s", Int(job.plan.outW) + L"x" + Int(job.plan.outH), L"-framerate", Number(fps), L"-i", L"pipe:0"};
        if (job.gif)
        {
            args.insert(args.end(), {L"-filter_complex", L"[0:v]split[a][b];[a]palettegen=stats_mode=full[p];[b][p]paletteuse", L"-loop", L"0", L"-f", L"gif", job.output});
        }
        else
        {
            if (job.audio)
            {
                if (job.start > 0.0)
                {
                    args.insert(args.end(), {L"-ss", Number(job.start)});
                }
                args.insert(args.end(), {L"-t", Number(job.duration), L"-i", job.source, L"-map", L"0:v:0", L"-map", L"1:a:0?"});
            }
            const wchar_t* crf[] = {L"18", L"23", L"28"};
            args.insert(args.end(), {L"-c:v", L"libx264", L"-preset", L"medium", L"-crf", crf[std::clamp(job.quality, 0, 2)], L"-pix_fmt", L"yuv420p"});
            if (job.audio)
            {
                args.insert(args.end(), {L"-c:a", L"aac", L"-b:a", L"192k", L"-shortest"});
            }
            args.insert(args.end(), {L"-movflags", L"+faststart", L"-f", L"mp4", job.output});
        }
        auto writer = host.StartProcess(ffmpeg, args, true, false);
        if (!writer)
        {
            error = L"не удалось запустить ffmpeg";
            return ExportResult::Failed;
        }
        const auto abort = [&](ExportResult result) {
            reader.Stop();
            writer->Kill();
            int code = 0;
            writer->Finish(code, []() { return true; });
            host.RemoveFile(job.output);
            return result;
        };
        PlayerComposer composer(job.frame, job.vars, job.plan, job.background);
        const std::wstring length = FormatClock(job.duration);
        Image frame;
        int written = 0;
        for (int i = 0; i < total; ++i)
        {
            if (!step(i, total))
            {
                return abort(ExportResult::Cancelled);
            }
            if (!reader.Next(frame))
            {
                break;
            }
            const double t = i / fps;
            const Image& out = composer.Compose(frame, job.duration > 0.0 ? t / job.duration : 0.0, FormatClock(t) + L" / " + length);
            if (!writer->Write(out.pixels.data(), out.pixels.size() * sizeof(Color)))
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
            error = FromUtf8(LastLines(reader.Log(), 3));
            if (error.empty())
            {
                error = L"не удалось прочитать кадры видео";
            }
            return abort(ExportResult::Failed);
        }
        reader.Stop();
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
            host.RemoveFile(job.output);
            return ExportResult::Failed;
        }
        return ExportResult::Done;
    }
}
