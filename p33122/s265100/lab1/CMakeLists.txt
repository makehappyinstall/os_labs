cmake_minimum_required(VERSION 3.16)
project(os_lab C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS_INIT "-Werror -Wall -Wpedantic")

add_executable(os_lab main.c CpuTime.c)
target_link_libraries(os_lab pthread)