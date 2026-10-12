#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
out="${1:-build/tests}"
mkdir -p "$out"

flags=(-std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter)
if [[ "${SANITIZE:-1}" == "1" ]]; then
    flags+=(-fsanitize=address,undefined)
fi

g++ "${flags[@]}" -o "$out/zip_test" src/core/zip.cpp src/core/utf.cpp tests/zip_test.cpp
g++ "${flags[@]}" -o "$out/library_test" src/core/*.cpp tests/library_test.cpp
g++ "${flags[@]}" -o "$out/ui_test" src/core/*.cpp tests/ui_test.cpp
g++ "${flags[@]}" -o "$out/frames_test" src/core/*.cpp tests/frames_test.cpp
g++ "${flags[@]}" -o "$out/video_test" src/core/*.cpp tests/video_test.cpp
g++ "${flags[@]}" -o "$out/music_test" src/core/*.cpp tests/music_test.cpp

python3 tests/make_test_data.py "$out/data"
python3 tests/check_zip.py "$out/zip_test" "$out/zip"
"$out/library_test" "$out/data/icons" "$out/data/pack.zip" "$out"
"$out/ui_test" "$out/data/photo1.ppm" "$out/data/photo2.ppm" "$out"
"$out/frames_test" "$out/data/photo1.ppm" "$out"
if command -v ffmpeg > /dev/null; then
    "$out/video_test" "$out" "$(command -v ffmpeg)"
    "$out/music_test" "$out" "$(command -v ffmpeg)"
else
    echo "ffmpeg не найден: проверки видео и музыки пропущены"
fi
echo "Снимки окон и рамок: $out/*.ppm"
