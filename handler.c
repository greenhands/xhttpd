//
// Created by Haiwei Zhang on 2021/11/11.
//
#include "handler.h"

void echo_query_params(struct request *r) {
    char body[4096];
    int b = 0;
    char *key = "Key:", *value = " Value:";
    int lk = strlen(key);
    int lv = strlen(value);
    if (!r->query) {
        sprintf(body, "no query params\n");
        b = strlen(body);
    } else {
        char *query = r->query;
        int p;
        while (1) {
            p = strcspn(query, "&");
            if (b+p+lk+lv+2 > 4096)
                break;
            memmove(body+b, key, lk);
            b+=lk;
            for (int i = 0; i < p; ++i) {
                if (query[i] != '=') body[b++] = query[i];
                else {
                    memmove(body+b, value, lv);
                    b+=lv;
                }
            }
            body[b++] = '\n';
            if (query[p] == '\0')
                break;
            query += p+1;
        }
        body[b] = '\0';
    }

    response_body(r, body, b);
}