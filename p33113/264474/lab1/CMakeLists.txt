cmake_minimum_required(VERSION 3.15)
project(lab1os C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS_INIT "-Werror -Wall -Wpedantic")
find_package(Threads REQUIRED)
add_executable(lab1os main.c MemoryFiller.c FileIO.c)
target_link_libraries(${CMAKE_PROJECT_NAME} Threads::Threads)
