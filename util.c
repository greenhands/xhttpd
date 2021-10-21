//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "util.h"

int set_nonblocking(int fd){
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}