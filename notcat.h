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

// fmt.c

extern void fmt_note_buf(buffer *buf, char *fmt, const NLNote *n);
extern char *fmt_note(char *fmt, const NLNote *n);

#endif
