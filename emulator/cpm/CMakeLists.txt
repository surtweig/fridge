cmake_minimum_required(VERSION 3.16)
project(cpm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CPM source files (Frion compiler)
set(CPM_SOURCES
    cpm.cpp
    CPMParser.cpp
    CPMCompiler.cpp
    CPMIntermediate.cpp
    Logger.cpp
)

add_executable(cpm ${CPM_SOURCES})

# Include paths: project root for fridge.h, local dir for peer headers
target_include_directories(cpm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Not a Windows build — don't pull in tchar.h / SDKDDKVer.h
target_compile_definitions(cpm PRIVATE
    $<$<NOT:$<PLATFORM_ID:Windows>>:_TCHAR=char>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_tmain=main>
)
