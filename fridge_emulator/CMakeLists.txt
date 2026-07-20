cmake_minimum_required(VERSION 3.16)
project(fridge_emulator CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

set(IMGUI_SOURCES
    deps/imgui/imgui.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/backends/imgui_impl_sdl2.cpp
    deps/imgui/backends/imgui_impl_vulkan.cpp
)

add_executable(fridge_emulator
    src/main.cpp
    src/app.cpp
    src/fridge_ctx.cpp
    src/rom_loader.cpp
    src/emu_worker.cpp
    src/keymap.cpp
    src/vk_texture.cpp
    src/file_dialog.cpp
    src/falcfrontend.cpp
    src/ui_source.cpp
    src/ui_symbols.cpp
    ${IMGUI_SOURCES}

    # fridgemulib as sources (matches fridge_opengl_emulator.pro pattern).
    # posit.c intentionally excluded — PAM16 disabled.
    ../fridgemulib/fridgemulib.c

    # falc compiler as source (no main.cpp — only the compiler class).
    ../falc/FridgeAssemblyLanguageCompiler.cpp
)

# fridgemulib.c is plain C89 but its header uses no extern "C" guards, so compile
# it as C++ to share mangled symbols with the rest of the executable. This matches
# the existing Qt project (fridge_opengl_emulator.pro) which builds the same file
# as C++ via the .pro qmake pipeline. C permits implicit int/enum conversions
# that C++ rejects; -fpermissive downgrades them to warnings.
set_source_files_properties(../fridgemulib/fridgemulib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(../fridgemulib/fridgemulib.c APPEND
    PROPERTIES COMPILE_OPTIONS "-fpermissive")

target_include_directories(fridge_emulator PRIVATE
    src
    ../include
    ../fridgemulib
    ../falc
    deps/imgui
    deps/imgui/backends
    deps/vma/include
)

target_compile_definitions(fridge_emulator PRIVATE
    $<$<CONFIG:Debug>:APP_USE_VULKAN_DEBUG_REPORT>
)

if(TARGET SDL2::SDL2)
    target_link_libraries(fridge_emulator PRIVATE SDL2::SDL2)
else()
    target_link_libraries(fridge_emulator PRIVATE SDL2)
endif()

target_link_libraries(fridge_emulator PRIVATE
    Vulkan::Vulkan
    Threads::Threads
)