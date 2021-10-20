#include <stdio.h>

#define XHTTP_LISTEN_PORT 5000

#include "xhttp.h"

int xhttp_create_bind_socket(const char *addr, int port){
    log_info("create socket finish");
    return 0;
}

int main() {
    int listen_fd;

    listen_fd = xhttp_create_bind_socket("127.0.0.1", XHTTP_LISTEN_PORT);

    return 0;
}
