cmake_minimum_required(VERSION 3.20)

project(akashvani_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AKASHVANI_WERROR "Treat compiler warnings as errors" ON)

include(CTest)

set(AKASHVANI_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../../libraries")

add_library(akashvani_logic STATIC
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src/Status.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src/Crc.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src/Hex.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src/TimeUtils.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src/DeviceId.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniDavis/src/DavisDecoder.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniDavis/src/DavisEncoder.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniDavis/src/DavisSimulator.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniProtocol/src/MessageType.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniProtocol/src/CommandType.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniProtocol/src/Envelope.cpp"
  "${AKASHVANI_LIB_DIR}/AkashvaniStorage/src/ArchivePath.cpp"
)

target_include_directories(akashvani_logic PUBLIC
  "${AKASHVANI_LIB_DIR}/AkashvaniCore/src"
  "${AKASHVANI_LIB_DIR}/AkashvaniDavis/src"
  "${AKASHVANI_LIB_DIR}/AkashvaniProtocol/src"
  "${AKASHVANI_LIB_DIR}/AkashvaniRadio/src"
  "${AKASHVANI_LIB_DIR}/AkashvaniRtc/src"
  "${AKASHVANI_LIB_DIR}/AkashvaniStorage/src"
)

target_compile_options(akashvani_logic PRIVATE -Wall -Wextra -Wpedantic)
if(AKASHVANI_WERROR)
  target_compile_options(akashvani_logic PRIVATE -Werror)
endif()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(akashvani_tests
  test_core.cpp
  test_davis.cpp
  test_protocol.cpp
  test_radio.cpp
  test_storage.cpp
)

target_link_libraries(akashvani_tests PRIVATE akashvani_logic GTest::gtest_main)
target_compile_options(akashvani_tests PRIVATE -Wall -Wextra -Wpedantic)
if(AKASHVANI_WERROR)
  target_compile_options(akashvani_tests PRIVATE -Werror)
endif()

include(GoogleTest)
gtest_discover_tests(akashvani_tests)
