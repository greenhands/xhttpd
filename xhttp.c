#include <stdio.h>

#include "xhttp.h"
#include "handler.h"

static int xhttp_create_bind_socket(const char *addr, int port){
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
    if (setsockopt(listen_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0)
        log_error(strerror(errno));
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        log_error(strerror(errno));

    if (listen(listen_fd, 1024) < 0)
        log_error(strerror(errno));

    log_infof(NULL,"server listened on %s:%d", addr, port);
    return listen_fd;
}

void xhttp_init(struct xhttp *http) {
    struct event *ev = event_init();
    set_connect_cb(ev, request_new);

    http->ev = ev;
    ev->http = http;
    http->allowed_methods = HTTP_GET|HTTP_POST;

    http->req_size = N_REQUEST;
    http->reqs = mem_calloc(http->req_size, sizeof(struct request));

    http->handle_size = 0;
    http->handlers = mem_calloc(N_HANDLER, sizeof(struct handler));

    int httpd = xhttp_create_bind_socket("127.0.0.1", XHTTP_LISTEN_PORT);
    event_add(http->ev, httpd, EVENT_READ, handle_connection);

    // TODO: set http handler
}

static struct handler* xhttp_get_handler(struct xhttp *http, char *pattern) {
    for (int i = 0; i < http->handle_size; ++i) {
        if (strequal(http->handlers[i].pattern, pattern))
            return &http->handlers[i];
    }
    return NULL;
}

void xhttp_set_handler(struct xhttp *http, char *pattern, http_handler f) {
    if (!pattern || !f)
        log_error("empty http handler pattern or function");

    if (http->handle_size == N_HANDLER)
        return;

    struct handler *h = xhttp_get_handler(http, pattern);
    if (h) {
        h->func = f;
        log_warnf(NULL, "overwrite http handler: %s", pattern);
        return;
    }
    h = &http->handlers[http->handle_size++];
    h->pattern = pattern;
    h->func = f;
}

void xhttp_start(struct xhttp *http) {
    while (1) {
        event_dispatch(http->ev);
    }
}

static int xhttp_dispatch_handler(struct xhttp *http, struct request *r) {
    struct handler *h = xhttp_get_handler(http, r->path);
    if (!h)
        return -1;
    h->func(r);
    return 0;
}

static int xhttp_handle_resource(struct request *r) {
    char filename[FILENAME_MAX];
    snprintf(filename, FILENAME_MAX, "%s%s", DOC_ROOT, r->path);
    if (r->path[strlen(r->path)-1] == '/') {
        int file_len = strlen(filename);
        snprintf(filename+file_len, FILENAME_MAX-file_len, DOC_INDEX);
    }

    struct stat st;
    if (stat(filename, &st) == -1) {
        if (errno != ENOENT) { // error except file not exist should report error
            log_warnf(__func__ , "stat file: %s error: %s", filename, strerror(errno));
            response_error(r, HTTP_INTERNAL_ERROR, "internal server error");
            return 0;
        } else {
            return EAGAIN;
        }
    }

    response_file(r, filename, st);
    return 0;
}

static void xhttp_excute_cgi(struct request *r) {
    log_debugf(__func__ , "cgi path: %s", r->path);
    response_error(r, HTTP_NOT_FOUND, "not found");
}

void handle_http_request(struct request *r) {
    log_infof(NULL , "receive http request from %s:%d, method: %s, url: %s, body: %s", r->c->remote, r->c->port,
              method_text(r->method), r->uri, r->request_body?r->request_body:"(empty)");

    int ret = xhttp_dispatch_handler(r->http, r);
    if (ret == 0)
        return;

    if (r->method == HTTP_GET && !r->query) {
        if (xhttp_handle_resource(r) == 0) return;
    }

    xhttp_excute_cgi(r);
}
static void xhttp_daemon() {
    switch (fork()) {
        case -1:
            log_error("fork() failed");
            break;
        case 0: /* child */
            break;
        default: /* parent */
            exit(0);
    }

    if (setsid() == -1) /* set process group id and session id */
        log_error("setsid() failed");

    umask(0); /* set file mode mask */

    int fd = open("/dev/null", O_RDWR);
    if (fd == -1)
        log_error("open /dev/null failed");

    if (dup2(fd, STDIN_FILENO) == -1) /* redirect stdin to /dev/null */
        log_error("dup stdin failed");

    if (dup2(fd, STDOUT_FILENO) == -1) /* redirect stdout to /dev/null */
        log_error("dup stdout failed");

    if (fd > STDERR_FILENO) { /* no need to keep this fd */
        if (close(fd) == -1)
            log_error("close fd /dev/null failed");
    }
    log_info("http server is running in background");
}

int main() {
    xhttp_daemon();
    log_init(LOG_PATH);

    struct xhttp http;
    xhttp_init(&http);
    xhttp_set_handler(&http, "/EchoQueryParams", echo_query_params);
    xhttp_start(&http);

    return 0;
}
