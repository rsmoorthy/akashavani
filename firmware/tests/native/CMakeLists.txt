cmake_minimum_required(VERSION 3.20)

project(akashavani_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AKASHAVANI_WERROR "Treat compiler warnings as errors" ON)

include(CTest)

set(AKASHAVANI_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../../libraries")

add_library(akashavani_logic STATIC
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src/Status.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src/Crc.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src/Hex.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src/TimeUtils.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src/DeviceId.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniDavis/src/DavisDecoder.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniDavis/src/DavisEncoder.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniDavis/src/DavisSimulator.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniArchive/src/ArchiveCollector.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniProtocol/src/MessageType.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniProtocol/src/CommandType.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniProtocol/src/Envelope.cpp"
  "${AKASHAVANI_LIB_DIR}/AkashavaniStorage/src/ArchivePath.cpp"
)

target_include_directories(akashavani_logic PUBLIC
  "${AKASHAVANI_LIB_DIR}/AkashavaniCore/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniDavis/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniArchive/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniProtocol/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniRadio/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniRtc/src"
  "${AKASHAVANI_LIB_DIR}/AkashavaniStorage/src"
)

target_compile_options(akashavani_logic PRIVATE -Wall -Wextra -Wpedantic)
if(AKASHAVANI_WERROR)
  target_compile_options(akashavani_logic PRIVATE -Werror)
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

add_executable(akashavani_tests
  test_core.cpp
  test_archive.cpp
  test_davis.cpp
  test_protocol.cpp
  test_radio.cpp
  test_storage.cpp
)

target_link_libraries(akashavani_tests PRIVATE akashavani_logic GTest::gtest_main)
target_compile_options(akashavani_tests PRIVATE -Wall -Wextra -Wpedantic)
if(AKASHAVANI_WERROR)
  target_compile_options(akashavani_tests PRIVATE -Werror)
endif()

include(GoogleTest)
gtest_discover_tests(akashavani_tests)
