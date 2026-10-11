cmake_minimum_required(VERSION 3.20)

project(RetroFrame VERSION 0.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RetroFrame WIN32 src/main.cpp)

target_compile_definitions(RetroFrame PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(RetroFrame PRIVATE /utf-8 /W4 /permissive-)
target_link_libraries(RetroFrame PRIVATE user32 gdi32)
