/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <fcntl.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"


void
create_loop_and_launch(GMainLoop **loop)
{
    *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(*loop);
    g_main_loop_unref(*loop);
    *loop = NULL;
}

void
shutdown_loop(GMainLoop *loop)
{
    if (loop && g_main_loop_is_running(loop))
        g_main_loop_quit(loop);
}

GQuark
virt_viewer_error_quark(void)
{
  return g_quark_from_static_string ("virt-viewer-error-quark");
}

GtkBuilder *remote_viewer_util_load_ui(const char *name)
{
    GtkBuilder *builder;
    gchar *resource = g_strdup_printf("%s/ui/%s",
                                      VIRT_VIEWER_RESOURCE_PREFIX,
                                      name);

    builder = gtk_builder_new_from_resource(resource);

    g_free(resource);
    return builder;
}
//  spice://192.168.11.110:5908
int
virt_viewer_util_extract_host(const char *uristr,
                              char **scheme,
                              char **host,
                              char **transport,
                              char **user,
                              int *port)
{
    xmlURIPtr uri;
    char *offset = NULL;

    if (uristr == NULL ||
        !g_ascii_strcasecmp(uristr, "xen"))
        uristr = "xen:///";

    uri = xmlParseURI(uristr);
    g_return_val_if_fail(uri != NULL, 1);

    if (host) {
        if (!uri->server) {
            *host = g_strdup("localhost");
        } else {
            if (uri->server[0] == '[') {
                gchar *tmp;
                *host = g_strdup(uri->server + 1);
                if ((tmp = strchr(*host, ']')))
                    *tmp = '\0';
            } else {
                *host = g_strdup(uri->server);
            }
        }
    }

    if (user) {
        if (uri->user)
            *user = g_strdup(uri->user);
        else
            *user = NULL;
    }

    if (port)
        *port = uri->port;

    if (uri->scheme)
        offset = strchr(uri->scheme, '+');

    if (transport) {
        if (offset)
            *transport = g_strdup(offset + 1);
        else
            *transport = NULL;
    }

    if (scheme && uri->scheme) {
        if (offset)
            *scheme = g_strndup(uri->scheme, offset - uri->scheme);
        else
            *scheme = g_strdup(uri->scheme);
    }

    xmlFreeURI(uri);
    return 0;
}

typedef struct {
    GObject *instance;
    GObject *observer;
    GClosure *closure;
    gulong handler_id;
} WeakHandlerCtx;

static WeakHandlerCtx *
whc_new(GObject *instance,
        GObject *observer)
{
    WeakHandlerCtx *ctx = g_new0(WeakHandlerCtx, 1);

    ctx->instance = instance;
    ctx->observer = observer;

    return ctx;
}

static void
whc_free(WeakHandlerCtx *ctx)
{
    g_free(ctx);
}

static void observer_destroyed_cb(gpointer, GObject *);
static void closure_invalidated_cb(gpointer, GClosure *);

/*
 * If signal handlers are removed before the object is destroyed, this
 * callback will never get triggered.
 */
static void
instance_destroyed_cb(gpointer ctx_,
                      GObject *where_the_instance_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    /* No need to disconnect the signal here, the instance has gone away. */
    g_object_weak_unref(ctx->observer, observer_destroyed_cb, ctx);
    g_closure_remove_invalidate_notifier(ctx->closure, ctx,
                                         closure_invalidated_cb);
    whc_free(ctx);
}

/* Triggered when the observer is destroyed. */
static void
observer_destroyed_cb(gpointer ctx_,
                      GObject *where_the_observer_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    g_closure_remove_invalidate_notifier(ctx->closure, ctx,
                                         closure_invalidated_cb);
    g_signal_handler_disconnect(ctx->instance, ctx->handler_id);
    g_object_weak_unref(ctx->instance, instance_destroyed_cb, ctx);
    whc_free(ctx);
}

/* Triggered when either object is destroyed or the handler is disconnected. */
static void
closure_invalidated_cb(gpointer ctx_,
                       GClosure *where_the_closure_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    g_object_weak_unref(ctx->instance, instance_destroyed_cb, ctx);
    g_object_weak_unref(ctx->observer, observer_destroyed_cb, ctx);
    whc_free(ctx);
}

/* Copied from tp_g_signal_connect_object. */
/**
  * virt_viewer_signal_connect_object: (skip)
  * @instance: the instance to connect to.
  * @detailed_signal: a string of the form "signal-name::detail".
  * @c_handler: the #GCallback to connect.
  * @gobject: the object to pass as data to @c_handler.
  * @connect_flags: a combination of #GConnectFlags.
  *
  * Similar to g_signal_connect_object() but will delete connection
  * when any of the objects is destroyed.
  *
  * Returns: the handler id.
  */
gulong virt_viewer_signal_connect_object(gpointer instance,
                                         const gchar *detailed_signal,
                                         GCallback c_handler,
                                         gpointer gobject,
                                         GConnectFlags connect_flags)
{
    GObject *instance_obj = G_OBJECT(instance);
    WeakHandlerCtx *ctx = whc_new(instance_obj, gobject);

    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE (instance), 0);
    g_return_val_if_fail(detailed_signal != NULL, 0);
    g_return_val_if_fail(c_handler != NULL, 0);
    g_return_val_if_fail(G_IS_OBJECT (gobject), 0);
    g_return_val_if_fail((connect_flags & ~(G_CONNECT_AFTER|G_CONNECT_SWAPPED)) == 0, 0);

    if (connect_flags & G_CONNECT_SWAPPED)
        ctx->closure = g_cclosure_new_object_swap(c_handler, gobject);
    else
        ctx->closure = g_cclosure_new_object(c_handler, gobject);

    ctx->handler_id = g_signal_connect_closure(instance, detailed_signal,
                                               ctx->closure, (connect_flags & G_CONNECT_AFTER) ? TRUE : FALSE);

    g_object_weak_ref(instance_obj, instance_destroyed_cb, ctx);
    g_object_weak_ref(gobject, observer_destroyed_cb, ctx);
    g_closure_add_invalidate_notifier(ctx->closure, ctx,
                                      closure_invalidated_cb);

    return ctx->handler_id;
}

static const gchar *log_level_to_str(GLogLevelFlags log_level)
{
    switch (log_level) {
        case G_LOG_FLAG_FATAL:
            return "FATAL";
        case G_LOG_LEVEL_ERROR:
            return "ERROR";
        case G_LOG_LEVEL_CRITICAL:
            return "CRITICAL";
        case G_LOG_LEVEL_WARNING:
            return "W";
        case G_LOG_LEVEL_MESSAGE:
            return "M";
        case G_LOG_LEVEL_INFO:
            return "I";
        case G_LOG_LEVEL_DEBUG:
            return "D";
        case G_LOG_FLAG_RECURSION:
        case G_LOG_LEVEL_MASK:
        default:
            return "";
    }
}


static void log_handler(const gchar *log_domain G_GNUC_UNUSED,
                        GLogLevelFlags log_level,
                        const gchar *message,
                        gpointer unused_data G_GNUC_UNUSED)
{
    GDateTime *datetime = g_date_time_new_now_local();
    gchar *data_time_string = g_date_time_format(datetime, "%T");
    g_date_time_unref(datetime);

    printf("%s %s: %s\n", log_level_to_str(log_level), data_time_string, message);
    g_free(data_time_string);
}

#ifdef G_OS_WIN32
static BOOL is_handle_valid(HANDLE h)
{
    if (h == INVALID_HANDLE_VALUE || h == NULL)
        return FALSE;

    DWORD flags;
    return GetHandleInformation(h, &flags);
}
#endif

void virt_viewer_util_init(const char *appname)
{
#ifdef G_OS_WIN32
    /*
     * This named mutex will be kept around by Windows until the
     * process terminates. This allows other instances to check if it
     * already exists, indicating already running instances. It is
     * used to warn the user that installer can't proceed in this
     * case.
     */
    CreateMutexA(0, 0, "VirtViewerMutex");

    /* Get redirection from parent */
    BOOL out_valid = is_handle_valid(GetStdHandle(STD_OUTPUT_HANDLE));
    BOOL err_valid = is_handle_valid(GetStdHandle(STD_ERROR_HANDLE));

    /*
     * If not all output are redirected try to redirect to parent console.
     * If parent has no console (for instance as launched from GUI) just
     * rely on default (no output).
     */
    if ((!out_valid || !err_valid) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!out_valid) {
            freopen("CONOUT$", "w", stdout);
            dup2(fileno(stdout), STDOUT_FILENO);
        }
        if (!err_valid) {
            freopen("CONOUT$", "w", stderr);
            dup2(fileno(stderr), STDERR_FILENO);
        }
    }
#endif

    setlocale(LC_ALL, "");
    //setlocale(LC_ALL, "Russian");
    bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_set_application_name(appname);

    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, log_handler, NULL);
}

static gchar *
ctrl_key_to_gtk_key(const gchar *key)
{
    guint i;

    static const struct {
        const char *ctrl;
        const char *gtk;
    } keys[] = {
        { "alt", "<Alt>" },
        { "ralt", "<Alt>" },
        { "rightalt", "<Alt>" },
        { "right-alt", "<Alt>" },
        { "lalt", "<Alt>" },
        { "leftalt", "<Alt>" },
        { "left-alt", "<Alt>" },

        { "ctrl", "<Ctrl>" },
        { "rctrl", "<Ctrl>" },
        { "rightctrl", "<Ctrl>" },
        { "right-ctrl", "<Ctrl>" },
        { "lctrl", "<Ctrl>" },
        { "leftctrl", "<Ctrl>" },
        { "left-ctrl", "<Ctrl>" },

        { "shift", "<Shift>" },
        { "rshift", "<Shift>" },
        { "rightshift", "<Shift>" },
        { "right-shift", "<Shift>" },
        { "lshift", "<Shift>" },
        { "leftshift", "<Shift>" },
        { "left-shift", "<Shift>" },

        { "cmd", "<Ctrl>" },
        { "rcmd", "<Ctrl>" },
        { "rightcmd", "<Ctrl>" },
        { "right-cmd", "<Ctrl>" },
        { "lcmd", "<Ctrl>" },
        { "leftcmd", "<Ctrl>" },
        { "left-cmd", "<Ctrl>" },

        { "win", "<Super>" },
        { "rwin", "<Super>" },
        { "rightwin", "<Super>" },
        { "right-win", "<Super>" },
        { "lwin", "<Super>" },
        { "leftwin", "<Super>" },
        { "left-win", "<Super>" },

        { "esc", "Escape" },
        /* { "escape", "Escape" }, */

        { "ins", "Insert" },
        /* { "insert", "Insert" }, */

        { "del", "Delete" },
        /* { "delete", "Delete" }, */

        { "pgup", "Page_Up" },
        { "pageup", "Page_Up" },
        { "pgdn", "Page_Down" },
        { "pagedown", "Page_Down" },

        /* { "home", "home" }, */
        { "end", "End" },
        /* { "space", "space" }, */

        { "enter", "Return" },

        /* { "tab", "tab" }, */
        /* { "f1", "F1" }, */
        /* { "f2", "F2" }, */
        /* { "f3", "F3" }, */
        /* { "f4", "F4" }, */
        /* { "f5", "F5" }, */
        /* { "f6", "F6" }, */
        /* { "f7", "F7" }, */
        /* { "f8", "F8" }, */
        /* { "f9", "F9" }, */
        /* { "f10", "F10" }, */
        /* { "f11", "F11" }, */
        /* { "f12", "F12" } */
    };

    for (i = 0; i < G_N_ELEMENTS(keys); ++i) {
        if (g_ascii_strcasecmp(keys[i].ctrl, key) == 0)
            return g_strdup(keys[i].gtk);
    }

    return g_ascii_strup(key, -1);
}

gchar*
spice_hotkey_to_gtk_accelerator(const gchar *key)
{
    gchar *accel, **k, **keyv;

    keyv = g_strsplit(key, "+", -1);
    g_return_val_if_fail(keyv != NULL, NULL);

    for (k = keyv; *k != NULL; k++) {
        gchar *tmp = *k;
        *k = ctrl_key_to_gtk_key(tmp);
        g_free(tmp);
    }

    accel = g_strjoinv(NULL, keyv);
    g_strfreev(keyv);

    return accel;
}

static gboolean str_is_empty(const gchar *str)
{
  return ((str == NULL) || (str[0] == '\0'));
}

// Вернет больше нуля, если первая больша. Меньше нуля, если первая меньше. 0 - если равны
gint
virt_viewer_compare_version(const gchar *s1, const gchar *s2)
{
    gint i, retval = 0;
    gchar **v1, **v2;

    if (str_is_empty(s1) && str_is_empty(s2)) {
      return 0;
    } else if (str_is_empty(s1)) {
      return -1;
    } else if (str_is_empty(s2)) {
      return 1;
    }

    v1 = g_strsplit(s1, ".", -1);
    v2 = g_strsplit(s2, ".", -1);

    for (i = 0; v1[i] && v2[i]; ++i) {
        gchar *e1 = NULL, *e2 = NULL;
        guint64 m1 = g_ascii_strtoull(v1[i], &e1, 10);
        guint64 m2 = g_ascii_strtoull(v2[i], &e2, 10);

        retval = m1 - m2;
        if (retval != 0)
            goto end;

        g_return_val_if_fail(e1 && e2, 0);
        if (*e1 || *e2) {
            g_warning("the version string contains a suffix");
            goto end;
        }
    }

    if (v1[i])
        retval = 1;
    else if (v2[i])
        retval = -1;

end:
    g_strfreev(v1);
    g_strfreev(v2);
    return retval;
}

/**
 * virt_viewer_compare_buildid:
 * @s1: a version-like string
 * @s2: a version-like string
 *
 * Compare two buildid strings: 1.1-1 > 1.0-1, 1.0-2 > 1.0-1, 1.10 > 1.7...
 *
 * String suffix (1.0rc1 etc) are not accepted, and will return 0.
 *
 * Returns: negative value if s1 < s2; zero if s1 = s2; positive value if s1 > s2.
 **/
gint
virt_viewer_compare_buildid(const gchar *s1, const gchar *s2)
{
    int ret = 0;
    GStrv split1 = NULL;
    GStrv split2 = NULL;

    split1 = g_strsplit(s1, "-", 2);
    split2 = g_strsplit(s2, "-", 2);
    if ((split1 == NULL) || (split2 == NULL)) {
      goto end;
    }
    /* Compare versions */
    ret = virt_viewer_compare_version(split1[0], split2[0]);
    if (ret != 0) {
        goto end;
    }
    if ((split1[0] == NULL) || (split2[0] == NULL)) {
      goto end;
    }

    /* Compare -release */
    ret = virt_viewer_compare_version(split1[1], split2[1]);

end:
    g_strfreev(split1);
    g_strfreev(split2);

    return ret;
}

/* simple sorting of monitors. Primary sort left-to-right, secondary sort from
 * top-to-bottom, finally by monitor id */
static int
displays_cmp(const void *p1, const void *p2, gpointer user_data)
{
    guint diff;
    GHashTable *displays = user_data;
    guint i = *(guint*)p1;
    guint j = *(guint*)p2;
    GdkRectangle *m1 = g_hash_table_lookup(displays, GINT_TO_POINTER(i));
    GdkRectangle *m2 = g_hash_table_lookup(displays, GINT_TO_POINTER(j));
    g_return_val_if_fail(m1 != NULL && m2 != NULL, 0);
    diff = m1->x - m2->x;
    if (diff == 0)
        diff = m1->y - m2->y;
    if (diff == 0)
        diff = i - j;

    return diff;
}

static void find_max_id(gpointer key,
                        gpointer value G_GNUC_UNUSED,
                        gpointer user_data)
{
    guint *max_id = user_data;
    guint id = GPOINTER_TO_INT(key);
    *max_id = MAX(*max_id, id);
}

void
virt_viewer_align_monitors_linear(GHashTable *displays)
{
    guint i;
    gint x = 0;
    guint *sorted_displays;
    guint max_id = 0;
    guint ndisplays = 0;
    GHashTableIter iter;
    gpointer key;

    g_return_if_fail(displays != NULL);

    if (g_hash_table_size(displays) == 0)
        return;

    g_hash_table_foreach(displays, find_max_id, &max_id);
    ndisplays = max_id + 1;

    sorted_displays = g_new0(guint, ndisplays);

    g_hash_table_iter_init(&iter, displays);
    while (g_hash_table_iter_next(&iter, &key, NULL))
        sorted_displays[GPOINTER_TO_INT(key)] = GPOINTER_TO_INT(key);

    g_qsort_with_data(sorted_displays, ndisplays, sizeof(guint), displays_cmp, displays);

    /* adjust monitor positions so that there's no gaps or overlap between
     * monitors */
    for (i = 0; i < ndisplays; i++) {
        guint nth = sorted_displays[i];
        g_assert(nth < ndisplays);
        GdkRectangle *rect = g_hash_table_lookup(displays, GINT_TO_POINTER(nth));
        g_return_if_fail(rect != NULL);
        rect->x = x;
        rect->y = 0;
        x += rect->width;
    }
    g_free(sorted_displays);
}

/* Shift all displays so that the monitor origin is at (0,0). This reduces the
 * size of the screen that will be required on the guest when all client
 * monitors are fullscreen but do not begin at the origin. For example, instead
 * of sending down the following configuration:
 *   1280x1024+4240+0
 * (which implies that the guest screen must be at least 5520x1024), we'd send
 *   1280x1024+0+0
 * (which implies the guest screen only needs to be 1280x1024). The first
 * version might fail if the guest video memory is not large enough to handle a
 * screen of that size.
 */
void
virt_viewer_shift_monitors_to_origin(GHashTable *displays)
{
    gint xmin = G_MAXINT;
    gint ymin = G_MAXINT;
    GHashTableIter iter;
    gpointer value;

    if (g_hash_table_size(displays) == 0)
        return;

    g_hash_table_iter_init(&iter, displays);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        GdkRectangle *display = value;
        g_return_if_fail(display != NULL);
        if (display->width > 0 && display->height > 0) {
            xmin = MIN(xmin, display->x);
            ymin = MIN(ymin, display->y);
        }
    }
    g_return_if_fail(xmin < G_MAXINT && ymin < G_MAXINT);

    if (xmin > 0 || ymin > 0) {
        g_debug("%s: Shifting all monitors by (%i, %i)", G_STRFUNC, xmin, ymin);
        g_hash_table_iter_init(&iter, displays);
        while (g_hash_table_iter_next(&iter, NULL, &value)) {
            GdkRectangle *display = value;
            if (display->width > 0 && display->height > 0) {
                display->x -= xmin;
                display->y -= ymin;
            }
        }
    }
}

/**
 * virt_viewer_parse_monitor_mappings:
 * @mappings: (array zero-terminated=1) values for the "monitor-mapping" key
 * @nmappings: the size of @mappings
 * @nmonitors: the count of client's monitors
 *
 * Parses and validates monitor mappings values to return a hash table
 * containing the mapping from guest display ids to client monitors ids.
 *
 * Returns: (transfer full) a #GHashTable containing mapping from guest display
 *  ids to client monitor ids or %NULL if the mapping is invalid.
 */
GHashTable*
virt_viewer_parse_monitor_mappings(gchar **mappings, const gsize nmappings, const gint nmonitors)
{
    GHashTable *displaymap;
    GHashTable *monitormap;
    guint i, max_display_id = 0;
    gchar **tokens = NULL;

    g_return_val_if_fail(nmonitors != 0, NULL);
    displaymap = g_hash_table_new(g_direct_hash, g_direct_equal);
    monitormap = g_hash_table_new(g_direct_hash, g_direct_equal);
    if (nmappings == 0) {
        g_warning("Empty monitor-mapping configuration");
        goto configerror;
    }

    for (i = 0; i < nmappings; i++) {
        gchar *endptr = NULL;
        gint display = 0, monitor = 0;

        tokens = g_strsplit(mappings[i], ":", 2);
        if (g_strv_length(tokens) != 2) {
            g_warning("Invalid monitor-mapping configuration: '%s'. "
                      "Expected format is '<DISPLAY-ID>:<MONITOR-ID>'",
                      mappings[i]);
            g_strfreev(tokens);
            goto configerror;
        }

        display = strtol(tokens[0], &endptr, 10);
        if ((endptr && *endptr != '\0') || display < 0) {
            g_warning("Invalid monitor-mapping configuration: display id is invalid: %s %p='%s'", tokens[0], endptr, endptr);
            g_strfreev(tokens);
            goto configerror;
        }
        monitor = strtol(tokens[1], &endptr, 10);
        if ((endptr && *endptr != '\0') || monitor < 0) {
            g_warning("Invalid monitor-mapping configuration: monitor id '%s' is invalid", tokens[1]);
            g_strfreev(tokens);
            goto configerror;
        }
        g_strfreev(tokens);

        if (monitor >= nmonitors) {
            g_warning("Invalid monitor-mapping configuration: monitor #%i for display #%i does not exist", monitor, display);
            goto configerror;
        }

        if (g_hash_table_lookup_extended(displaymap, GINT_TO_POINTER(display), NULL, NULL) ||
            g_hash_table_lookup_extended(monitormap, GINT_TO_POINTER(monitor), NULL, NULL)) {
            g_warning("Invalid monitor-mapping configuration: a display or monitor id was specified twice");
            goto configerror;
        }
        g_debug("Fullscreen config: mapping guest display %i to monitor %i", display, monitor);
        g_hash_table_insert(displaymap, GINT_TO_POINTER(display), GINT_TO_POINTER(monitor));
        g_hash_table_insert(monitormap, GINT_TO_POINTER(monitor), GINT_TO_POINTER(display));
        max_display_id = MAX((guint) display, max_display_id);
    }

    for (i = 0; i < max_display_id; i++) {
        if (!g_hash_table_lookup_extended(displaymap, GINT_TO_POINTER(i), NULL, NULL)) {
            g_warning("Invalid monitor-mapping configuration: display #%d was not specified", i);
            goto configerror;
        }
    }

    g_hash_table_unref(monitormap);
    return displaymap;

configerror:
    g_hash_table_unref(monitormap);
    g_hash_table_unref(displaymap);
    return NULL;
}
// todo: rename to free_string_safely
void free_memory_safely(gchar **string_ptr)
{
    if (string_ptr && *string_ptr) {
        g_free(*string_ptr);
        *string_ptr = NULL;
    }
}

size_t strlen_safely(const gchar *str)
{
    if (str)
        return strlen(str);
    else
        return 0;
}

void update_string_safely(gchar **string_ptr, const gchar *new_string)
{
    free_memory_safely(string_ptr);
    if (string_ptr) {
        if (new_string)
            *string_ptr = g_strdup(new_string);
        else
            *string_ptr = NULL;
    }
}

gchar *strstrip_safely(gchar *str)
{
    if (str == NULL)
        return str;

    return g_strstrip(str);
}

void g_source_remove_safely(guint *timeout_id)
{
    if (*timeout_id) {
        g_source_remove(*timeout_id);
        *timeout_id = 0;
    }
}

const gchar* determine_http_protocol_by_port(int port)
{
    const int https_port = 443;
    return ((port == https_port) ? "https" : "http");
}

void set_client_spice_cursor_visible(gboolean is_visible)
{
    const gchar *spice_cursor_env_var = "SPICE_DEBUG_CURSOR";
    if (is_visible) {
        gboolean is_var_set = g_setenv(spice_cursor_env_var, "1", TRUE);
        g_info("%s is_var_set: %i", (const char *)__func__, is_var_set);
    } else {
        g_unsetenv(spice_cursor_env_var);
    }
}

GtkWidget *get_widget_from_builder(GtkBuilder *builder, const gchar *name)
{
    return GTK_WIDGET(gtk_builder_get_object(builder, name));
}

gchar *get_log_dir_path()
{
    // log dir dipends on OS
#ifdef G_OS_UNIX
    gchar *log_dir = g_strdup("log");
#elif _WIN32
    const gchar *locap_app_data_path = g_getenv("LOCALAPPDATA");
    gchar *log_dir = g_strdup_printf("%s\\%s\\log", locap_app_data_path, APP_FILES_DIRECTORY_NAME);
#endif
    return log_dir;
}

gchar* replace_str(const gchar *src, const gchar *find, const gchar *replace)
{
    gchar* retval = g_strdup(src);

    if (retval && find) {

        gchar *ptr = NULL;
        ptr = g_strstr_len(retval, -1, find);
        if (ptr != NULL) {
            gchar *after_find = replace_str(ptr + strlen(find), find, replace);
            gchar *before_find = g_strndup(retval, ptr - retval);
            gchar *temp = g_strconcat(before_find, replace, after_find, NULL);
            g_free(retval);
            retval = g_strdup(temp);
            g_free(before_find);
            g_free(temp);
        }
    }

    return retval;
}

gchar *util_remove_all_spaces(const gchar *src)
{
    return replace_str(src, " ", "");
}

void convert_string_from_utf8_to_locale(gchar **utf8_str)
{
    if (!utf8_str || !(*utf8_str))
        return;

    gchar *local_str = g_locale_from_utf8(*utf8_str,
                                          -1,
                                          NULL,
                                          NULL,
                                          NULL);
    g_free(*utf8_str);
    *utf8_str = local_str;
}

gchar *get_windows_app_data_location()
{
    const gchar *locap_app_data_path = g_getenv("LOCALAPPDATA");
    gchar *app_data_dir = g_strdup_printf("%s\\%s", locap_app_data_path, APP_FILES_DIRECTORY_NAME);
    return app_data_dir;
}

gchar *get_windows_app_temp_location()
{
    g_autofree gchar *app_data_dir = NULL;
    app_data_dir = get_windows_app_data_location();
    gchar *temp_dir = g_strdup_printf("%s\\temp", app_data_dir);
    g_mkdir_with_parents(temp_dir, 0755);

    return temp_dir;
}

const gchar *get_cur_ini_group_vdi()
{
    return "RemoteViewerConnect"; // legacy name to keep existing user settings
}

const gchar *get_cur_ini_group_direct()
{
    return "ManualConnect";
}

const gchar *get_cur_ini_group_controller()
{
    return "ControllerConnect";
}

//void gtk_combo_box_text_set_active_text(GtkComboBoxText *combo_box, const gchar *text)
//{
//    //GtkTreeModel *list_store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo_box));
//    //GtkTreeIter iter;
//    //gboolean valid = gtk_tree_model_get_iter_first (list_store, &iter);
//    //
//    //while (valid) {
//    //
//    //    valid = gtk_tree_model_iter_next(list_store, &iter);
//    //}
//    GtkTreeIter iter;
//    GtkTreeModel *model;
//    gint text_column;
//    gint column_type;
//    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
//    if(!model)
//        return;
//    text_column = gtk_combo_box_get_entry_text_column(GTK_COMBO_BOX(combo_box));
//    //g_return_val_if_fail (text_column >= 0, NULL);
//    column_type = gtk_tree_model_get_column_type (model, text_column);
//    //g_return_val_if_fail (column_type == G_TYPE_STRING, NULL);
//    gtk_tree_model_get(model, &iter, text_column, &text, -1);
//}

const gchar *util_get_os_name()
{
#ifdef _WIN64
    return "Windows 64-bit";
#elif _WIN32
    return "Windows 32-bit";
#elif __APPLE__ || __MACH__
    return "Mac OSX";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
    #elif __unix || __unix__
    return "Unix";
    #else
    return "Unknown";
#endif
}

const gchar *bool_to_str(gboolean flag)
{
    return (flag ? "true" : "false");
}

const gchar *vm_power_state_to_str(int power_state)
{
    VM_POWER_STATE state = (VM_POWER_STATE)power_state;
    if (state == VM_POWER_STATE_OFF)
        return "Выключена";
    else if (state == VM_POWER_STATE_PAUSED)
        return "На паузе";
    else if (state == VM_POWER_STATE_ON)
        return "Включена";
    else
        return "?";
}

void set_vm_power_state_on_label(GtkLabel *label, int power_state)
{
    if (!label)
        return;

    if ((VM_POWER_STATE)power_state == VM_POWER_STATE_ON) {
        gtk_label_set_text(label, "");

    } else {
        const gchar *vm_power_str = vm_power_state_to_str(power_state);
        gchar *final_message = g_strdup_printf(
                "<span background=\"#0022ff\"  color=\"white\">%s</span>", vm_power_str);
        gtk_label_set_markup(label, final_message);
        g_free(final_message);
    }
}

static void set_label_selectable(gpointer data, gpointer user_data G_GNUC_UNUSED)
{
    GtkWidget *widget = GTK_WIDGET(data);

    if (GTK_IS_LABEL(widget)) {
        gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
    }
}

static void
msg_box_parent_hidden(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data)
{
    GtkWidget *dialog_msg = (GtkWidget *)user_data;
    gtk_widget_hide(dialog_msg);
}

void show_msg_box_dialog(GtkWindow *parent, const gchar *message)
{
    GtkWidget *dialog_msg = gtk_message_dialog_new(parent,
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_WARNING,
                                                   GTK_BUTTONS_OK,
                                                   "%s", message);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog_msg), GTK_RESPONSE_ACCEPT);

    GtkWidget *area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog_msg));
    GtkContainer *box = (GtkContainer *) area;

    GList *children = gtk_container_get_children(box);
    g_list_foreach(children, set_label_selectable, NULL);
    g_list_free(children);

    gulong sig_handler = g_signal_connect(parent, "hide", G_CALLBACK(msg_box_parent_hidden), dialog_msg);

    gtk_widget_show_all(dialog_msg);
    gtk_dialog_run(GTK_DIALOG(dialog_msg));

    g_signal_handler_disconnect(G_OBJECT(parent), sig_handler);
    gtk_widget_destroy(dialog_msg);
}

void remove_char(char *str, char character)
{
    if (!str)
        return;

    char *dst;
    for (dst = str; *str != '\0'; str++) {
        *dst = *str;
        if (*dst != character)
            dst++;
    }
    *dst = '\0';
}

gchar *get_current_readable_time()
{
    GDateTime *datetime = g_date_time_new_now_local();
    gchar *time = g_date_time_format(datetime, "%Y-%m-%d   %H:%M:%S");
    g_date_time_unref(datetime);

    return time;
}

void copy_file_content(FILE *sourceFile, FILE *destFile)
{
    char ch;
    /* Copy file contents character by character. */
    while ((ch = fgetc(sourceFile)) != EOF)
    {
        fputc(ch, destFile);
        //g_info("symbol: %i.", ch);
    }
}

void extract_name_and_domain(const gchar *full_user_name, gchar **user_name, gchar **domain)
{
    if (strchr(full_user_name, '@')) {

        const gint token_amount = 2;
        gchar **name_domain_array = g_strsplit(full_user_name, "@", token_amount);

        if (g_strv_length(name_domain_array) == (guint)token_amount) {
            update_string_safely(user_name, name_domain_array[0]);
            update_string_safely(domain, name_domain_array[1]);
        }

        g_strfreev(name_domain_array);
    }
}

void util_show_monitor_config_window(GtkWindow *parent, GdkDisplay *display)
{
    // get config
    int monitor_amount = gdk_display_get_n_monitors(display);
    gchar *final_string = NULL;

    for (int i = 0; i < monitor_amount; ++i) {
        GdkMonitor *monitor = gdk_display_get_monitor(display, i);

        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);

        const char *manufacturer = gdk_monitor_get_manufacturer(monitor);
        const char *model = gdk_monitor_get_model(monitor);
        gboolean is_primary = gdk_monitor_is_primary(monitor);

        gchar *monitor_info = NULL;
        monitor_info = g_strdup_printf(
                "№ %i   (%i %i %i %i)   %s %s   %s",
                i, geometry.x, geometry.y, geometry.width, geometry.height,
                manufacturer ? manufacturer : "", model,
                is_primary ? "Primary monitor" : "");

        gchar *temp_final_string;
        if (final_string) {
            temp_final_string = g_strdup_printf("%s\n%s", final_string, monitor_info);
            g_free(final_string);
            g_free(monitor_info);

            final_string = temp_final_string;
        } else {
            final_string =  monitor_info;
        }
    }

    g_info("Monitor config: %s", final_string);

    // show config
    show_msg_box_dialog(parent, final_string);

    g_free(final_string);
}

int util_get_monitor_number(GdkDisplay *display)
{
#if GDK_VERSION_CUR_STABLE <= G_ENCODE_VERSION(3, 20)
    GdkScreen *screen = gdk_display_get_default_screen(display);
    return gdk_screen_get_n_monitors(screen);
#else
    return gdk_display_get_n_monitors(display);
#endif
}

void util_get_monitor_geometry(GdkDisplay *display, int num, GdkRectangle *geometry)
{
#if GDK_VERSION_CUR_STABLE <= G_ENCODE_VERSION(3, 20)
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gdk_screen_get_monitor_geometry(screen, num, geometry);
#else
    GdkMonitor *monitor = gdk_display_get_monitor(display, num);
    gdk_monitor_get_geometry(monitor, geometry);
#endif
}

gboolean util_check_if_monitor_primary(GdkDisplay *display, int num)
{
#if GDK_VERSION_CUR_STABLE <= G_ENCODE_VERSION(3, 20)
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gint primary_mon_num = gdk_screen_get_primary_monitor(screen);
    return num == primary_mon_num;
#else
    GdkMonitor *monitor = gdk_display_get_monitor(display, num);
    return gdk_monitor_is_primary(monitor);
#endif
}

gboolean util_check_if_monitor_number_valid(GdkDisplay *display, int num)
{
#if GDK_VERSION_CUR_STABLE <= G_ENCODE_VERSION(3, 20)
    int monitor_amount = util_get_monitor_number(display);
    return (num >= 0 && num < monitor_amount);
#else
    GdkMonitor *monitor = gdk_display_get_monitor(display, num);
    return (monitor != NULL); // valid if not NULL
#endif
}

const gchar *util_spice_channel_event_to_str(SpiceChannelEvent event)
{
    switch (event) {
        case SPICE_CHANNEL_NONE:
            return "SPICE_CHANNEL_NONE";
        case SPICE_CHANNEL_OPENED:
            return "SPICE_CHANNEL_OPENED";
        case SPICE_CHANNEL_SWITCHING:
            return "SPICE_CHANNEL_SWITCHING";
        case SPICE_CHANNEL_CLOSED:
            return "SPICE_CHANNEL_CLOSED";
        case SPICE_CHANNEL_ERROR_CONNECT:
            return "SPICE_CHANNEL_ERROR_CONNECT";
        case SPICE_CHANNEL_ERROR_TLS:
            return "SPICE_CHANNEL_ERROR_TLS";
        case SPICE_CHANNEL_ERROR_LINK:
            return "SPICE_CHANNEL_ERROR_LINK";
        case SPICE_CHANNEL_ERROR_AUTH:
            return "SPICE_CHANNEL_ERROR_AUTH";
        case SPICE_CHANNEL_ERROR_IO:
            return "SPICE_CHANNEL_ERROR_IO";
        default:
            return "UNKNOWN";
    }
}

guint util_send_message(SoupSession *soup_session, SoupMessage *msg, const char *uri_string)
{
    static int count = 0;
    g_info("Send_count: %i", ++count);

    guint status = soup_session_send_message(soup_session, msg);
    g_info("%s: Response code: %i  reason_phrase: %s  uri_string: %s",
           (const char *)__func__, status, msg->reason_phrase, uri_string);
    return status;
}

void util_free_veil_vm_data(VeilVmData *vm_data)
{
    free_memory_safely(&vm_data->vm_host);
    free_memory_safely(&vm_data->vm_username);
    free_memory_safely(&vm_data->vm_password);
    free_memory_safely(&vm_data->message);
    free_memory_safely(&vm_data->vm_verbose_name);

    if (vm_data->farm_array) {
        for (guint i = 0; i < vm_data->farm_array->len; ++i) {
            VeilFarmData farm_data = g_array_index(vm_data->farm_array, VeilFarmData, i);
            g_free(farm_data.farm_alias);

            if (farm_data.app_array) {
                for (guint j = 0; j < vm_data->farm_array->len; ++j) {
                    VeilAppData app_data = g_array_index(farm_data.app_array, VeilAppData, j);
                    g_free(app_data.app_alias);
                    g_free(app_data.app_name);
                    g_free(app_data.icon_base64);
                }

                g_array_free(farm_data.app_array, TRUE);
            }
        }

        g_array_free(vm_data->farm_array, TRUE);
    }

    free(vm_data);
}

// set info message
void util_set_message_to_info_label(GtkLabel *label, const gchar *message)
{
    if (!message)
        return;

    const guint max_str_width = 300;

    // trim message
    if ( strlen_safely(message) > max_str_width ) {
        gchar *message_str = g_strndup(message, max_str_width);
        gtk_label_set_text(label, message_str);
        g_free(message_str);
    } else {
        gtk_label_set_text(label, message);
    }

    gtk_widget_set_tooltip_text(GTK_WIDGET(label), message);
}

VmRemoteProtocol util_str_to_remote_protocol(const gchar *protocol_str)
{
    VmRemoteProtocol protocol = ANOTHER_REMOTE_PROTOCOL;
    if (g_strcmp0("SPICE", protocol_str) == 0)
        protocol = SPICE_PROTOCOL;
    else if (g_strcmp0("SPICE_DIRECT", protocol_str) == 0)
        protocol = SPICE_DIRECT_PROTOCOL;
    else if (g_strcmp0("RDP", protocol_str) == 0)
        protocol = RDP_PROTOCOL;
    else if (g_strcmp0("NATIVE_RDP", protocol_str) == 0)
        protocol = RDP_NATIVE_PROTOCOL;
    else if (g_strcmp0("X2GO", protocol_str) == 0)
        protocol = X2GO_PROTOCOL;

    return protocol;
}

const gchar *util_remote_protocol_to_str(VmRemoteProtocol protocol)
{
    switch (protocol) {
        case SPICE_PROTOCOL:
            return "SPICE";
        case SPICE_DIRECT_PROTOCOL:
            return "SPICE_DIRECT";
        case RDP_PROTOCOL:
            return "RDP";
        case RDP_NATIVE_PROTOCOL:
            return "NATIVE_RDP";
        case X2GO_PROTOCOL:
            return "X2GO";
        case ANOTHER_REMOTE_PROTOCOL:
        default:
            return "UNKNOWN_PROTOCOL";
    }
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
