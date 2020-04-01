/* Copyright 2020 Jack Conger */

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
#include <string.h>
#include <stdio.h>

#include "notcat.h"

#define TERM_INITIAL_CAP 1

#define TS_NORMAL   0
#define TS_PCT      1
#define TS_PCTPAREN 2
#define TS_KV       3

static fmt_item make_literal(size_t len, char *s, char c) {
    fmt_item f;
    f.type = ITEM_TYPE_LITERAL;
    f.str = malloc(len);
    snprintf(f.str, len, s, c);
    return f;
}

static fmt_term parse_term(char *str) {
    fmt_term term;
    fmt_item cur;
    const char *c, *c2;
    char state = TS_NORMAL;
    size_t cap = TERM_INITIAL_CAP;
    term.len = 0;
    term.items = malloc(sizeof(fmt_item) * cap);

    for (c = str; *c; c++) {
        if (term.len == cap) {
            cap *= 2;
            term.items = realloc(term.items, sizeof(fmt_item) * cap);
        }

        switch (state) {
        case TS_KV:
            /* cur.type already set in TS_PCTPAREN code */
            for (c2 = c; *c2 && *c2 != ')'; c2++)
                ;
            if (!*c2) {
                /* oops! literal: `%(x:...` */
                term.items[term.len++] = make_literal(5, "%%(%d:", cur.type);
                c = c2 - 1;
                break;
            } else {
                cur.str = malloc(c2 - c + 1);
                strncpy(cur.str, c, c2 - c);
                cur.str[c2 - c] = '\0';
                term.items[term.len++] = cur;
                c = c2;
            }
            state = TS_NORMAL;
            break;
        case TS_PCTPAREN:
            switch (*c) {
            case 'A': case 'h':
                cur.type = *c;
                if (c[1] != ':') {
                    /* oops! literal: `%(xx` */
                    term.items[term.len++] = make_literal(4, "%%(%c", *c);
                    state = TS_NORMAL;
                    break;
                }
                c++;
                state = TS_KV;
                break;
            case 'i': case 'a': case 's': case 'B':
            case 't': case 'u': case 'c': case 'n':
                cur.type = *c;
                switch (c[1]) {
                case ')':
                    c++;
                    state = TS_NORMAL;
                    cur.str = NULL;
                    term.items[term.len++] = cur;
                    break;
                default:
                    /* oops! literal: `%(x` */
                    term.items[term.len++] = make_literal(4, "%%(%c", *c);
                    state = TS_NORMAL;
                }
                break;
            default:
                /* oops! literal: `%(x` */
                term.items[term.len++] = make_literal(4, "%%(%c", *c);
                state = TS_NORMAL;
            }
            break;
        case TS_PCT:
            switch (*c) {
            case 'i': case 'a': case 's': case 'B':
            case 't': case 'u': case 'c': case 'n':
                cur.type = *c;
                cur.str  = NULL;
                term.items[term.len++] = cur;
                state = TS_NORMAL;
                break;
            case '(':
                state = TS_PCTPAREN;
                break;
            case '%':
                cur.type = ITEM_TYPE_LITERAL;
                cur.str = "%";
                term.items[term.len++] = cur;
                state = TS_NORMAL;
                break;
            default:
                /* oops! literal */
                /* `%x` */
                /* cur.type = ITEM_TYPE_LITERAL;
                cur.str = malloc(sizeof(char) * 3);
                cur.str[0] = '%';
                cur.str[1] = *c;
                cur.str[2] = '\0'; */
                term.items[term.len++] = make_literal(3, "%%%c", *c);
                state = TS_NORMAL;
            }
            break;
        case TS_NORMAL:
            if (*c == '%') {
                state = TS_PCT;
                break;
            }
            cur.type = ITEM_TYPE_LITERAL;
            for (c2 = c; *c2 && *c2 != '%'; c2++)
                ;
            cur.str = malloc(c2 - c + 1);
            strncpy(cur.str, c, c2 - c);
            cur.str[c2 - c] = '\0';
            term.items[term.len++] = cur;
            if (*c2 == '%') {
                state = TS_PCT;
                c = c2;
            } else {
                c = c2 - 1;
            }
            break;
        default:
            exit(111);
        }
    }

    return term;
}

extern format parse_format(size_t len, char **str) {
    size_t i;
    format fmt;

    fmt.len = len;
    fmt.terms = malloc(sizeof(fmt_term) * len);
    for (i = 0; i < len; i++)
        fmt.terms[i] = parse_term(str[i]);

    return fmt;
}