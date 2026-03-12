cmake_minimum_required(VERSION 3.20)

project(AsmLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(ASMLAB_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

find_package(OpenGL REQUIRED)
find_package(SDL2 QUIET CONFIG)

if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found locally, fetching via FetchContent")
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.11
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

set(ASMLAB_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/LevelLoader.cpp
    src/render/Renderer.cpp
    src/ui/TextBuffer.cpp
    src/vm/Assembler.cpp
    src/vm/VirtualMachine.cpp
)

add_executable(asmlab ${ASMLAB_SOURCES})

target_include_directories(asmlab PRIVATE src)
target_compile_definitions(asmlab PRIVATE ASMLAB_PROJECT_ROOT="${CMAKE_SOURCE_DIR}" SDL_MAIN_HANDLED)

if(MSVC)
    target_compile_definitions(asmlab PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(asmlab PRIVATE /W4 /permissive-)
    if(ASMLAB_WARNINGS_AS_ERRORS)
        target_compile_options(asmlab PRIVATE /WX)
    endif()
else()
    target_compile_options(asmlab PRIVATE -Wall -Wextra -Wpedantic)
    if(ASMLAB_WARNINGS_AS_ERRORS)
        target_compile_options(asmlab PRIVATE -Werror)
    endif()
endif()

set(ASMLAB_LIBS OpenGL::GL SDL2::SDL2)

target_link_libraries(asmlab PRIVATE ${ASMLAB_LIBS})

set_target_properties(asmlab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
