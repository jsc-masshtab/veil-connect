/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include "loudplay_launcher.h"
#include "vdi_event.h"

G_DEFINE_TYPE( LoudplayLauncher, loudplay_launcher, G_TYPE_OBJECT )


static void loudplay_launcher_finalize(GObject *object)
{
    g_info("%s", (const char *) __func__);

    LoudplayLauncher *self = LOUDPLAY_LAUNCHER(object);
    g_object_unref(self->ctrl_server);
    if(self->ctrl_widget)
        g_object_unref(self->ctrl_widget);

    GObjectClass *parent_class = G_OBJECT_CLASS( loudplay_launcher_parent_class );
    ( *parent_class->finalize )( object );
}

static void
loudplay_launcher_on_settings_change_requested(gpointer data G_GNUC_UNUSED, LoudplayLauncher *self)
{
    loudplay_control_server_update_config(self->ctrl_server, self->ctrl_widget->p_conn_data->loudplay_config);
    loudplay_control_server_send_cur_config_all(self->ctrl_server);
}

static void
loudplay_launcher_on_event_happened(gpointer data G_GNUC_UNUSED, const gchar *text, LoudplayLauncher *self)
{
    loudplay_control_widget_set_status(self->ctrl_widget, text);
}

static void
loudplay_launcher_on_process_stop_requested(gpointer data G_GNUC_UNUSED, LoudplayLauncher *self)
{
    loudplay_control_server_send_stop_streaming(self->ctrl_server);
}

static void loudplay_launcher_class_init(LoudplayLauncherClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = loudplay_launcher_finalize;

    // signals
    g_signal_new("job-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoudplayLauncherClass, job_finished),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void loudplay_launcher_init(LoudplayLauncher *self)
{
    g_info("%s", (const char *) __func__);

    self->ctrl_server = loudplay_control_server_new();
    self->ctrl_widget = NULL;

    g_signal_connect(self->ctrl_server, "event-happened",
                     G_CALLBACK(loudplay_launcher_on_event_happened), self);
}

static void loudplay_launcher_cb_child_watch(GPid pid, gint status, LoudplayLauncher *self)
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    g_spawn_close_pid(pid);

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);

#if defined(_WIN32)
    self->pid = NULL;
#else
    self->pid = 0;
#endif
    self->is_launched = FALSE;

    // Stop control server
    loudplay_control_server_stop(self->ctrl_server);
    loudplay_control_widget_hide(self->ctrl_widget);
    g_signal_emit_by_name(self, "job-finished");
}

// Запустить процесс. Возвращает TRUE, если успешно.
static gboolean loudplay_launcher_launch_process(LoudplayLauncher *self, ConnectSettingsData *conn_data)
{
    gchar *argv[3] = {};

    const gchar *loudplay_dir = conn_data->loudplay_config->loudplay_client_path;
    if(strlen_safely(loudplay_dir) == 0) {
        g_warning("%s: loudplay_client_path is not specified", (const char *)__func__);
        return FALSE;
    }

#ifdef G_OS_WIN32
    const gchar *program_name = "streaming.exe";
#else
    const gchar *program_name = "streaming";
#endif
    if (conn_data->loudplay_config->is_client_path_relative) {
        g_autofree gchar *current_dir = NULL;
        current_dir = g_get_current_dir();
        argv[0] = g_build_filename(current_dir, loudplay_dir, "bin", program_name, NULL);
    } else {
        argv[0] = g_build_filename(loudplay_dir, "bin", program_name, NULL);
    }
    g_info("app path: %s", argv[0]);

    // The second argument is port number
    g_autofree gchar *port = NULL;
    port = g_strdup_printf("%i", CONTROL_SERVER_PORT);
    argv[1] = g_locale_to_utf8(port, -1, NULL, NULL, NULL);
    g_info("Control server port: %s", port);

    GError *error = NULL;
    self->is_launched = g_spawn_async(loudplay_dir, argv, NULL,
                                      G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
                                      NULL, &self->pid, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    gchar *err_msg = NULL;
    if (!self->is_launched) {
        if (error)
            err_msg = error->message;

        g_autofree gchar *full_err_msg = NULL;
        full_err_msg = g_strdup_printf("LOUDPLAY CLIENT SPAWN FAILED. %s", err_msg);
        g_warning("%s", full_err_msg);
        show_msg_box_dialog(NULL, full_err_msg);

        g_clear_error(&error);
        return FALSE;
    }

    // callback for process termination
    g_child_watch_add(self->pid, (GChildWatchFunc)loudplay_launcher_cb_child_watch, self);

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED);

    return TRUE;
}

LoudplayLauncher *loudplay_launcher_new()
{
    return LOUDPLAY_LAUNCHER( g_object_new( TYPE_LOUDPLAY_LAUNCHER, NULL ) );
}

void loudplay_launcher_start(LoudplayLauncher *self, GtkWindow *parent, ConnectSettingsData *conn_data)
{
    g_info("%s", (const char *) __func__);

    loudplay_control_server_stop(self->ctrl_server); // На всякий случай, если сервер по какой-либо причине запущен

    if (conn_data == NULL || conn_data->loudplay_config == NULL) {
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    self->parent_widget = parent;

    // Show control GUI
    if(self->ctrl_widget == NULL) {
        self->ctrl_widget = loudplay_control_widget_new(conn_data);

        g_signal_connect(self->ctrl_widget, "settings-change-requested",
                         G_CALLBACK(loudplay_launcher_on_settings_change_requested), self);
        g_signal_connect(self->ctrl_widget, "process-stop-requested",
                         G_CALLBACK(loudplay_launcher_on_process_stop_requested), self);
    }
    loudplay_control_widget_show_on_top(self->ctrl_widget);

    // write current url to config
    g_autofree gchar *url = NULL;
    url = g_strdup_printf("rtsp://%s:%i/desktop", conn_data->ip, conn_data->port);
    g_object_set(conn_data->loudplay_config, "server-url", url, NULL);

    // Start control server
    loudplay_control_server_start(self->ctrl_server, conn_data->loudplay_config);

    // launch process
    gboolean launch_success = loudplay_launcher_launch_process(self, conn_data);
    // Stop control server if client launch was unsuccessful
    if (!launch_success) {
        loudplay_control_server_stop(self->ctrl_server);
        loudplay_control_widget_hide(self->ctrl_widget);
        g_signal_emit_by_name(self, "job-finished");
    }
}

void loudplay_launcher_stop(LoudplayLauncher *self)
{
    if (self->pid) {
        terminate_process(self->pid);
    }
}