cmake_minimum_required(VERSION 3.10)
project(TinyWebServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 🟢 bug3 修复：使用 gnu++11（GNU 扩展的 C++11）而不是纯 c++11
#      纯 c++11 严格模式不支持 ##__VA_ARGS__ 零参数时去掉逗号的 GNU 扩展
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

# ==================== 服务器程序 ====================
# 6.0 版本：main + server + worker + threadpool
add_executable(TinyWebServer
    src/main.cpp
    src/server.cpp
    src/worker.cpp
    src/threadpool.cpp
    src/logger.cpp     # 🆕 日志系统
    src/timer_wheel.cpp  # 🆕 时间轮超时管理
)

# 给服务器程序加线程库（多线程版本需要）
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(TinyWebServer PRIVATE Threads::Threads)


# ==================== 头文件目录 ====================
include_directories(include)