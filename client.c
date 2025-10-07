/* Copyright 2019, 2025 Jack Conger */

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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <dbus/dbus.h>

#include "notcat.h"

#define NF_INTERFACE    "org.freedesktop.Notifications"
#define NF_NAME         "org.freedesktop.Notifications"
#define NF_PATH         "/org/freedesktop/Notifications"

static DBusConnection *dconnect(void) {
    DBusConnection *conn;
    DBusError error;

    dbus_error_init(&error);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "DBus connection error: %s\n", error.message);
        return NULL;
    }
    return conn;
}

static DBusMessage *dcall(DBusConnection *conn, char *method,
        void (*infunc)(DBusMessage *, void *),
        void *user_data) {
    DBusError error;
    DBusMessage *req, *resp;

    if (conn == NULL)
        return NULL;

    dbus_error_init(&error);
    req = dbus_message_new_method_call(NF_NAME, NF_PATH, NF_INTERFACE, method);
    if (infunc != NULL)
        infunc(req, user_data);
    resp = dbus_connection_send_with_reply_and_block(conn, req, 1000, &error);
    dbus_message_unref(req);

    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "DBus call error (%s): %s\n", method, error.message);
        return NULL;
    }
    return resp;
}

typedef struct {
    int (*callback_func)(const char *, DBusMessage *, void *);
    void *user_data;
} signal_handler_struct;

static DBusHandlerResult handle_message(DBusConnection *conn, DBusMessage *msg, void *d) {
    signal_handler_struct *shs = (signal_handler_struct *)d;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (strcmp(dbus_message_get_interface(msg), NF_INTERFACE) != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (!shs->callback_func(dbus_message_get_member(msg), msg, shs->user_data))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void dlisten(DBusConnection *conn,
        int (*callback_func)(const char *, DBusMessage *, void *),
        void *user_data) {
    DBusError error;
    DBusObjectPathVTable vt;
    signal_handler_struct shs;

    if (conn == NULL)
        return;

    dbus_error_init(&error);
    dbus_bus_add_match(conn, "type='signal',sender='" NF_NAME "'", NULL);

    vt.message_function = handle_message;
    vt.unregister_function = NULL;
    shs.user_data = user_data;
    shs.callback_func = callback_func;
    dbus_connection_try_register_object_path(conn, NF_PATH, &vt, &shs, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Listening for signal: %s\n", error.message);
        return;
    }
    while (dbus_connection_read_write_dispatch(conn, -1))
        ;
}

static int get_int(char *str, long *out) {
    char *end;
    *out = strtol(str, &end, 10);
    return (str != NULL && *str != '\0' && *end == '\0');
}

static int sync_send_callback(const char *signal_name,
                              DBusMessage *msg, void *data) {
    dbus_uint32_t id, close_reason;
    const char *action_key;
    long want_id = *(long *)data;
    DBusError error;
    dbus_error_init(&error);
    if (strcmp(signal_name, "NotificationClosed") == 0) {
        dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &id,
                DBUS_TYPE_UINT32, &close_reason,
                DBUS_TYPE_INVALID);
        if (dbus_error_is_set(&error) || want_id != id)
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (strcmp(signal_name, "ActionInvoked") == 0) {
        dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &id,
                DBUS_TYPE_STRING, &action_key,
                DBUS_TYPE_INVALID);
        if (dbus_error_is_set(&error) || want_id != id)
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        printf("%s\n", action_key);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static struct {
    char *name;
    size_t len;
    char mode;
} longopts[] = {
    {"--app-name=", 11, 'a'},
    {"--timeout=",  10, 't'},
    {"--id=",        5, 'i'},
    {"--actions=",  10, 'A'},
    {"--hint=",      7, 'h'},
    {"--urgency=",  10, 'u'},
    {"--category=", 11, 'c'},
    {"--icon=",      7, 'I'},
    {NULL, 0, '\0'}
};

typedef struct {
    char *app_name, *app_icon;
    char *summary, *body;
    char urgency, *category;
    long id, timeout;

    char *actions[50];
    char *hints[50];
    size_t nactions, nhints;
} note_t;

static void set_note_action(char *arg, DBusMessageIter *iter) {
    char *c = arg;
    while (*arg) {
        while (*c && *c != ',') c++;
        char reset = 0;
        if (*c) {
            reset = 1;
            *c = '\0';
        }

        char *c2 = arg;
        for (; *c2; c2++) {
            if (*c2 != ':')
                continue;
            *c2 = '\0';
            char *c3 = c2+1;
            dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &arg);
            dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &c3);
            *c2 = ':';
            break;
        }
        if (!*c2) {
            dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &arg);
            dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &arg);
        }

        arg = c + reset;
        if (reset) *c = ',';
        c += reset;
    }
}

static void set_note_hint(char *arg, DBusMessageIter *iter) {
#if 0
    char *c2 = arg, *fs = arg, *name = NULL, *value = NULL;
    for (; *c2; c2++) {
        if (*c2 == ':') {
            if (name == NULL) {
                *c2 = '\0';
                name = c2 + 1;
            } else if (value == NULL) {
                *c2 = '\0';
                value = c2 + 1;
            }
        }
    }
    if (value == NULL) {
        if (name == NULL) {
            fprintf(stderr, "Hint must be formatted as "
                    "NAME:VALUE or TYPE:NAME:VALUE\n");
            return 2;
        }
        value = name;
        name = fs;
        fs = "s";
    }
    if (*fs == '\0')
        fs = "s";

    if (!g_variant_type_string_is_valid(fs)) {
        fprintf(stderr, "'%s' is not a valid D-Bus variant type\n",
                fs);
        return 2;
    }

    /* Fix forgotten quotes.  For ease of use! */
    if (*fs == 's' && fs[1] == '\0' && *value != '"') {
        char *nv = malloc(strlen(value) + 3);
        sprintf(nv, "\"%s\"", value);
        value = nv;
    }

    GError *e = NULL;
    GVariant *gv = g_variant_parse(G_VARIANT_TYPE(fs),
            value, c2, NULL, &e);
    if (e != NULL) {
        fprintf(stderr, "Could not parse '%s' as '%s': %s\n",
                value, fs, e->message);
        return 2;
    }
    g_variant_builder_add(hints, "{sv}", name, gv);
    break;
#endif
}

// app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout
static void set_note(DBusMessage *msg, void *notep) {
    note_t *note = (note_t *)notep;
    dbus_uint32_t id = (dbus_uint32_t)note->id;
    dbus_int32_t timeout = (dbus_int32_t)note->timeout;

    DBusMessageIter iter, actiter, hintiter;
    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &note->app_name);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &id);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &note->app_icon);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &note->summary);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &note->body);

    // actions - type "as"
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &actiter);
        for (size_t i = 0; i < note->nactions; i++)
            set_note_action(note->actions[i], &actiter);
    dbus_message_iter_close_container(&iter, &actiter);

    // hints - type "a{sv}"
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &hintiter);
        if (note->urgency != -1) {  // urgency
            DBusMessageIter ui, uiv;
            static const char *urgstr = "urgency";
            dbus_message_iter_open_container(&hintiter, DBUS_TYPE_DICT_ENTRY, NULL, &ui);
                dbus_message_iter_append_basic(&ui, DBUS_TYPE_STRING, &urgstr);
                dbus_message_iter_open_container(&ui, DBUS_TYPE_VARIANT, "y", &uiv);
                    dbus_message_iter_append_basic(&uiv, DBUS_TYPE_BYTE, &note->urgency);
                dbus_message_iter_close_container(&ui, &uiv);
            dbus_message_iter_close_container(&hintiter, &ui);
        }
        if (note->category != NULL) {  // category
            DBusMessageIter ci, civ;
            static const char *catstr = "category";
            dbus_message_iter_open_container(&hintiter, DBUS_TYPE_DICT_ENTRY, NULL, &ci);
                dbus_message_iter_append_basic(&ci, DBUS_TYPE_STRING, &catstr);
                dbus_message_iter_open_container(&ci, DBUS_TYPE_VARIANT, "s", &civ);
                    dbus_message_iter_append_basic(&civ, DBUS_TYPE_STRING, &note->category);
                dbus_message_iter_close_container(&ci, &civ);
            dbus_message_iter_close_container(&hintiter, &ci);
        }
        for (size_t i = 0; i < note->nhints; i++)  // everything else
            set_note_hint(note->hints[i], &hintiter);
    dbus_message_iter_close_container(&iter, &hintiter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &timeout);
}

extern int send_note(int argc, char **argv) {
    note_t note;
    note.app_name = "notcat";
    note.app_icon = "";
    note.summary = NULL;
    note.body = NULL;
    note.urgency = -1;
    note.category = NULL;
    note.id = 0;
    note.timeout = -1;
    note.nactions = 0;
    note.nhints = 0;

    bool print_id = false, sync = false;
    char mode = '\0';
    int skip = 0;
    int i;
    for (i = 0; i < argc; i++)
top:
    {
        char *arg = argv[i];
        switch (mode) {
        // We've seen an option, like "-a" or "--app-name", and need the value.
        case 'a':
            note.app_name = arg;
            break;
        case 'i':
            if (!get_int(argv[i], &note.id) || note.id < 0 || note.id > 0xFFFFFFFF) {
                fprintf(stderr, "ID must be a valid value of uint32\n");
                return 2;
            }
            break;
        case 'I':
            note.app_icon = arg;
            break;
        case 't':
            if (!get_int(argv[i], &note.timeout) || note.timeout < -1 || note.timeout > 0x7FFFFFFF) {
                fprintf(stderr, "Timeout must be a valid int32 not less than -1\n");
                return 2;
            }
            break;
        case 'c':
            note.category = arg;
            break;
        case 'A':
            note.actions[note.nactions++] = arg;
            break;
        case 'h':
            note.hints[note.nhints++] = arg;
            break;
        case 'u':
            if (strcmp(arg, "low") == 0 || strcmp(arg, "LOW") == 0) {
                note.urgency = 0;
            } else if (strcmp(arg, "normal") == 0 || strcmp(arg, "NORMAL") == 0) {
                note.urgency = 1;
            } else if (strcmp(arg, "critical") == 0 || strcmp(arg, "CRITICAL") == 0) {
                note.urgency = 2;
            } else {
                fprintf(stderr, "Urgency must be one of 'low', 'normal', or 'critical'\n");
                return 2;
            }
            break;

        // We're ready to parse a fresh new arg.
        default: {
            if (arg[0] == '-' && !skip) {
                // Time for option parsing!
                if (arg[2] == '\0') {
                    switch (arg[1]) {
                    case '-':
                        // Stop option parsing
                        skip = 1;
                        continue;
                    case 'p':
                        // Print ID notification receives
                        print_id = true;
                        continue;
                    case 't': case 'i': case 'a': case 'A': case 'h':
                    case 'u': case 'c': case 'I':
                        // Set 'mode' to react to the proper one-character
                        // flag!
                        mode = arg[1];
                        continue;
                    default:
                        fprintf(stderr, "Unrecognized option '-%c'\n",
                                arg[1]);
                        return 2;
                    }
                }

                // longopt parsing; a bit hacky.
                for (int i = 0; longopts[i].name != NULL; i++) {
                    if (strncmp(arg, longopts[i].name, longopts[i].len) == 0) {
                        arg += longopts[i].len;
                        mode = longopts[i].mode;
                        goto top;
                    }
                }

                if (!strcmp(arg, "--print-id")) {
                    print_id = true;
                    break; /* from case */
                } else if (!strcmp(arg, "--sync")) {
                    sync = true;
                    break; /* from case */
                }

                fprintf(stderr, "Unrecognized option '%s'\n", arg);
            }

            // If it's not an option, then it's the summary, the body, or
            // an error.  Do the right thing in those cases.
            if (note.summary == NULL) {
                note.summary = arg;
            } else if (note.body == NULL) {
                note.body = arg;
            } else {
                fprintf(stderr, "Exactly one summary and one body argument expected\n");
                return 2;
            }
        }}
        mode = '\0';
    }

    if (mode != '\0') {
        fprintf(stderr, "Value required for option '-%c'\n", mode);
        return 2;
    }

    if (note.summary == NULL)
        note.summary = "";

    if (note.body == NULL)
        note.body = "";

    DBusConnection *conn = dconnect();
    if (conn == NULL)
        return 1;

    DBusMessage *msg = dcall(conn, "Notify", set_note, &note);
    if (msg == NULL)
        return 1;

    dbus_uint32_t id;
    DBusError error;
    dbus_error_init(&error);
    dbus_message_get_args(msg, &error, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Handling response from notification server: %s\n", error.message);
        return 1;
    }

    if (print_id)
        printf("%u\n", id);

    dbus_message_unref(msg);

    if (!sync)
        return 0;

    dlisten(conn, sync_send_callback, &id);
    return 0;
}

static void set_id(DBusMessage *msg, void *idp) {
    dbus_message_append_args(msg, DBUS_TYPE_UINT32, idp, DBUS_TYPE_INVALID);
}

extern int close_note(char *arg) {
    char *end;
    dbus_uint32_t id;
    long lid = strtol(arg, &end, 10);
    if (*arg == '\0' || *end != '\0')
        return 10;
    if (id < 0 || id > 65536)
        return 11;
    id = (dbus_uint32_t)lid;

    DBusMessage *msg = dcall(dconnect(), "CloseNotification", set_id, &id);
    if (msg == NULL)
        return 1;
    dbus_message_unref(msg);
    return 0;
}

// TODO: public get_capabilities() wrapping a private call_get_capabilities(char *)
extern int get_capabilities(char *test_cap) {
    DBusMessageIter oiter, iter;
    DBusMessage *msg = dcall(dconnect(), "GetCapabilities", NULL, NULL);

    if (msg == NULL)
        return 1;

    // TODO: we're making a lot of assumptions on message type that may not be
    // correct
    dbus_message_iter_init(msg, &oiter);
    dbus_message_iter_recurse(&oiter, &iter);
    while (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID) {
        DBusBasicValue cap;
        dbus_message_iter_get_basic(&iter, &cap);
        if (test_cap != NULL) {
            if (strcmp(cap.str, test_cap) == 0) {
                dbus_message_unref(msg);
                return 0;
            }
        } else
            printf("%s\n", cap.str);
        dbus_message_iter_next(&iter);
    }
    dbus_message_unref(msg);
    return (test_cap != NULL ? 1 : 0);
}

extern int get_server_information(void) {
    const char *name, *vendor, *version, *spec;
    DBusError error;
    DBusMessage *msg = dcall(dconnect(), "GetServerInformation", NULL, NULL);
    if (msg == NULL)
        return 1;

    dbus_error_init(&error);
    dbus_message_get_args(msg, &error,
            DBUS_TYPE_STRING, &name,
            DBUS_TYPE_STRING, &vendor,
            DBUS_TYPE_STRING, &version,
            DBUS_TYPE_STRING, &spec,
            DBUS_TYPE_INVALID);

    if (dbus_error_is_set(&error))
        return 1;

    printf("name: %s\nvendor: %s\nversion: %s\nspec: %s\n",
            name, vendor, version, spec);

    dbus_message_unref(msg);
    return 0;
}

static int dlisten_callback(const char *signal_name,
                            DBusMessage *msg, void *data) {
    dbus_uint32_t id;
    DBusError error;
    dbus_error_init(&error);
    if (strcmp(signal_name, "NotificationClosed") == 0) {
        dbus_uint32_t reason;
        dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &id,
                DBUS_TYPE_UINT32, &reason,
                DBUS_TYPE_INVALID);
        if (dbus_error_is_set(&error))
            fprintf(stderr, "unpacking NotificationClosed arguments: %s\n", error.message);
        else
            printf("closed %d (%s)\n", id,
                    (reason == 1 ? "expired"
                     : reason == 2 ? "dismissed"
                     : reason == 3 ? "closed"
                     : "unknown reason"));
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (strcmp(signal_name, "ActionInvoked") == 0) {
        const char *key;
        dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &id,
                DBUS_TYPE_STRING, &key,
                DBUS_TYPE_INVALID);
        if (dbus_error_is_set(&error))
            fprintf(stderr, "unpacking NotificationClosed arguments: %s\n", error.message);
        else
            printf("invoked %d %s\n", id, key);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void set_action(DBusMessage *msg, void *dp) {
    struct { dbus_uint32_t id; const char *key; } *d = dp;
    dbus_message_append_args(msg,
            DBUS_TYPE_UINT32, &(d->id),
            DBUS_TYPE_STRING, &(d->key),
            DBUS_TYPE_INVALID);
}

extern int invoke_action(int argc, char **argv) {
    char *end, *idarg = argv[0];
    struct { dbus_uint32_t id; const char *key; } d;
    DBusMessage *msg;
    long id = strtol(idarg, &end, 10);
    if (*idarg == '\0' || *end != '\0')
        return 10;
    if (id < 0 || id > 65536)
        return 11;

    if (get_capabilities("x-notlib-remote-actions") != 0) {
        fprintf(stderr, "Notification server does not support remote actions\n");
        return 1;
    }
    d.id  = (dbus_uint32_t)id;
    d.key = (argc > 1 ? argv[1] : "default");
    if ((msg = dcall(dconnect(), "InvokeAction", set_action, &d)) == NULL)
        return 1;
    dbus_message_unref(msg);
    return 0;
}

extern int listen_for_signals(void) {
    dlisten(dconnect(), dlisten_callback, NULL);
    return 0;
}
