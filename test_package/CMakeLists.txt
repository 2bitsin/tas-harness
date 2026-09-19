cmake_minimum_required(VERSION 3.25)
project(package_smoke CXX)

find_package(tash REQUIRED CONFIG)

add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE tash::linked)
