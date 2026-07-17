# The following lines of Boilerplate have to be in your project's CMakeLists.txt
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "1.3.0")
project(smart_gateway)
