cmake_minimum_required(VERSION 3.10)
project(SiriusOS_NDE LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick)
find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)



add_subdirectory(src)