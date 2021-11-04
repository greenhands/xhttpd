//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "util.h"

static const int int_len = 16;
static char *int_str;

struct ext_content_type {
    char *ext;
    char *content_type;
};

static struct ext_content_type exts[] = {
        {".html", "text/html"},
        {NULL, NULL},
};

int set_nonblocking(int fd){
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

char* get_token(char *str, int *len) {
    char *head = str + strspn(str, EMPTY_CHAR);
    if (*head == '\0')
        return NULL;
    char *tail = strpbrk(head, EMPTY_CHAR);
    if (!tail) {
        *len = strlen(head);
    }else {
        *len = tail - head;
    }
    return head;
}

// the string should dealloc by user
char* alloc_copy_string(char *str) {
    char *new_str = mem_calloc(strlen(str)+1, sizeof(char));
    strcpy(new_str, str);
    return new_str;
}

char* alloc_copy_nstring(char *str, int n) {
    char *new_str = mem_calloc(n+1, sizeof(char));
    memmove(new_str, str, n);
    return new_str;
}

char* int_to_string(int num) {
    if (!int_str)
        int_str = mem_calloc(int_len, sizeof(char));
    snprintf(int_str, int_len, "%d", num);
    return int_str;
}

char* ext_to_content_type(char *ext) {
    for (int i = 0;; ++i) {
        if (exts[i].ext == NULL)
            return "application/oct-stream";
        if (strequal(exts[i].ext, ext))
            return exts[i].content_type;
    }
}