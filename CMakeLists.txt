cmake_minimum_required(VERSION 3.10)

project(easy_tcp)

include_directories(include)

set(SERVER_SRC src/server.cpp)
set(CLIENT_SRC src/client.cpp)

add_executable(server ${SERVER_SRC})
add_executable(client ${CLIENT_SRC})

target_link_libraries(server glog pthread)
target_link_libraries(client glog pthread)