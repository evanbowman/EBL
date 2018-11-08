cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -O2")
include_directories(./)


# The lisp runtime library.
project(lisp)

add_library(lisp
  lib/lisp.cpp
  lib/memory.cpp
  lib/subr.cpp
  lib/lexer.cpp
  lib/parser.cpp
  lib/ast.cpp)


# The interpreter. Should be replaced with a bytecode compiler in the future.
add_library(lisp-evaluator
  interp/evaluator.cpp)

target_link_libraries(lisp-evaluator
  lisp)


# A repl.
add_executable(repl
  tools/repl.cpp)

target_link_libraries(repl
  lisp-evaluator)


# Execute a script file.
add_executable(dofile
  tools/dofile.cpp)

target_link_libraries(dofile
  lisp)
