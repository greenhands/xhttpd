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