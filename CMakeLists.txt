cmake_minimum_required(VERSION 3.0.0)
project(WebServer VERSION 0.3.3)

set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -std=c++14 -O3 -pthread -Wall")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_BUILD_TYPE "Release")
link_libraries(pthread)

include_directories(${PROJECT_SOURCE_DIR}/include/)

set(source_files
    src/log.cpp
    src/http_conn.cpp
    src/main.cpp
)

add_executable(WebServer ${source_files})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
