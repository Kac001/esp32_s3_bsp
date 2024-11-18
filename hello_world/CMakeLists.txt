# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16) # 指定 CMake 最小版本

include($ENV{IDF_PATH}/tools/cmake/project.cmake) #包含 IDF 的路径  指定cmake路径
project(hello_world) #工程名称
