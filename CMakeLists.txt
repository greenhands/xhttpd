cmake_minimum_required(VERSION 3.20)
project(Xhttpd C)

set(CMAKE_C_STANDARD 99)
INCLUDE_DIRECTORIES(./include)

add_executable(Xhttpd xhttp.c include/event.h include/xhttp.h event.c include/error.h error.c include/memory.h memory.c include/common.h include/connection.h connection.c include/util.h util.c include/request.h request.c include/response.h response.c include/status.h status.c)
