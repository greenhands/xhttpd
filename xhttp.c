#include <stdio.h>

#include "xhttp.h"

int xhttp_create_bind_socket(const char *addr, int port){
    int on = 1;
    struct sockaddr_in address;

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
        log_error(strerror(errno));
    log_info("create socket finish");

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, addr, &address.sin_addr);

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        log_error(strerror(errno));
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        log_error(strerror(errno));

    if (listen(listen_fd, 64) < 0)
        log_error(strerror(errno));

    log_infof(NULL,"server listened on %s:%d", addr, port);
    return listen_fd;
}

void xhttp_init(struct xhttp *http) {
    struct event *ev = event_init();
    set_connect_cb(ev, request_new);

    http->ev = ev;
    ev->http = http;

    http->req_size = N_REQUEST;
    http->reqs = mem_calloc(http->req_size, sizeof(struct request));

    int httpd = xhttp_create_bind_socket("127.0.0.1", XHTTP_LISTEN_PORT);
    event_add(http->ev, httpd, EVENT_READ, handle_connection);
}

void xhttp_start(struct xhttp *http) {
    while (1) {
        event_dispatch(http->ev);
    }
}

int main() {
    struct xhttp http;
    xhttp_init(&http);
    xhttp_start(&http);

    return 0;
}
