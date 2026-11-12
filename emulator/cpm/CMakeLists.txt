cmake_minimum_required(VERSION 3.16)
project(cpm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Debug so GDB breakpoints work out of the box
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_library(cpm_core STATIC
    CPMParser.cpp
    CPMCompiler.cpp
    CPMSemantic.cpp
    CPMIR.cpp
    CPMAssembly.cpp
    CPMIntermediate.cpp
    Logger.cpp
)
target_include_directories(cpm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

if(NOT TARGET falc_core)
    add_subdirectory(../../falc falc EXCLUDE_FROM_ALL)
endif()

add_executable(cpm cpm.cpp)
target_link_libraries(cpm PRIVATE cpm_core falc_core)

# Not a Windows build — don't pull in tchar.h / SDKDDKVer.h
target_compile_definitions(cpm PRIVATE
    $<$<NOT:$<PLATFORM_ID:Windows>>:_TCHAR=char>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_tmain=main>
)

include(CTest)
if(BUILD_TESTING)
    add_executable(cpm_separator_tests tests/separators.cpp)
    target_link_libraries(cpm_separator_tests PRIVATE cpm_core)
    add_test(NAME cpm_separators COMMAND cpm_separator_tests)

    add_executable(cpm_type_tests tests/type_expressions.cpp)
    target_link_libraries(cpm_type_tests PRIVATE cpm_core)
    target_compile_definitions(cpm_type_tests PRIVATE
        CPM_EXAMPLE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/test.cpm"
    )
    add_test(NAME cpm_types COMMAND cpm_type_tests)

    add_executable(cpm_semantic_tests tests/semantics.cpp)
    target_link_libraries(cpm_semantic_tests PRIVATE cpm_core)
    add_test(NAME cpm_semantics COMMAND cpm_semantic_tests)

    enable_language(C)
    add_executable(cpm_ir_tests
        tests/ir.cpp
        ../../fridgemulib/fridgemulib.c
        ../../fridgemulib/posit.c
    )
    target_include_directories(cpm_ir_tests PRIVATE ../../fridgemulib)
    target_compile_definitions(cpm_ir_tests PRIVATE
        CPM_HELLO_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.cpm"
        CPM_VTEXT_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/x2al_std/vtext.falc"
    )
    target_link_libraries(cpm_ir_tests PRIVATE cpm_core falc_core)
    add_test(NAME cpm_ir COMMAND cpm_ir_tests)
endif()
