cmake_minimum_required(VERSION 3.17)
project(tools C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-static")
add_executable(tools main.c)