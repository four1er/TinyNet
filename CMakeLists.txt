cmake_minimum_required(VERSION 3.15)
project(tinynet VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 src 子目录
add_subdirectory(src)

# 添加 examples 子目录
# add_subdirectory(examples)
