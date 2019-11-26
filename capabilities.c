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

#include "notcat.h"

static char *BASE_capabilities[] = {"actions", "x-notlib-remote-actions", NULL, NULL};
char **capabilities = BASE_capabilities;

static size_t caps_len = 2;
static size_t caps_cap = 4;

static void grow_capabilities() {
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

extern void fmt_capabilities() {
    size_t i;
    char pct = 0;
    for (i = 0; i < fmt_string_opt_len; i++) {
        char *c;
        for (c = fmt_string_opt[i]; *c; c++) {
            if (pct) {
                if (*c == 'B') {
                    add_capability("body");
                }
            } else if (*c == '%') {
                pct = 1;
            }
        }
    }
}
