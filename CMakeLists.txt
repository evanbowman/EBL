cmake_minimum_required(VERSION 2.8)

macro(use_cplusplus11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cplusplus11)

use_cplusplus11()

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


# A repl.
add_executable(repl
  tools/repl.cpp)

target_link_libraries(repl
  lisp)


# Execute a script file.
add_executable(dofile
  tools/dofile.cpp)

target_link_libraries(dofile
  lisp)
