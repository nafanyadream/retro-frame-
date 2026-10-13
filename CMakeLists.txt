cmake_minimum_required(VERSION 3.20)

project(RetroFrame VERSION 0.6.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)

add_executable(RetroFrame WIN32 ${CORE_SOURCES} ${WIN_SOURCES} res/app.rc)

target_compile_definitions(RetroFrame PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(RetroFrame PRIVATE /utf-8 /W4 /permissive- /wd4100)
target_link_libraries(RetroFrame PRIVATE user32 gdi32 ole32 oleaut32 windowscodecs shell32 comdlg32 advapi32 uuid winmm)
