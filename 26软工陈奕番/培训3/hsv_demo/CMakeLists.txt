cmake_minimum_required(VERSION 3.11)

project(hsv_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(hsv_demo main.cpp)

target_include_directories(hsv_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(hsv_demo PRIVATE ${OpenCV_LIBS})