/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "x2go_launcher.h"


G_DEFINE_TYPE( X2goLauncher, x2go_launcher, G_TYPE_OBJECT )


static void x2go_launcher_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( x2go_launcher_parent_class );
    ( *parent_class->finalize )( object );
}

static void x2go_launcher_class_init(X2goLauncherClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = x2go_launcher_finalize;

    // signals
    g_signal_new("job-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(X2goLauncherClass, job_finished),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void x2go_launcher_init(X2goLauncher *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

X2goLauncher *x2go_launcher_new()
{
    g_info("%s", (const char *)__func__);
    X2goLauncher *obj = X2GO_LAUNCHER( g_object_new( TYPE_X2GO_LAUNCHER, NULL ) );
    return obj;
}

void x2go_launcher_start(X2goLauncher *self, const gchar *user, const gchar *password,
        const ConnectSettingsData *conn_data)
{
    self->p_conn_data = conn_data;
    self->send_signal_upon_job_finish = TRUE;

    switch (conn_data->x2Go_settings.app_type) {
        case X2GO_APP_QT_CLIENT:
            memset(&self->qt_client_data, 0, sizeof(X2goQtClientData));
            x2go_launcher_start_qt_client(self, user, password);
            break;
        case X2GO_APP_PYHOCA_CLI:
            memset(&self->pyhoca_data, 0, sizeof(X2goPyhocaData));
            x2go_launcher_start_pyhoca(self, user, password);
            break;
        default:
            g_signal_emit_by_name(self, "job-finished");
            break;
    }
}

void x2go_launcher_stop(X2goLauncher *self)
{
    self->send_signal_upon_job_finish = FALSE;
    self->pyhoca_data.clear_all_upon_sig_termination = TRUE;
#ifdef G_OS_WIN32
    x2go_launcher_stop_pyhoca(self, 0);
#elif __linux__
    x2go_launcher_stop_pyhoca(self, SIGKILL);
#endif
    if (self->qt_client_data.pid)
        terminate_process(self->qt_client_data.pid);
}