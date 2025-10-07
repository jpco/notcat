/* Copyright 2025 Jack Conger */

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
#include <sys/types.h>

#include <stdio.h>

#include "notcat.h"

#define EOB             0
#define BODY_TEXT       1
#define BODY_TAG        2
#define BODY_TAG_CLOSE  3

typedef struct {
    char type;
    const char *str;
    size_t len;
} token;

static ssize_t next_token(const char *in, token *t) {
    size_t i = 0;
    char c = in[i];
    switch (c) {
    case '\0':
        t->type = EOB;
        t->str = NULL;
        t->len = 0;
        return 0;
    case '<':
        i++;
        if (in[i] == '/') {
            i++;
            t->type = BODY_TAG_CLOSE;
            t->str = in + 2;
        } else {
            t->type = BODY_TAG;
            t->str = in + 1;
        }
        break;
    default:
        t->type = BODY_TEXT;
        t->str = in;
    }
    switch (t->type) {
    case BODY_TAG:
        for (; (c = in[i]); ++i) {
            if (c == '>') {
                t->len = i - 1;
                return i + 1;
            }
        }
        return -1;
    case BODY_TAG_CLOSE:
        for (; (c = in[i]); ++i) {
            if (c == '>') {
                t->len = i - 2;
                return i + 1;
            }
        }
        return -1;
    default:
        for (; (c = in[i]); ++i) {
            if (c == '<') {
                t->len = i;
                return i;
            }
        }
        t->len = i;
        return i;
    }
}

#define TOP_LEVEL_NODE  0
#define TEXT_NODE       1
#define TAG_NODE        2

typedef struct node {
    char type;
    const char *str;
    size_t strlen;
    struct node *parent;
    struct node *children[50];
    size_t childlen;
} node;

static void free_tree(node *tree) {
    for (int i = 0; i < tree->childlen; i++)
        free_tree(tree->children[i]);
    free(tree);
}

static node *parse(const char *in) {
    token t;
    size_t i = 0;
    ssize_t adv;
    node *tree = calloc(1, sizeof(node));
    node *curr = tree;
    while ((adv = next_token(in + i, &t)) >= 0) {
        i += adv;
        switch (t.type) {
        case EOB:
            goto after_loop;
        case BODY_TEXT: {
            /* FIXME: we don't check that childlen < 50 */
            curr->children[curr->childlen] = calloc(1, sizeof(node));
            node *child = curr->children[curr->childlen++];

            child->parent = curr;
            child->type = TEXT_NODE;
            child->str = t.str;
            child->strlen = t.len;
            break;
        }
        case BODY_TAG: {
            /* FIXME: we don't check that childlen < 50 */
            curr->children[curr->childlen] = calloc(1, sizeof(node));
            node *child = curr->children[curr->childlen++];

            child->parent = curr;
            child->type = TAG_NODE;
            child->str = t.str;
            child->strlen = t.len;

            /* ignore tag attributes */
            for (int i = 0; i < child->strlen; i++) {
                if (child->str[i] == ' ') {
                    child->strlen = i;
                    break;
                }
            }

            curr = child;
            break;
        }
        case BODY_TAG_CLOSE:
            if (t.len != curr->strlen || strncmp(t.str, curr->str, t.len)) {
                /* closing the wrong tag. note we look at the whole tag contents here */
                free_tree(tree);
                return NULL;
            }
            if (curr->parent == NULL) {
                /* closing tag without opening tag */
                free_tree(tree);
                return NULL;
            }
            curr = curr->parent;
            break;
        default:
            /* code bug */
            free_tree(tree);
            return NULL;
        }
    }
after_loop:
    if (adv == -1) {
        /* tokenizer sadness */
        free_tree(tree);
        return NULL;
    }
    if (curr != tree) {
        /* unclosed tags. maybe we don't care about this? */
        free_tree(tree);
        return NULL;
    }
    return tree;
}

static int last_char_was_newline = 0;

static struct {
    const char *code;
    size_t len;
    char out;
} ampcodes[] = {
    {"&amp;", 5, '&'},
    {"&lt;", 4, '<'},
    {"&gt;", 4, '>'},
    {"&quot;", 6, '"'},
    {"&apos;", 6, '\''},
    {NULL, 0, '\0'}
};

static int findampcode(const char *in, char *out) {
    for (int i = 0; ampcodes[i].code != NULL; i++) {
        if (strncmp(ampcodes[i].code, in, ampcodes[i].len) == 0) {
            return i;
        }
    }
    return -1;
}

static size_t drain_node(node *n, char *out) {
    size_t ret = 0;
    switch (n->type) {
    case TAG_NODE:
        /* right now we just strip all tags */
        for (int i = 0; i < n->childlen; i++)
            ret += drain_node(n->children[i], out + ret);
        break;
    case TOP_LEVEL_NODE:
        for (int i = 0; i < n->childlen; i++)
            ret += drain_node(n->children[i], out + ret);
        break;
    case TEXT_NODE:
        for (int i = 0; i < n->strlen; i++) {
            if (n->str[i] == '\n') {
                if (!last_char_was_newline)
                    out[ret++] = ' ';
                last_char_was_newline = 1;
            } else if (n->str[i] == '&') {
                last_char_was_newline = 0;
                int aci = findampcode(n->str + i, out + ret);
                if (aci >= 0) {
                    out[ret++] = ampcodes[aci].out;
                    i += ampcodes[aci].len - 1;
                } else {
                    out[ret++] = n->str[i];
                }
            } else {
                last_char_was_newline = 0;
                out[ret++] = n->str[i];
            }
        }
    }
    return ret;
}

// NOTE: we do not check the length of out. this is fine as long as we only strip markup
extern int markup_body(const char *in, char *out) {
    last_char_was_newline = 0;
    node *tree = parse(in);
    if (tree == NULL) {
        return -1;   /* Fall back to raw text */
    }
    size_t i = drain_node(tree, out);
    out[i] = '\0';
    free_tree(tree);
    return 0;
}

/* vim: set ft=c tabstop=4 softtabstop=4 shiftwidth=4 expandtab textwidth=0: */
