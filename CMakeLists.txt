cmake_minimum_required(VERSION 3.16)
project(AutoClikerSiris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AutoCliker-Siris WIN32
    src/main.cpp
    src/clicker.cpp
    src/settings.cpp
    src/ui.cpp
)

target_include_directories(AutoCliker-Siris PRIVATE src)
target_compile_definitions(AutoCliker-Siris PRIVATE NOMINMAX UNICODE _UNICODE)
target_link_libraries(AutoCliker-Siris PRIVATE gdiplus user32 gdi32 winmm)

if(MSVC)
    target_compile_options(AutoCliker-Siris PRIVATE /utf-8 /W4)
endif()