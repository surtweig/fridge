cmake_minimum_required(VERSION 3.16)
project(fridge_sdk LANGUAGES CXX)

option(BUILD_TESTING "Build the compiler tests" OFF)

add_subdirectory(../falc falc)
add_subdirectory(../emulator/cpm cpm)
add_subdirectory(../fridge_emulator fridge_emulator)

install(TARGETS falc cpm fridge_emulator RUNTIME DESTINATION bin)
