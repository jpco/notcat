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

#ifndef NOTCAT_H
#define NOTCAT_H

#include "notlib/notlib.h"

// buffer.c

#define BUF_LEN 512

typedef struct _buffer buffer;

extern buffer *new_buffer(size_t);
extern char *dump_buffer(buffer *buf);

extern void put_strn(buffer *, size_t, const char *);
extern void put_str (buffer *, const char *);
extern void put_char(buffer *, char);

// parse.c

#define ITEM_TYPE_LITERAL       128  /* data held in item 'str' */
#define ITEM_TYPE_CONDITIONAL   129  /* "if" held in item 'chr', "then" held in item 'subterm' */

typedef struct _fmt_item fmt_item;

typedef struct _fmt_term {
    size_t len;
    fmt_item *items;
} fmt_term;

struct _fmt_item {
    unsigned char type;
    char *str;
    char chr;
    fmt_term subterm;
};

typedef struct _format {
    size_t len;
    fmt_term *terms;
} format;

extern format fmt;

extern format parse_format(size_t len, char **str);

// fmt.c

extern char *current_event;

extern char *str_urgency(const enum NLUrgency urgency);
extern void fmt_note_buf(buffer *buf, fmt_term *fmt, const NLNote *n);
extern char *fmt_note(fmt_term *fmt, const NLNote *n);

// run.c

extern char **fmt_string_opt;
extern size_t fmt_string_opt_len;
extern int shell_run_opt;
extern int use_env_opt;

extern void print_note(const NLNote *n);
extern void run_cmd(char *cmd, const NLNote *n);

// capabilities.c

extern char **capabilities;

extern void fmt_capabilities(void);
extern void add_capability(char *);

// client.c

extern int send_note(int argc, char **argv);
extern int close_note(char *arg);
extern int get_capabilities(void);
extern int get_server_information(void);
extern int listen_for_signals(void);
extern int invoke_action(int argc, char **argv);

#endif
