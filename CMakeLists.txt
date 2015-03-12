cmake_minimum_required(VERSION 2.8)
project(socket5_v4)

add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-Wformat=2)
add_definitions(-Wstrict-aliasing=2)
add_definitions(-Wcast-align)
add_definitions(-Wwrite-strings)
add_definitions(-Wformat-nonliteral)
add_definitions(-Wconversion)
add_definitions(-Wfloat-equal)
add_definitions(-Wpointer-arith)
add_definitions(-Wswitch-enum)

set(SRCLIST
src/rb_tree.c
src/rw_buffer.c
src/epoll_loop.c
src/pipe_tools.c
src/s5_local.c
src/s5_remote.c
src/dns_pool.c
)

add_executable(socket5_v4_remote ${SRCLIST} src/remote_test.c)
add_executable(socket5_v4_local ${SRCLIST} src/local_test.c)

