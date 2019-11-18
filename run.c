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
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#include "notlib/notlib.h"
#include "notcat.h"

static char *default_fmt_string_opt[] = {"%s"};
char **fmt_string_opt = default_fmt_string_opt;
size_t fmt_string_opt_len = 1;
int shell_run_opt = 0;
int use_env_opt = 0;

extern void print_note(const NLNote *n) {
    buffer *buf = new_buffer(BUF_LEN);

    const char *fmt_override = NULL;
    if (nl_get_string_hint(n, "format", &fmt_override)) {
        fmt_note_buf(buf, fmt_override, n);
    } else {
        size_t fmt_idx;
        for (fmt_idx = 0; fmt_idx < fmt_string_opt_len; fmt_idx++) {
            fmt_note_buf(buf, fmt_string_opt[fmt_idx], n);
            if (fmt_idx < fmt_string_opt_len - 1)
                put_char(buf, ' ');
        }
    }

    put_char(buf, '\n');
    char *fin = dump_buffer(buf);
    fputs(fin, stdout);
    free(fin);
}

extern void run_cmd(char *cmd, const NLNote *n) {
    size_t prefix_len = (shell_run_opt ? 4 : 1);
    size_t fmt_len = (use_env_opt ? 0 : fmt_string_opt_len);

    char **cmd_argv = malloc(sizeof(char *)
            * (1 + prefix_len + fmt_len));

    static char *sh = NULL;
    if (!sh && !(sh = getenv("SHELL")))
        sh = "/bin/sh";

    if (shell_run_opt) {
        cmd_argv[0] = sh;
        cmd_argv[1] = "-c";
        cmd_argv[2] = cmd;
        cmd_argv[3] = "notcat";
    } else {
        cmd_argv[0] = cmd;
    }

    size_t i;
    for (i = 0; i < fmt_len; i++) {
        cmd_argv[i+prefix_len] = fmt_note(fmt_string_opt[i], n);
    }
    cmd_argv[fmt_len + prefix_len] = NULL;

    if (use_env_opt) {
        setenv("NOTCAT_EVENT", current_event, 1);

        if (n != NULL) {
            char str[12];  // big enough to store a 32-bit int

            setenv("NOTE_APP_NAME", n->appname, 1);
            setenv("NOTE_SUMMARY", n->summary, 1);
            setenv("NOTE_BODY", n->body, 1);
            setenv("NOTE_URGENCY", str_urgency(n->urgency), 1);

            snprintf(str, 12, "%u", n->id);
            setenv("NOTE_ID", str, 1);

            snprintf(str, 12, "%d", n->timeout);
            setenv("NOTE_TIMEOUT", str, 1);

            char *h = nl_get_hint_as_string(n, "category");
            if (h != NULL) {
                setenv("NOTE_CATEGORY", h, 1);
                free(h);
            }

            // TODO: Figure out a sensible way to do hints
        }
    }

    switch (fork()) {
    case -1:
        perror("Could not fork");
        break;
    case 0: {
        extern char **environ;
        int err = execve(cmd_argv[0], cmd_argv, environ);
        perror(cmd_argv[0]);
        exit(err);
    }
    default:
        wait(NULL);
    }

    for (i = 0; i < fmt_len; i++)
        free(cmd_argv[i+prefix_len]);

    free(cmd_argv);
}
