/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "loadplay_launcher.h"

G_DEFINE_TYPE( LoadplayLauncher, loadplay_launcher, G_TYPE_OBJECT )


static void loadplay_launcher_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( loadplay_launcher_parent_class );
    ( *parent_class->finalize )( object );
}

static void loadplay_launcher_class_init(LoadplayLauncherClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = loadplay_launcher_finalize;

    // signals
    g_signal_new("job-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoadplayLauncherClass, job_finished),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void loadplay_launcher_init(LoadplayLauncher *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

static void loadplay_launcher_cb_child_watch(GPid pid, gint status, LoadplayLauncher *self)
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    g_spawn_close_pid(pid);

#if defined(_WIN32)
    //vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);
    self->pid = NULL;
#else
    self->pid = 0;
#endif
    self->is_launched = FALSE;

    g_signal_emit_by_name(self, "job-finished");
}

LoadplayLauncher *loadplay_launcher_new()
{
    return LOADPLAY_LAUNCHER( g_object_new( TYPE_LOADPLAY_LAUNCHER, NULL ) );
}

void loadplay_launcher_start(LoadplayLauncher *self, GtkWindow *parent, ConnectSettingsData *conn_data)
{
    g_info("%s", (const char *) __func__);

    if (conn_data == NULL || conn_data->loadplay_config == NULL) {
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    self->parent_widget = parent;

    // launch process
    gchar *argv[3] = {};

    const gchar *loadplay_dir = conn_data->loadplay_config->loadplay_client_path;
#ifdef G_OS_WIN32
    argv[0] = g_build_filename(loadplay_dir, "bin", "streaming.exe", NULL);
#else
    argv[0] = g_build_filename(loadplay_dir, "bin", "streaming", NULL);
#endif
    g_info("app path: %s", argv[0]);

    g_autofree gchar *loadplay_config_file_name = NULL;
    loadplay_config_file_name = loadplay_config_get_file_name();
    argv[1] = g_locale_to_utf8(loadplay_config_file_name, -1, NULL, NULL, NULL);
    g_info("app config path: %s", argv[1]);

    GError *error = NULL;
    self->is_launched = g_spawn_async(loadplay_dir, argv, NULL,
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
        full_err_msg = g_strdup_printf("LOADPLAY CLIENT SPAWN FAILED. %s", err_msg);
        g_warning("%s", full_err_msg);
        show_msg_box_dialog(NULL, full_err_msg);

        g_clear_error(&error);
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    //vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED);


    // stop process callback
    g_child_watch_add(self->pid, (GChildWatchFunc)loadplay_launcher_cb_child_watch, self);
}

void loadplay_launcher_stop(LoadplayLauncher *self)
{
    if (self->pid) {
        terminate_process(self->pid);
    }
}