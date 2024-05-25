
cmake_minimum_required(VERSION 3.3 FATAL_ERROR)

project(convertTS)

# include all components
add_executable(writeTS MACOSX_BUNDLE writeTS.cpp)
target_link_libraries(writeTS PRIVATE umesh)
