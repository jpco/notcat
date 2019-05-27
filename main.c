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
#include <stdio.h>
#include <string.h>

#include "notlib/notlib.h"
#include "notcat.h"

/**
 * TODO: args for:
 *  - hreffmt
 *  - markupfmt
 */

static char *on_notify_opt = "echo";
static char *on_close_opt = NULL;
static char *on_empty_opt = NULL;

static void usage(char *arg0, int code) {
    fprintf(stderr, "Usage:\n"
            "  %s [-h | --help]\n"
            "  %s [ send <opts> | close <id> | getcapabilities | getserverinfo ]\n"
            "  %s [-se] [--capabilities=<cap1>,<cap2>...] \\\n"
            "           [--on-notify=<cmd>] [--on-close=<cmd>] [--on-empty=<cmd>] \\\n"
            "           [--] [format]...\n"
            "\n"
            "Options:\n"
            "  --capabilities=<cap1>,<cap2>...\n"
            "             Additional capabilities to advertise\n\n"
            "  --on-notify=<cmd>  Command to run on each notification created\n\n"
            "  --on-close=<cmd>   Command to run on each notification closed\n\n"
            "  --on-empty=<cmd>   Command to run when no notifications remain\n\n"
            "  -s, --shell       Execute commands in a subshell\n"
            "  -e, --env         Pass notifications to commands in the environment\n"
            "  -h, --help        This help text\n"
            "  --                Stop flag parsing\n"
            "\n"
            "For more detailed information and options for the 'send' subcommand,\n"
            "consult `man 1 notcat`.\n",
           arg0, arg0, arg0);

    exit(code);
}

static void notcat_getopt(int argc, char **argv) {
    size_t fo_idx = 0;
    char *arg0 = argv[0];

    int av_idx;
    int skip = 0;
    for (av_idx = 1; av_idx < argc; av_idx++) {
        char *arg = argv[av_idx];
        if (!skip && arg[0] == '-' && arg[1]) {
            if (arg[1] == '-' && !arg[2]) {
                skip = 1;
                continue;
            }
            if (arg[1] != '-') {
                char *c;
                for (c = arg + 1; *c; c++) {
                    switch (*c) {
                    case 's':
                        shell_run_opt = 1;
                        break;
                    case 'e':
                        use_env_opt = 1;
                        break;
                    case 'h':
                        usage(arg0, 0);
                    default:
                        usage(arg0, 2);
                    }
                }
                continue;
            }

            arg = arg + 2;
            if (!strncmp("on-notify=", arg, 10)) {
                on_notify_opt = arg + 10;
            } else if (!strncmp("on-close=", arg, 9)) {
                on_close_opt = arg + 9;
            } else if (!strncmp("on-empty=", arg, 9)) {
                on_empty_opt = arg + 9;
            } else if (!strncmp("capabilities=", arg, 13)) {
                char *ce, *cc = arg + 13;
                for (ce = cc; *ce; ce++) {
                    if (*ce != ',')
                        continue;
                    *ce = '\0';
                    add_capability(cc);
                    cc = ce + 1;
                }
                add_capability(cc);
            } else if (!strcmp("shell", arg)) {
                shell_run_opt = 1;
            } else if (!strcmp("env", arg)) {
                use_env_opt = 1;
            } else if (!strcmp("help", arg)) {
                usage(arg0, 0);
            }
            continue;
        }
        argv[fo_idx++] = argv[av_idx];
    }

    if (fo_idx > 0) {
        fmt_string_opt_len = fo_idx;
        fmt_string_opt = argv;
    }
}

static uint32_t rc = 0;

void inc(const NLNote *n) {
    ++rc;
    if (!strcmp(on_notify_opt, "echo") && !shell_run_opt) {
        print_note(n);
    } else if (*on_notify_opt) {
        run_cmd(on_notify_opt, n);
    }
    fflush(stdout);
}

void dec(const NLNote *n) {
    --rc;
    if (on_close_opt) {
        if (!strcmp(on_close_opt, "echo") && !shell_run_opt) {
            print_note(n);
        } else if (*on_close_opt) {
            run_cmd(on_close_opt, n);
        }
    }
    if (rc == 0 && on_empty_opt) {
        if (!strcmp(on_empty_opt, "echo") && !shell_run_opt) {
            printf("\n");
        } else if (*on_empty_opt) {
            run_cmd(on_empty_opt, NULL);
        }
    }
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "send")) {
        return send_note(argc, argv);
    }
    if (argc > 1 && !strcmp(argv[1], "close")) {
        if (argc != 3) usage(argv[0], 2);
        return close_note(argv[2]);
    }
    if (argc > 1 && !strcmp(argv[1], "getcapabilities")) {
        if (argc != 2) usage(argv[0], 2);
        return get_capabilities();
    }
    if (argc > 1 && !strcmp(argv[1], "getserverinfo")) {
        if (argc != 2) usage(argv[0], 2);
        return get_server_information();
    }

    notcat_getopt(argc, argv);
    if (use_env_opt) {
        add_capability("body");
    } else fmt_capabilities();

    NLNoteCallbacks cbs = {
        .notify = inc,
        .close = dec,
        .replace = print_note
    };
    NLServerInfo info = {
        .app_name = "notcat",
        .author = "jpco",
        .version = "0.2"
    };

    notlib_run(cbs, capabilities, &info);
    return 0;
}
