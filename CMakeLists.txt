cmake_minimum_required(VERSION 3.11)

project(factory_task CXX)
set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(factory_task main.cpp)
target_link_libraries(factory_task PRIVATE Threads::Threads)
