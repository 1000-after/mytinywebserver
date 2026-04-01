# 最低CMake版本要求（3.10兼容绝大多数Linux环境）
cmake_minimum_required(VERSION 3.10)
# 项目名称（自定义，和你的项目名一致）
project(TinyWebServer VERSION 1.0 LANGUAGES CXX)

# 设置C++标准（C++11，WebServer开发通用标准，可按需升级到14/17）
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制要求该标准，不允许降级
set(CMAKE_CXX_EXTENSIONS OFF)        # 禁用编译器特定扩展，保证跨平台



# 【关键】指定头文件搜索路径：把include目录加入包含路径
# 这样代码里写 #include "server.h" 就能直接找到，不用写相对路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 【关键】收集src目录下所有源文件（自动遍历，后续加新文件不用改这里）
file(GLOB_RECURSE SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)


# 生成可执行文件：第一个参数是生成的程序名，第二个是源文件列表
add_executable(${PROJECT_NAME} ${SRC_FILES})