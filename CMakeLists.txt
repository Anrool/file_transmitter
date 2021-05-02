cmake_minimum_required(VERSION 3.0)

project("file_transmitter" CXX)

find_package(Boost REQUIRED COMPONENTS
    system
    thread
    date_time
    regex
    serialization
    coroutine
)

add_library(common INTERFACE)
target_compile_options(common INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -O2
)
target_include_directories(common INTERFACE include)
target_link_libraries(common INTERFACE ${Boost_LIBRARIES})

add_executable(client src/client.cpp)
target_link_libraries(client PRIVATE common)

add_executable(server src/server.cpp)
target_link_libraries(server PRIVATE common)
