cmake_minimum_required(VERSION 3.13)
project(ccont)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
link_libraries(m)

add_executable(ccont source/main.c source/jail.c source/shortid.c source/ccontutils.c)