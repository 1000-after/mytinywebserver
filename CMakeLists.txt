#最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

#项目名称和版本
project(TinyWebServer VERSION 1.0.0)

#设置c++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#打印欢迎信息
message("开始构建 TinyWebServer项目")
message("项目目录：${CMAKE_SOURCE_DIR}")

#添加可执行文件按
add_executable(tinywebserver
src/main.cpp
)

#设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)