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
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "notlib/notlib.h"
#include "notcat.h"

/*
 * Proposed format syntax (incomplete)
 *
 * id               %i
 * app_name         %a
 * summary          %s
 * body             %B          - also %(B:30) for 'first 30 chars of b'?
 * actions          ???         - TODO - %(A:key)?
 * arbitrary hints  %(h:key)
 * category         %c
 * expire_timeout   %t
 * urgency          %u
 */

char *current_event = "ERROR";

extern char *str_urgency(const enum NLUrgency u) {
    switch (u) {
        case URG_NONE: return "NONE";
        case URG_LOW:  return "LOW";
        case URG_NORM: return "NORMAL";
        case URG_CRIT: return "CRITICAL";
        default:       return "IDFK";
    }
}

static void put_urgency(buffer *buf, const enum NLUrgency u) {
    put_str(buf, str_urgency(u));
}

static void put_uint(buffer *buf, uint32_t u) {
    if (u == 0) {
        put_char(buf, '0');
        return;
    }

    char chs[10] = {0, 0, 0, 0, 0};
    size_t idx;
    for (idx = 9; u > 0; idx--) {
        chs[idx] = '0' + (u % 10);
        u /= 10;
    }

    for (idx = 0; idx < 10; idx++) {
        if (chs[idx] != 0)
            put_char(buf, chs[idx]);
    }
}

static void put_int(buffer *buf, int32_t i) {
    if (i < 0) {
        put_char(buf, '-');
        put_uint(buf, UINT32_MAX - (uint32_t) i + 1);
    } else {
        put_uint(buf, i);
    }
}

static void fmt_body(const char *in, char *out) {
    size_t i;
    char c;
    for (i = 0; (c = in[i]); ++i) {
        switch (c) {
        case '\n':
            out[i] = ' ';
            break;
        default:
            out[i] = c;
        }
    }
    out[i] = '\0';
}

static void put_hint(buffer *buf, const NLNote *n, const char *name) {
    char *hs;
    if (!(hs = nl_get_hint_as_string(n, name)))
        return;
    put_str(buf, hs);
    free(hs);
}

static void put_action(buffer *buf, const NLNote *n, const char *key) {
    const char *an;
    if (!(an = nl_action_name(n, key)))
        return;
    put_str(buf, an);
}

#define NORMAL      0
#define PCT         1
#define PCTPAREN    2
#define HINT        3
#define ACTION      4

extern void fmt_note_buf(buffer *buf, const char *fmt, const NLNote *n) {
    const char *c;
    char *body = NULL;
    char state = NORMAL;

    for (c = fmt; *c; c++) {
        switch (state) {
        case HINT: {
            char *c2;
            ++c;
            for (c2 = (char *)c; *c2 && *c2 != ')'; c2++)
                ;
            if (!*c2) {
                put_str(buf, "%(h:");
                put_str(buf, c);
                c = c2 - 1;
            } else {
                *c2 = '\0';
                put_hint(buf, n, c);
                *c2 = ')';
                c = c2;
                state = NORMAL;
            }
            break;
        }
        case ACTION: {
            char *c2;
            ++c;
            for (c2 = (char *)c; *c2 && *c2 != ')'; c2++)
                ;
            if (!*c2) {
                put_str(buf, "%(A:");
                put_str(buf, c);
                c = c2 - 1;
            } else {
                *c2 = '\0';
                put_action(buf, n, c);
                *c2 = ')';
                c = c2;
                state = NORMAL;
            }
            break;
        }
        case PCTPAREN:
            switch (*c) {
            case 'h':
                if (c[1] == ':') {
                    state = HINT;
                    break;
                }
            case 'A':
                if (c[1] == ':') {
                    state = ACTION;
                    break;
                }
            // TODO: allow things like '%(s)'
            default:
                put_str(buf, "%(");
                put_char(buf, *c);
                state = NORMAL;
                break;
            }
            break;
        case PCT:
            switch (*c) {
            case 'i':
                if (n) put_uint(buf, n->id);
                break;
            case 'a':
                if (n && n->appname) put_str(buf, n->appname);
                break;
            case 's':
                if (n && n->summary) put_str(buf, n->summary);
                break;
            case 'B':
                if (!n) break;
                if (body == NULL) {
                    body = (n->body == NULL ? "" : malloc(1 + strlen(n->body)));
                    fmt_body(n->body, body);
                }
                put_str(buf, body);
                break;
            case 't':
                if (n) put_int(buf, n->timeout);
                break;
            case 'u':
                if (n) put_urgency(buf, n->urgency);
                break;
            case 'c':
                put_hint(buf, n, "category");
                break;
            case 'n':
                put_str(buf, current_event);
                break;
            case '%':
                put_char(buf, '%');
                break;
            case '(':
                state = PCTPAREN;
                break;
            default:
                put_char(buf, '%');
                put_char(buf, *c);
            }
            if (state == PCT) {
                state = NORMAL;
            }
            break;
        case NORMAL:
            switch (*c) {
            case '%':
                state = PCT;
                break;
            default:
                put_char(buf, *c);
            }
            break;
        }
    }

    switch (state) {
    case PCT:
        put_char(buf, '%');
        break;
    case PCTPAREN:
        put_str(buf, "%(");
        break;
    }

    if (body != NULL && n && n->body != NULL)
        free(body);
}

extern char *fmt_note(const char *fmt, const NLNote *n) {
    buffer *buf = new_buffer(BUF_LEN);
    fmt_note_buf(buf, fmt, n);
    return dump_buffer(buf);
}
