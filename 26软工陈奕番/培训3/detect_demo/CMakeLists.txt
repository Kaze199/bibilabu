cmake_minimum_required(VERSION 3.11)

project(detect_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(detect_demo main.cpp)

target_include_directories(detect_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(detect_demo PRIVATE ${OpenCV_LIBS})