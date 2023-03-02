cmake_minimum_required(VERSION 3.20)
project(Main)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra -pedantic-errors)

add_executable(Main main.cpp)
