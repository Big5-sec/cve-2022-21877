cmake_minimum_required(VERSION 3.0)
project(BUGSPACEPORT)

set(MYTARGET "leakspaceport")

set(root_dir "${CMAKE_SOURCE_DIR}")
set(src_dir "${CMAKE_SOURCE_DIR}/src")


set(CMAKE_VERBOSE_MAKEFILE TRUE)

# Redefine output directory

set(
  CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE
  ${CMAKE_SOURCE_DIR}/bin/
)
set(
  CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE
  ${CMAKE_SOURCE_DIR}/bin/
)

#########################
# build the producer
#########################

add_executable(
  ${MYTARGET}
  "${src_dir}/Source.cpp"
  "${src_dir}/spaceport.cpp"
  "${src_dir}/spaceport.h"
  "${src_dir}/hexdump.hpp"

)
