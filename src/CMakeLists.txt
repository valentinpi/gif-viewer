cmake_minimum_required(VERSION 3.16)

project(gif-viewer)

# I am using this guide: https://trenki2.github.io/blog/2017/06/02/using-sdl2-with-cmake/
find_package(SDL2 REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

# GCC flags
if(MINGW)
    set(CMAKE_C_FLAGS "-O2 -std=c11 -Wall -Wextra")
endif(MINGW)

set(SOURCE_FILES main.c)
add_executable(gif-viewer ${SOURCE_FILES})
#target_link_libraries(gif-viewer ${SDL_LIBRARIES})
target_link_libraries(gif-viewer "-lmingw32 -lSDL2main -lSDL2")
