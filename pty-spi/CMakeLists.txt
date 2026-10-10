cmake_minimum_required(VERSION 3.30)

project(pty-spi)

add_executable(${CMAKE_PROJECT_NAME}
    "main.c"
    "queue.c"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "."
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    _GNU_SOURCE
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -O2 -g
    -std=c23
    -Wall
    -Wextra
    -Wpedantic
)
