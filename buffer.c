/* Copyright 2018 Jack Conger */

/*
 * This file is part of notcat.
 *
 * notcat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * notcat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with notcat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "notcat.h"

struct _buffer {
    size_t len;
    char *start;
    char *curr;
};

static void check_buffer_size(buffer *buf, size_t cn) {
    ptrdiff_t csz = buf->curr - buf->start;
    if (csz + cn > buf->len) {
        ptrdiff_t nsz = (csz == 0 ? 1 : csz * 2);
        while (nsz < (csz + cn)) {
            nsz = nsz * 2;
        }
        buf->len = nsz;
        buf->start = realloc(buf->start, nsz);
        buf->curr = buf->start + csz;
    }
}

extern void put_strn(buffer *buf, size_t cn, const char *c) {
    check_buffer_size(buf, cn);

    size_t i;
    for (i = 0; i < cn; i++) {
        *(buf->curr) = c[i];
        buf->curr++;
    }
}

extern void put_str(buffer *buf, const char *c) {
    put_strn(buf, strlen(c), c);
}

extern void put_char(buffer *buf, char c) {
    check_buffer_size(buf, 1);
    *(buf->curr) = c;
    buf->curr++;
}

extern buffer *new_buffer(size_t initial) {
    buffer *b = malloc(sizeof(buffer));

    b->len = initial;
    b->start = malloc(sizeof(char *) * initial);
    b->curr = b->start;

    return b;
}

extern char *dump_buffer(buffer *buf) {
    char *r = buf->start;
    *(buf->curr) = '\0';
    free(buf);
    return r;
}
