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

#include <stdio.h>
#include <string.h>

#include "notcat.h"

void cmp_fmt(char *in, char *want) {
    NLNote note = {
        .id = 13,
        .summary = "summary",
        .body = "body",
    };

    format fmt = parse_format(1, &in);
    if (fmt.len != 1) {
        fprintf(stderr, "FAILED: %s produced format of length != 1", in);
        return;
    }
    char *out = fmt_note(fmt.terms, &note);
    if (strcmp(out, want)) {
        fprintf(stderr, "FAILED: %s => %s -- got %s\n", in, want, out);
        return;
    }
    fprintf(stderr, "passed: %s => %s\n", in, want);
    return;
}

void test_fmt() {
    cmp_fmt("test", "test");
    cmp_fmt("%", "%");
    cmp_fmt("%(", "%(");
    cmp_fmt("%(:", "%(:");
    cmp_fmt("%()", "%()");
    cmp_fmt("%(h:", "%(h:");
    cmp_fmt("%(h:test", "%(h:test");
    cmp_fmt("%(s:", "%(s:");
    cmp_fmt("%(s:test", "%(s:test");
    cmp_fmt("%(?", "%(?");
    cmp_fmt("%(?B", "%(?B");
    cmp_fmt("%(?B:", "%(?B:");
    cmp_fmt("%(?B:test", "%(?B:test");

    cmp_fmt("%B", "body");
    cmp_fmt("before%Bafter", "beforebodyafter");
    cmp_fmt("%(B)", "body");

    cmp_fmt("%(?a:haveappname)", "");
    cmp_fmt("%(?B:havebody)", "havebody");
    cmp_fmt("%(?B:havebody)after", "havebodyafter");
    cmp_fmt("%(?B:%B)", "body");
    cmp_fmt("%(?B:%(B))", "body");
}

int main() {
    test_fmt();
}
