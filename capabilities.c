/* Copyright 2019 Jack Conger */

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
#include <stdbool.h>

#include "notcat.h"

static char *BASE_capabilities[] = {
    "actions",
    "x-notlib-remote-actions",
    "x-canonical-private-synchronous",
    "x-dunst-stack-tag",
    NULL, NULL, NULL, NULL
};
char **capabilities = BASE_capabilities;

static size_t caps_len = 4;
static size_t caps_cap = 8;

static void grow_capabilities(void) {
    caps_cap *= 2;
    char **new = calloc(sizeof(char *), caps_cap);
    memcpy(new, capabilities, sizeof(char *) * caps_len);

    if (capabilities != BASE_capabilities)
        free (capabilities);

    capabilities = new;
}

extern void add_capability(char *cap) {
    size_t i;
    for (i = 0; cap[i]; i++) {
        if (!strchr("abcdefghijklmnopqrstuvwxyz0123456789-", cap[i])) {
            fprintf(stderr, "Capabilities must be made up of lower-case "
                            "alphanumeric characters or dashes\n");
            exit(2);
        }
    }

    char **curr;
    for (curr = capabilities; *curr != NULL; curr++) {
        if (!strcmp(*curr, cap))
            return;
    }

    if (caps_len + 1 >= caps_cap)
        grow_capabilities();

    capabilities[caps_len++] = cap;
}

static bool body_term(fmt_term t) {
    size_t i;
    for (i = 0; i < t.len; i++) {
        switch (t.items[i].type) {
        case 'b': case 'B':
            return true;
        case ITEM_TYPE_CONDITIONAL:
            if (t.items[i].chr == 'b' || t.items[i].chr == 'B')
                return true;
            if (body_term(t.items[i].subterm))
                return true;
        }
    }
    return false;
}

static bool body_fmt_term(fmt_term t) {
    size_t i;
    for (i = 0; i < t.len; i++) {
        switch (t.items[i].type) {
        case 'B':
            return true;
        case ITEM_TYPE_CONDITIONAL:
            if (t.items[i].chr == 'B')
                return true;
            if (body_fmt_term(t.items[i].subterm))
                return true;
        }
    }
    return false;
}

extern void fmt_capabilities(void) {
    size_t i;
    bool body = false;
    bool markup = false;
    for (i = 0; i < fmt.len; i++) {
        if (body_fmt_term(fmt.terms[i])) {
            body = true;
            markup = true;
        } else if (body_term(fmt.terms[i])) {
            body = true;
        }
        if (body && markup)
            break;
    }
    if (body)
        add_capability("body");
    if (markup)
        add_capability("body-markup");
}

/* vim: set ft=c tabstop=4 softtabstop=4 shiftwidth=4 expandtab textwidth=0: */
