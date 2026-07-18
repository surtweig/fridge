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
    ${IMGUI_SOURCES}
)

target_include_directories(fridge_emulator PRIVATE
    src
    ../include
    ../fridgemulib
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