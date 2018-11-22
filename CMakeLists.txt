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

add_library(lisp SHARED
  lib/environment.cpp
  lib/listBuilder.cpp
  lib/memory.cpp
  lib/parser.cpp
  lib/types.cpp
  lib/lexer.cpp
  lib/lisp.cpp
  lib/ast.cpp
  lib/dll.cpp
  lib/gc.cpp)

target_link_libraries(lisp
  dl)


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

# Run the unit tests
add_custom_command(TARGET dofile POST_BUILD
  COMMAND ./dofile lisp/unit-test.lisp)


# Build extensions
add_library(test SHARED dll/test.cpp)
target_link_libraries(test lisp)
