cmake_minimum_required(VERSION 3.13)
project(try1 C)

set(CMAKE_C_STANDARD 99)

add_executable(try1 main.c code.c)

#потоки работают
find_package(Threads REQUIRED)
target_link_libraries(try1 Threads::Threads)
add_compile_options(-pthread)