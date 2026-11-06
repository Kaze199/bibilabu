cmake_minimum_required(VERSION 3.15)

project(StudentDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Student STATIC
    tools/student.cpp
)

target_include_directories(Student
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

add_executable(student_demo
    src/main.cpp
)

target_link_libraries(student_demo
    PRIVATE
        Student
)
