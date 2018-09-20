cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(lisp)

add_library(lisp
  lisp.cpp
  memory.cpp
  types.cpp
  lexer.cpp)

target_compile_options(lisp
  PRIVATE "-O2")

add_executable(repl
  test.cpp)

target_link_libraries(repl lisp)
