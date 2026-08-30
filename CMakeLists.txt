cmake_minimum_required(VERSION 3.16)
project(video_pipeline_vulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(vpv src/main.cpp)
target_link_libraries(vpv PRIVATE Vulkan::Vulkan)