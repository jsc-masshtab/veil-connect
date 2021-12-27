/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <libxml/uri.h>

#include "settingsfile.h"
#include "virt-viewer-session-spice.h"
#include "virt-viewer-app.h"
#include "virt-viewer-auth.h"
#include "virt-viewer-file.h"
#include "virt-viewer-session.h"
#include "remote-viewer-util.h"
#include "remote-viewer.h"
#include "remote-viewer-connect.h"
#include "vdi_manager.h"
#include "settings_data.h"
#include "usbredir_controller.h"
#include "rdp_viewer.h"
#include "x2go/x2go_launcher.h"
#if defined(_WIN32) || defined(__MACH__)
#include "native_rdp_launcher.h"
#endif


struct _RemoteViewerPrivate {
    gpointer reserved;
};

G_DEFINE_TYPE (RemoteViewer, remote_viewer, GTK_TYPE_APPLICATION)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), REMOTE_VIEWER_TYPE, RemoteViewerPrivate))

enum RemoteViewerProperties {
    PROP_0,
#ifdef HAVE_OVIRT
    PROP_OVIRT_FOREIGN_MENU,
#endif
};

static void remote_viewer_start(RemoteViewer *self);

static GtkCssProvider *setup_css()
{
    GtkCssProvider *cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "css_style.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(cssProvider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
    return cssProvider;
}

static void
remote_viewer_startup(GApplication *app)
{
    G_APPLICATION_CLASS(remote_viewer_parent_class)->startup(app);

    RemoteViewer *self = REMOTE_VIEWER(app);

    // read ini file
    settings_data_read_all(&self->conn_data);

    virt_viewer_app_setup(self->virt_viewer_obj, &self->conn_data);

    GtkCssProvider *css_provider = setup_css(); // CSS setup
    remote_viewer_start(self);
    g_object_unref(css_provider);

    g_application_quit(app);
}

static void
remote_viewer_dispose(GObject *object)
{
    g_info("%s", (const char*)__func__);
    G_OBJECT_CLASS(remote_viewer_parent_class)->dispose(object);
}

static void
remote_viewer_finalize(GObject *object)
{
    g_info("%s", (const char*)__func__);
    G_OBJECT_CLASS(remote_viewer_parent_class)->finalize(object);
}

static gboolean
remote_viewer_local_command_line (GApplication   *gapp,
                                  gchar        ***args,
                                  int            *status)
{
    gboolean ret = FALSE;
    gint argc = g_strv_length(*args);
    GError *error = NULL;
    GOptionContext *context = g_option_context_new(NULL);
    GOptionGroup *group = g_option_group_new(PACKAGE, NULL, NULL, gapp, NULL);

    *status = 0;
    g_option_context_set_main_group(context, group);

    g_option_context_add_group(context, gtk_get_option_group(FALSE));

#ifdef HAVE_GTK_VNC
    g_option_context_add_group(context, vnc_display_get_option_group());
#endif

#ifdef HAVE_SPICE_GTK
    g_option_context_add_group(context, spice_get_option_group());
#endif

    if (!g_option_context_parse(context, &argc, args, &error)) {
        if (error != NULL) {
            g_printerr(_("%s\n"), error->message);
            g_error_free(error);
        }

        *status = 1;
        ret = TRUE;
        goto end;
    }

    end:
    g_option_context_free(context);
    return ret;
}

static void
remote_viewer_class_init(RemoteViewerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *g_app_class = G_APPLICATION_CLASS(klass);

    g_type_class_add_private (klass, sizeof (RemoteViewerPrivate));

    g_app_class->startup = remote_viewer_startup;
    g_app_class->command_line = NULL; /* inhibit GApplication default handler */

    object_class->dispose = remote_viewer_dispose;
    object_class->finalize = remote_viewer_finalize;

    g_app_class->local_command_line = remote_viewer_local_command_line;
}

static void
remote_viewer_init(RemoteViewer *self)
{
    g_info("%s", (const char*)__func__);
    self->priv = GET_PRIVATE(self);

    memset(&self->conn_data, 0, sizeof(ConnectSettingsData));
    self->conn_data.not_connected_to_prev_pool_yet = TRUE;

    // virt-viewer
    self->virt_viewer_obj = virt_viewer_app_new();
    virt_viewer_app_set_app_pointer(self->virt_viewer_obj, GTK_APPLICATION(self));

    // app updater entity
    self->app_updater = app_updater_new();

    // init usb redir
    usbredir_controller_get_static();

    // create vdi session
    get_vdi_session_static();
}

RemoteViewer *
remote_viewer_new(void)
{
    g_info("remote_viewer_new");
    return g_object_new(REMOTE_VIEWER_TYPE,
                        "application-id", "mashtab.veil-connect",
                        "flags", G_APPLICATION_NON_UNIQUE,
                        NULL);
}

void remote_viewer_free_resources(RemoteViewer *self)
{
    g_object_unref(self->virt_viewer_obj);
    g_object_unref(self->app_updater);
    if (self->vdi_manager)
        g_object_unref(self->vdi_manager);

    if (self->veil_messenger)
        g_object_unref(self->veil_messenger);

    usbredir_controller_deinit_static();
    vdi_session_static_destroy();

    settings_data_clear(&self->conn_data);
}

static void
remote_viewer_start(RemoteViewer *self)
{
    //create veil messenger
    self->veil_messenger = veil_messenger_new();
    // remote connect dialog
retry_auth:

    veil_messenger_hide(self->veil_messenger);
    // Забираем из ui адрес и порт
    GtkResponseType dialog_window_response = remote_viewer_connect_dialog(self);
    if (dialog_window_response == GTK_RESPONSE_CLOSE)
        goto to_exit;

    // После такого как забрали адресс с логином и паролем действуем в зависимости от opt_manual_mode
    // 1) в мануальном режиме сразу подключаемся к удаленноиу раб столу
    // 2) В дефолтном режиме вызываем vdi manager. В нем пользователь выберет машину для подключения
retry_connect_to_vm:
    /// instant connect attempt
    if (self->conn_data.opt_manual_mode) {
        RemoteViewerState app_state = APP_STATE_AUTH_DIALOG;
        if (vdi_session_get_current_remote_protocol() == VDI_RDP_PROTOCOL) {
            // Либо берем данные ГУИ, либо парсим rdp файл
            gboolean read_from_std_rdp_file = read_int_from_ini_file("RDPSettings", "use_rdp_file", 0);
            if (read_from_std_rdp_file) {
                rdp_settings_read_standard_file(&self->conn_data.rdp_settings, NULL);
            } else {
                rdp_settings_read_ini_file(&self->conn_data.rdp_settings, TRUE);
                rdp_settings_set_connect_data(&self->conn_data.rdp_settings, self->conn_data.user,
                        self->conn_data.password, self->conn_data.domain, self->conn_data.ip, self->conn_data.port);
            }
            app_state = rdp_viewer_start(self, &self->conn_data.rdp_settings);

        } else if (vdi_session_get_current_remote_protocol() == VDI_X2GO_PROTOCOL) {
            x2go_launcher_start(self->conn_data.user, self->conn_data.password, &self->conn_data);
        } else { // SPICE by default
            virt_viewer_app_set_spice_session_data(self->virt_viewer_obj, self->conn_data.ip, self->conn_data.port,
                                                   self->conn_data.user, self->conn_data.password);
            virt_viewer_app_instant_start(self->virt_viewer_obj);
            // go back to auth or quit
            if (virt_viewer_app_is_quitting(self->virt_viewer_obj))
                app_state = APP_STATE_EXITING;
            else
                app_state = APP_STATE_AUTH_DIALOG;
        }

        if (app_state == APP_STATE_AUTH_DIALOG)
            goto retry_auth;
        else if (app_state == APP_STATE_EXITING)
            goto to_exit;
    /// VDI connect mode
    } else {
        // show VDI manager window
        if (self->vdi_manager == NULL)
            self->vdi_manager = vdi_manager_new();
        RemoteViewerState next_app_state = vdi_manager_dialog(self->vdi_manager, &self->conn_data);
        if (next_app_state == APP_STATE_AUTH_DIALOG)
            goto retry_auth;
        else if (next_app_state == APP_STATE_EXITING)
            goto to_exit;

        // connect to vm depending remote protocol
        next_app_state = APP_STATE_VDI_DIALOG;
        if (vdi_session_get_current_remote_protocol() == VDI_RDP_PROTOCOL) {
            // Читаем из ini настройки remote_app только если они еще не установлены ранее (они могут быть получены от
            // RDS пула, например)
            rdp_settings_read_ini_file(&self->conn_data.rdp_settings, !self->conn_data.rdp_settings.is_remote_app);
            rdp_settings_set_connect_data(&self->conn_data.rdp_settings, vdi_session_get_vdi_username(),
                            vdi_session_get_vdi_password(), self->conn_data.domain, self->conn_data.ip, 0);
            next_app_state = rdp_viewer_start(self, &self->conn_data.rdp_settings);
#ifdef _WIN32
        } else if (vdi_session_get_current_remote_protocol() == VDI_RDP_NATIVE_PROTOCOL) {
            rdp_settings_read_ini_file(&self->conn_data.rdp_settings, !self->conn_data.rdp_settings.is_remote_app);
            rdp_settings_set_connect_data(&self->conn_data.rdp_settings, vdi_session_get_vdi_username(),
                                          vdi_session_get_vdi_password(), self->conn_data.domain,
                                          self->conn_data.ip, 0);
            launch_native_rdp_client(NULL, &self->conn_data.rdp_settings);
#endif
        } else if (vdi_session_get_current_remote_protocol() == VDI_X2GO_PROTOCOL) {
            x2go_launcher_start(vdi_session_get_vdi_username(), vdi_session_get_vdi_password(), &self->conn_data);
        } else { // spice by default
            virt_viewer_app_set_spice_session_data(self->virt_viewer_obj, self->conn_data.ip, self->conn_data.port,
                                                   self->conn_data.user, self->conn_data.password);
            virt_viewer_app_set_window_name(self->virt_viewer_obj, self->conn_data.vm_verbose_name,
                    vdi_session_get_vdi_username());
            virt_viewer_app_instant_start(self->virt_viewer_obj);
            next_app_state = virt_viewer_get_next_app_state(self->virt_viewer_obj);
        }

        if (next_app_state == APP_STATE_EXITING)
            goto to_exit;
        else if (next_app_state == APP_STATE_AUTH_DIALOG)
            goto retry_auth;
        else if (next_app_state == APP_STATE_VDI_DIALOG)
            goto retry_connect_to_vm;
    }

to_exit:
    settings_data_save_all(&self->conn_data);
}
