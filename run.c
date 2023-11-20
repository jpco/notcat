/* Copyright 2018-2023 Jack Conger */

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

// Used for setenv() (for now) and posix_spawnp()
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <spawn.h>

#include "notlib/notlib.h"
#include "notcat.h"

int shell_run_opt = 0;
int use_env_opt   = 0;

extern void print_note(const NLNote *n) {
    buffer *buf = new_buffer(BUF_LEN);

    size_t i;
    for (i = 0; i < fmt.len; i++) {
        fmt_note_buf(buf, &fmt.terms[i], n);
        if (i < fmt.len - 1)
            put_char(buf, ' ');
    }

    put_char(buf, '\n');
    char *fin = dump_buffer(buf);
    fputs(fin, stdout);
    free(fin);
}

extern void run_cmd(char *cmd, const NLNote *n) {
    size_t prefix_len = (shell_run_opt ? 4 : 1);
    size_t fmt_len    = (use_env_opt   ? 0 : fmt.len);

    char **cmd_argv = malloc(sizeof(char *) * (1 + prefix_len + fmt_len));

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
        cmd_argv[i+prefix_len] = fmt_note(&fmt.terms[i], n);
    }
    cmd_argv[fmt_len + prefix_len] = NULL;

    // FIXME: Build a new cmd_env instead of munging the local environment
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

    int err;
    pid_t cpid;
    extern char **environ;
    if ((err = posix_spawnp(&cpid, cmd_argv[0], NULL, NULL, cmd_argv, environ))) {
        char *fmt = "posix_spawnp(%s) on %s event";
        int msglen = strlen(cmd_argv[0]) + strlen(fmt) + strlen(current_event);
        char errmsg[msglen + 1];
        snprintf(errmsg, msglen, "posix_spawnp(%s) on %s event", cmd_argv[0], current_event);

        errno = err;
        perror(errmsg);
    }
    wait(NULL);

    for (i = 0; i < fmt_len; i++)
        free(cmd_argv[i+prefix_len]);

    free(cmd_argv);
}
