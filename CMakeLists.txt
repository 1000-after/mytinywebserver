#最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

#项目名称和版本
project(TinyWebServer VERSION 1.0.0)

#设置c++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

#源文件列表
set(
    SOURCES
    src/main.cpp
    src/simple_logger.cpp
)
#打印欢迎信息
message("开始构建 TinyWebServer项目")
message("项目目录：${CMAKE_SOURCE_DIR}")

#添加可执行文件按
add_executable(tinywebserver
${SOURCES}
)

#编译选项
target_compile_options(tinywebserver PRIVATE
    -Wall
    -Wextra
    -O2
)

#链接库
target_link_libraries(tinywebserver pthread)

#设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)