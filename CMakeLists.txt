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
project(ebl-runtime)

add_library(ebl-runtime SHARED
  runtime/environment.cpp
  runtime/listBuilder.cpp
  runtime/persistent.cpp
  runtime/builtins.cpp
  runtime/bytecode.cpp
  runtime/memory.cpp
  runtime/parser.cpp
  runtime/types.cpp
  runtime/lexer.cpp
  runtime/ast.cpp
  runtime/dll.cpp
  runtime/gc.cpp
  runtime/vm.cpp)

target_link_libraries(ebl-runtime
  dl)


# Execute a script file.
add_executable(ebl-dofile
  tools/dofile.cpp)

target_link_libraries(ebl-dofile
  ebl-runtime)

# Execute a script file.
add_executable(ebl-run-bytecode
  tools/runBytecode.cpp)

target_link_libraries(ebl-run-bytecode
  ebl-runtime)


# Build extensions
add_library(fs SHARED dll/fs.cpp)
target_link_libraries(fs ebl-runtime)
set_target_properties(fs PROPERTIES SUFFIX "")

add_library(sys SHARED dll/sys.cpp)
target_link_libraries(sys ebl-runtime)
set_target_properties(sys PROPERTIES SUFFIX "")

add_library(debug SHARED dll/debug.cpp)
target_link_libraries(debug ebl-runtime)
set_target_properties(debug PROPERTIES SUFFIX "")
