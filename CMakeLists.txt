cmake_minimum_required(VERSION 3.20)
project(Xhttpd C)

set(CMAKE_C_STANDARD 99)

add_executable(Xhttpd xhttp.c event.h xhttp.h event.c error.h error.c memory.h memory.c common.h connection.h connection.c util.h util.c request.h request.c response.h response.c status.h status.c)
