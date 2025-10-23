cmake_minimum_required(VERSION 3.16)
project(golf_launch_monitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(src)

set(SOURCES
        src/main.cpp
)

add_executable(launch_monitor ${SOURCES})
target_link_libraries(launch_monitor ${OpenCV_LIBS})
target_include_directories(launch_monitor PRIVATE ${OpenCV_INCLUDE_DIRS})

