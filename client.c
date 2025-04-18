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
#include <stdio.h>
#include <gio/gio.h>

#include "notcat.h"

static GDBusConnection *connect(void) {
    GDBusConnection *conn;
    GError *error = NULL;

    conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    if (error != NULL) {
        fprintf(stderr, "DBus connection error: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }
    return conn;
}

static GDBusProxy *make_proxy(GDBusConnection *conn) {
    GDBusProxy *proxy;
    GError *error = NULL;

    if (conn == NULL)
        return NULL;

    proxy = g_dbus_proxy_new_sync(conn,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.Notifications",
                                  "/org/freedesktop/Notifications",
                                  "org.freedesktop.Notifications",
                                  NULL,
                                  &error);

    if (error != NULL) {
        fprintf(stderr, "DBus proxy creation error: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }
    return proxy;
}

static GVariant *call(GDBusProxy *proxy, char *name, GVariant *args) {
    GVariant *result;
    GError *error = NULL;

    if (proxy == NULL)
        return NULL;

    result = g_dbus_proxy_call_sync(proxy,
            name,
            args,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error);
    if (error != NULL) {
        fprintf(stderr, "DBus call error (%s): %s\n", name, error->message);
        g_error_free(error);
        return NULL;
    }
    return result;
}

typedef int (*signal_callback_type)(const gchar *signal_name,
                                    GVariant *parameters, void *data);

typedef struct {
    guint sub_id;
    void *data;
    GMainLoop *loop;
    signal_callback_type callback;
} signal_callback_user_data_type;

static void signal_callback(GDBusConnection *conn,
                            const gchar *sender_name,
                            const gchar *object_path,
                            const gchar *iface_name,
                            const gchar *signal_name,
                            GVariant *parameters,
                            gpointer user_data) {
    signal_callback_user_data_type *d
        = (signal_callback_user_data_type *) user_data;
    if (d->callback(signal_name, parameters, d->data)) {
        g_dbus_connection_signal_unsubscribe(conn, d->sub_id);
        g_main_loop_quit(d->loop);
    }
}

static int listen(GDBusConnection *conn, signal_callback_type cb, void *data) {
    GMainLoop *loop;

    if (conn == NULL)
        return 1;

    loop = g_main_loop_new(NULL, FALSE);

    signal_callback_user_data_type ud;
    ud.callback = cb;
    ud.loop = loop;
    ud.data = data;
    ud.sub_id = g_dbus_connection_signal_subscribe(conn,
                        NULL,
                        "org.freedesktop.Notifications",
                        NULL,
                        "/org/freedesktop/Notifications",
                        NULL,
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        signal_callback,
                        (gpointer)&ud,
                        NULL);
    g_main_loop_run(loop);
    return 0;
}

static int get_int(char *str, long *out) {
    char *end;
    *out = strtol(str, &end, 10);
    return (str != NULL && *str != '\0' && *end == '\0');
}

static int sync_send_callback(const gchar *signal_name,
                              GVariant *parameters, void *data) {
    guint32 id = *(guint32 *)data;
    GVariant *iv = g_variant_get_child_value(parameters, 0);
    if (id != g_variant_get_uint32(iv)) {
        g_variant_unref(iv);
        return 0;
    }
    g_variant_unref(iv);

    if (!strcmp(signal_name, "NotificationClosed"))
        return 1;

    if (!strcmp(signal_name, "ActionInvoked")) {
        GVariant *av = g_variant_get_child_value(parameters, 1);
        printf("%s\n", g_variant_get_string(av, NULL));
        g_variant_unref(av);
    }
    return 0;
}

extern int send_note(int argc, char **argv) {
    char *app_name = "notcat";
    char *app_icon = "";
    char *summary = NULL;
    char *body = NULL;
    long id = 0;
    long timeout = -1;
    GVariantBuilder *actions = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GVariantBuilder *hints = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    char print_id = 0;
    char sync = 0;

    char mode = '\0';
    int skip = 0;
    int i;
    for (i = 0; i < argc; i++) {
        char *arg = argv[i];
        switch (mode) {
        // We've seen a single-letter option, like "-a".
        // Use the current argument to get the value for that field.
        case 'a':
get_appname:
            app_name = arg;
            break;
        case 'i':
get_id:
            if (!get_int(argv[i], &id) || id < 0 || id > 0xFFFFFFFF) {
                fprintf(stderr, "ID must be a valid value of uint32\n");
                return 2;
            }
            break;
        case 'I':
get_icon:
            app_icon = arg;
            break;
        case 't':
get_timeout:
            if (!get_int(argv[i], &timeout) || timeout < -1 || timeout > 0x7FFFFFFF) {
                fprintf(stderr, "Timeout must be a valid int32 not less "
                        "than -1\n");
                return 2;
            }
            break;
        case 'c':
get_category:
            g_variant_builder_add(hints, "{sv}", "category",
                    g_variant_new_string(arg));
            break;
        case 'A':
get_actions: {
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
                    g_variant_builder_add(actions, "s", arg);
                    g_variant_builder_add(actions, "s", c2 + 1);
                    *c2 = ':';
                    break;
                }
                if (!*c2) {
                    g_variant_builder_add(actions, "s", arg);
                    g_variant_builder_add(actions, "s", arg);
                }

                arg = c + reset;
                if (reset) *c = ',';
                c += reset;
            }
            break;
        }
        case 'h':
get_hint: {
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
        }
        case 'u':
get_urgency:
            if (!strcmp(arg, "low") || !strcmp(arg, "LOW")) {
                g_variant_builder_add(hints, "{sv}", "urgency",
                        g_variant_new_byte(0));
            } else if (!strcmp(arg, "normal") || !strcmp(arg, "NORMAL")) {
                g_variant_builder_add(hints, "{sv}", "urgency",
                        g_variant_new_byte(1));
            } else if (!strcmp(arg, "critical") || !strcmp(arg, "CRITICAL")) {
                g_variant_builder_add(hints, "{sv}", "urgency",
                        g_variant_new_byte(2));
            } else {
                fprintf(stderr, "Urgency must be one of 'low', 'normal', or 'critical'\n");
                return 2;
            }
            break;

        // We're in a normal state -- the last arg wasn't anything
        // interesting.
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
                        print_id = 1;
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

                // longopt processing.  This is some spaghetti nonsense,
                // but it deduplicates the actual per-option parsing
                // itself, so we count it as a win.
                if (!strncmp(arg, "--app-name=", 11)) {
                    arg += 11;
                    goto get_appname;
                } else if (!strncmp(arg, "--timeout=", 10)) {
                    arg += 10;
                    goto get_timeout;
                } else if (!strncmp(arg, "--id=", 5)) {
                    arg += 5;
                    goto get_id;
                } else if (!strncmp(arg, "--actions=", 10)) {
                    arg += 10;
                    goto get_actions;
                } else if (!strncmp(arg, "--hints=", 8)) {
                    arg += 8;
                    goto get_hint;
                } else if (!strncmp(arg, "--urgency=", 10)) {
                    arg += 10;
                    goto get_urgency;
                } else if (!strncmp(arg, "--category=", 11)) {
                    arg += 11;
                    goto get_category;
                } else if (!strncmp(arg, "--icon=", 7)) {
                    arg += 7;
                    goto get_icon;
                } else if (!strcmp(arg, "--print-id")) {
                    print_id = 1;
                    break;
                } else if (!strcmp(arg, "--sync")) {
                    sync = 1;
                    break;
                }

                fprintf(stderr, "Unrecognized option '%s'\n", arg);
            }

            // If it's not an option, then it's the summary, the body, or
            // an error.  Do the right thing in those cases.
            if (summary == NULL) {
                summary = arg;
            } else if (body == NULL) {
                body = arg;
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

    if (summary == NULL)
        summary = "";

    if (body == NULL)
        body = "";

    GDBusConnection *conn = connect();
    GDBusProxy *proxy = make_proxy(conn);
    if (proxy == NULL)
        return 1;
    GVariant *result = call(proxy, "Notify",
            g_variant_new("(susssasa{sv}i)",
                app_name,
                (u_int32_t) id,
                app_icon,
                summary,
                body,
                actions,
                hints,
                (int32_t) timeout));

    if (result == NULL)
        return 1;

    id = g_variant_get_uint32(g_variant_get_child_value(result, 0));

    if (print_id)
        printf("%ld\n", id);

    if (!sync)
        return 0;

    return listen(conn, sync_send_callback, &id);
}

extern int close_note(char *arg) {
    char *end;
    long id = strtol(arg, &end, 10);
    if (*arg == '\0' || *end != '\0')
        return 10;
    if (id < 0 || id > 65536)
        return 11;

    GDBusProxy *proxy = make_proxy(connect());
    GVariant *result = call(proxy, "CloseNotification",
                            g_variant_new("(u)", id));

    return (result == NULL);
}

extern int get_capabilities(void) {
    GDBusProxy *proxy = make_proxy(connect());
    GVariant *result = call(proxy, "GetCapabilities", NULL);

    if (result == NULL)
        return 1;

    GVariant *inner = g_variant_get_child_value(result, 0);

    size_t len;
    const char **arr = g_variant_get_strv(inner, &len);
    for (size_t i = 0; i < len; i++)
        printf("%s\n", arr[i]);

    return 0;
}

extern int get_server_information(void) {
    GDBusProxy *proxy = make_proxy(connect());
    GVariant *result = call(proxy, "GetServerInformation", NULL);

    if (result == NULL)
        return 1;

    printf("name: %s\n", g_variant_get_string(
                g_variant_get_child_value(result, 0), NULL));
    printf("vendor: %s\n", g_variant_get_string(
                g_variant_get_child_value(result, 1), NULL));
    printf("version: %s\n", g_variant_get_string(
                g_variant_get_child_value(result, 2), NULL));
    printf("spec: %s\n", g_variant_get_string(
                g_variant_get_child_value(result, 3), NULL));

    return 0;
}

static int listen_callback(const gchar *signal_name,
                           GVariant *parameters, void *data) {
    if (!strcmp(signal_name, "NotificationClosed")) {
        GVariant *iv = g_variant_get_child_value(parameters, 0);
        GVariant *rv = g_variant_get_child_value(parameters, 1);
        guint32 r = g_variant_get_uint32(rv);
        printf("closed %d (%s)\n",
                g_variant_get_uint32(iv),
                (r == 1 ? "expired"
                 : r == 2 ? "dismissed"
                 : r == 3 ? "closed"
                 : "unknown reason"));
        g_variant_unref(iv);
        g_variant_unref(rv);
    } else if (!strcmp(signal_name, "ActionInvoked")) {
        GVariant *iv = g_variant_get_child_value(parameters, 0);
        GVariant *av = g_variant_get_child_value(parameters, 1);
        printf("invoked %d %s\n",
                g_variant_get_uint32(iv),
                g_variant_get_string(av, NULL));
        g_variant_unref(iv);
        g_variant_unref(av);
    } else {
        printf("unknown signal %s emitted\n", signal_name);
    }
    return 0;
}

extern int listen_for_signals(void) {
    return listen(connect(), listen_callback, NULL);
}

extern int invoke_action(int argc, char **argv) {
    char *end;
    char *idarg = argv[0];
    long id = strtol(idarg, &end, 10);
    if (*idarg == '\0' || *end != '\0')
        return 10;
    if (id < 0 || id > 65536)
        return 11;

    char *key = (argc > 1 ? argv[1] : "default");

    GDBusProxy *proxy = make_proxy(connect());
    GVariant *result = call(proxy, "GetCapabilities", NULL);
    if (result == NULL)
        return 1;
    result = g_variant_get_child_value(result, 0);

    int got_cap = 0;
    size_t i, len;
    const gchar **cs = g_variant_get_strv(result, &len);
    for (i = 0; i < len; i++) {
        if (!strcmp(cs[i], "x-notlib-remote-actions")) {
            got_cap = 1;
            break;
        }
    }
    if (cs) g_free(cs);
    g_variant_unref(result);

    if (!got_cap) {
        fprintf(stderr, "Notification server does not support remote "
                        "actions\n");
        return 1;
    }

    result = call(proxy, "InvokeAction", g_variant_new("(us)", id, key));
    return (result == NULL);
}
