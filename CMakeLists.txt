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
project(ebl-lisp)

add_library(ebl-lisp SHARED
  lib/environment.cpp
  lib/listBuilder.cpp
  lib/persistent.cpp
  lib/bytecode.cpp
  lib/memory.cpp
  lib/parser.cpp
  lib/types.cpp
  lib/lexer.cpp
  lib/lisp.cpp
  lib/ast.cpp
  lib/dll.cpp
  lib/gc.cpp
  lib/vm.cpp)

target_link_libraries(ebl-lisp
  dl)


# A repl.
add_executable(ebl-repl
  tools/repl.cpp)

target_link_libraries(ebl-repl
  ebl-lisp)


# Execute a script file.
add_executable(ebl-dofile
  tools/dofile.cpp)

target_link_libraries(ebl-dofile
  ebl-lisp)

# Run the unit tests
# add_custom_command(TARGET ebl-dofile POST_BUILD
#   COMMAND ./ebl-dofile lisp/unit-test.lisp)


# Build extensions
add_library(fs SHARED dll/fs.cpp)
target_link_libraries(fs ebl-lisp)
set_target_properties(fs PROPERTIES SUFFIX "")

add_library(sys SHARED dll/sys.cpp)
target_link_libraries(sys ebl-lisp)
set_target_properties(sys PROPERTIES SUFFIX "")
