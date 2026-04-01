cmake_minimum_required(VERSION 3.10)
project(TinyWebServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 服务器程序（只编译 main.cpp + server.cpp）
add_executable(TinyWebServer
    src/main.cpp
    src/server.cpp
)

# 压测客户端（只编译 tester.cpp）
add_executable(tester
    src/tester.cpp
)

# 给 tester 加线程库（修复 pthread 错误）
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(tester PRIVATE Threads::Threads)

# 头文件目录
include_directories(include)