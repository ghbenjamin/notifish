cmake_minimum_required(VERSION 3.16)
project(notifish)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB_RECURSE CPP_SOURCE_LIST "src/*.cpp")
include_directories(include)

add_executable(notifish ${CPP_SOURCE_LIST})

find_package(ZeroMQ CONFIG REQUIRED)
target_link_libraries(notifish PRIVATE libzmq libzmq-static)

find_package(RapidJSON CONFIG REQUIRED)
target_include_directories(notifish PRIVATE ${RAPIDJSON_INCLUDE_DIRS})

find_package(fmt CONFIG REQUIRED)
target_link_libraries(notifish PRIVATE fmt::fmt fmt::fmt-header-only)