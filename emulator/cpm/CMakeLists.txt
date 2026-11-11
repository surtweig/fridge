cmake_minimum_required(VERSION 3.16)
project(cpm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Debug so GDB breakpoints work out of the box
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# CPM source files (Frion compiler)
set(CPM_SOURCES
    CPMParser.cpp
    CPMCompiler.cpp
    CPMSemantic.cpp
    CPMIntermediate.cpp
    Logger.cpp
)

add_executable(cpm cpm.cpp ${CPM_SOURCES})

# Include paths: repo root include/ for fridge.h, local dir for peer headers
target_include_directories(cpm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Not a Windows build — don't pull in tchar.h / SDKDDKVer.h
target_compile_definitions(cpm PRIVATE
    $<$<NOT:$<PLATFORM_ID:Windows>>:_TCHAR=char>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_tmain=main>
)

include(CTest)
if(BUILD_TESTING)
    add_executable(cpm_separator_tests
        tests/separators.cpp
        ${CPM_SOURCES}
    )
    target_include_directories(cpm_separator_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME cpm_separators COMMAND cpm_separator_tests)
    add_executable(cpm_type_tests
        tests/type_expressions.cpp
        ${CPM_SOURCES}
    )
    target_include_directories(cpm_type_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_compile_definitions(cpm_type_tests PRIVATE
        CPM_EXAMPLE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/test.cpm"
    )
    add_test(NAME cpm_types COMMAND cpm_type_tests)
    add_executable(cpm_semantic_tests tests/semantics.cpp ${CPM_SOURCES})
    target_include_directories(cpm_semantic_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME cpm_semantics COMMAND cpm_semantic_tests)
endif()
