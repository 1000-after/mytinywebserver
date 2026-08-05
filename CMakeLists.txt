cmake_minimum_required(VERSION 3.10)
project(TinyWebServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 服务器程序 ====================
# 6.0 版本：main + server + worker + threadpool
add_executable(TinyWebServer
    src/main.cpp
    src/server.cpp
    src/worker.cpp
    src/threadpool.cpp
)

# 给服务器程序加线程库（多线程版本需要）
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(TinyWebServer PRIVATE Threads::Threads)

# ==================== 压测客户端 ====================
add_executable(tester
    src/tester.cpp
)

# 给 tester 加线程库
target_link_libraries(tester PRIVATE Threads::Threads)

# ==================== 头文件目录 ====================
include_directories(include)