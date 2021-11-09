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

static struct ext_content_type ext_table[] = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".xml", "text/plain"},
        {".jpeg", "image/jpeg"},
        {".jpg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".js", "application/javascript"},
        {".json", "application/json"},
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

/**
 * Copy string src to buf, if space in buf if not enough, alloc new buf
 * @param buf dynamic allocated buff
 * @param src source string
 * @return
 */
char* copy_string(char *buf, char *src) {
    if (!buf || strlen(buf) < strlen(src)) {
        mem_free(buf);
        return alloc_copy_string(src);
    }
    strcpy(buf, src);
    return buf;
}

char* int_to_string(int num) {
    if (!int_str)
        int_str = mem_calloc(int_len, sizeof(char));
    snprintf(int_str, int_len, "%d", num);
    return int_str;
}

/**
 * Detect response content-type simply by file extension,
 * for more accurate method refer to https://mimesniff.spec.whatwg.org/#pattern-matching-algorithm
 * @param ext file extension
 * @return content-type
 */
char* ext_to_content_type(char *ext) {
    for (int i = 0;; ++i) {
        if (ext_table[i].ext == NULL)
            return "application/octet-stream";
        if (strequal(ext_table[i].ext, ext))
            return ext_table[i].content_type;
    }
}