cmake_minimum_required(VERSION 3.16)
project(os_lab2 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(lab2 main.c)
target_link_libraries(lab2 PRIVATE Threads::Threads)
target_compile_options(lab2 PRIVATE -Wall -Wextra -Wpedantic)
