# 示例工程：BSP 自检（bsp_smoke）
#
# 复用 bsp/ 组件（通过 EXTRA_COMPONENT_DIRS 引入，不复制任何 demo 代码）
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../bsp")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bsp_smoke)
