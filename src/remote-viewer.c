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
#include "vdi_manager.h"
#include "settings_data.h"
#include "usbredir_controller.h"
#include "rdp_viewer.h"
#include "controller_client/controller_session.h"


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

static void on_logged_out(gpointer data G_GNUC_UNUSED, const gchar *reason_phrase, RemoteViewer *self);
static void connect_to_vm(gpointer data G_GNUC_UNUSED, RemoteViewer *self);
static void on_vm_connection_finished(gpointer data G_GNUC_UNUSED, RemoteViewer *self);
static void show_vm_selector_window(gpointer data G_GNUC_UNUSED, RemoteViewer *self);
static void on_quit_requested(gpointer data G_GNUC_UNUSED, RemoteViewer *self);

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

    self->css_provider = setup_css(); // CSS setup

    self->veil_messenger = veil_messenger_new();
#if defined(_WIN32) || defined(__MACH__)
    g_signal_connect(self->native_rdp_launcher, "job-finished", G_CALLBACK(on_vm_connection_finished), self);
#endif
    g_signal_connect(self->x2go_launcher, "job-finished", G_CALLBACK(on_vm_connection_finished), self);

    g_signal_connect(self->rdp_viewer, "job-finished", G_CALLBACK(on_vm_connection_finished), self);
    g_signal_connect(self->rdp_viewer, "quit-requested", G_CALLBACK(on_quit_requested), self);

    virt_viewer_app_setup(self->virt_viewer_obj, &self->conn_data);
    g_signal_connect(self->virt_viewer_obj, "job-finished", G_CALLBACK(on_vm_connection_finished), self);
    g_signal_connect(self->virt_viewer_obj, "quit-requested", G_CALLBACK(on_quit_requested), self);

    g_signal_connect(self->loudplay_launcher, "job-finished", G_CALLBACK(on_vm_connection_finished), self);

    self->remote_viewer_connect = remote_viewer_connect_new();
    g_signal_connect(self->remote_viewer_connect, "show-vm-selector-requested",
            G_CALLBACK(show_vm_selector_window), self);
    g_signal_connect(self->remote_viewer_connect, "connect-to-vm-requested", G_CALLBACK(connect_to_vm), self);
    g_signal_connect(self->remote_viewer_connect, "quit-requested", G_CALLBACK(on_quit_requested), self);

    self->vdi_manager = vdi_manager_new();
    g_signal_connect(self->vdi_manager, "logged-out", G_CALLBACK(on_logged_out), self);
    g_signal_connect(self->vdi_manager, "connect-to-vm-requested", G_CALLBACK(connect_to_vm), self);
    g_signal_connect(self->vdi_manager, "quit-requested", G_CALLBACK(on_quit_requested), self);

    self->controller_manager = controller_manager_new();
    g_signal_connect(self->controller_manager, "logged-out", G_CALLBACK(on_logged_out), self);
    g_signal_connect(self->controller_manager, "connect-to-vm-requested", G_CALLBACK(connect_to_vm), self);
    g_signal_connect(self->controller_manager, "quit-requested", G_CALLBACK(on_quit_requested), self);

    // Show auth window
    remote_viewer_connect_show(self->remote_viewer_connect, &self->conn_data, self->app_updater);
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

int remote_viewer_command_line(GApplication *application, GApplicationCommandLine *command_line)
{
    g_info("%s", (const char*)__func__);
    int ret = G_APPLICATION_CLASS(remote_viewer_parent_class)->command_line(application, command_line);

    // Функция remote_viewer_command_line вызывается, если происходит попытка запуска второго экземпляра
    // приложения. В этом случае показываем окно первого экземляра поверх других окон
    remote_viewer_connect_raise(REMOTE_VIEWER(application)->remote_viewer_connect);
    vdi_manager_raise(REMOTE_VIEWER(application)->vdi_manager);

    return ret;
}

static void
remote_viewer_on_ws_cmd_received(gpointer data G_GNUC_UNUSED, const gchar *cmd, RemoteViewer *self)
{
    if (g_strcmp0(cmd, "DISCONNECT") == 0) {
        on_logged_out(NULL, _("Logout demanded by server"), self);
    }
}

static void
remote_viewer_class_init(RemoteViewerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *g_app_class = G_APPLICATION_CLASS(klass);

    g_type_class_add_private(klass, sizeof (RemoteViewerPrivate));

    g_app_class->startup = remote_viewer_startup;
    g_app_class->command_line = remote_viewer_command_line;
    g_app_class->local_command_line = remote_viewer_local_command_line;

    object_class->dispose = remote_viewer_dispose;
    object_class->finalize = remote_viewer_finalize;

}

static void
remote_viewer_init(RemoteViewer *self)
{
    g_info("%s", (const char*)__func__);
    self->priv = GET_PRIVATE(self);

    memset(&self->conn_data, 0, sizeof(ConnectSettingsData));
    self->conn_data.not_connected_to_prev_pool_yet = TRUE;
    self->conn_data.is_first_auth_time = TRUE;

    self->rdp_viewer = rdp_viewer_new();
    // virt-viewer
    self->virt_viewer_obj = virt_viewer_app_new();
    virt_viewer_app_set_app_pointer(self->virt_viewer_obj, GTK_APPLICATION(self));

    self->x2go_launcher = x2go_launcher_new();
#if defined(_WIN32) || defined(__MACH__)
    self->native_rdp_launcher = native_rdp_launcher_new();
#endif
    self->loudplay_launcher = loudplay_launcher_new();

    // app updater entity
    self->app_updater = app_updater_new();

    // init usb redir
    usbredir_controller_get_static();

    // create vdi session
    get_vdi_session_static();

    // create controller session
    get_controller_session_static();

    // signals
    g_signal_connect(get_vdi_session_static(), "ws-cmd-received",
                     G_CALLBACK(remote_viewer_on_ws_cmd_received), self);
}

RemoteViewer *
remote_viewer_new(gboolean unique_app)
{
    g_info("remote_viewer_new");

    GApplicationFlags flags = unique_app ? G_APPLICATION_HANDLES_COMMAND_LINE : G_APPLICATION_NON_UNIQUE;
    return g_object_new(REMOTE_VIEWER_TYPE,
                        "application-id", "mashtab.veil-connect",
                        "flags", flags,
                        NULL);
}

void remote_viewer_free_resources(RemoteViewer *self)
{
    settings_data_save_all(&self->conn_data);
    g_object_unref(self->loudplay_launcher);
#if defined(_WIN32) || defined(__MACH__)
    g_object_unref(self->native_rdp_launcher);
#endif
    g_object_unref(self->x2go_launcher);
    g_object_unref(self->rdp_viewer);
    g_object_unref(self->virt_viewer_obj);
    g_object_unref(self->app_updater);
    g_object_unref(self->vdi_manager);

    if (self->controller_manager)
        g_object_unref(self->controller_manager);
    if (self->veil_messenger)
        g_object_unref(self->veil_messenger);

    usbredir_controller_deinit_static();
    vdi_session_static_destroy();
    controller_session_static_destroy();

    settings_data_clear(&self->conn_data);

    if (self->remote_viewer_connect)
        g_object_unref(self->remote_viewer_connect);

    g_object_unref(self->css_provider);
}

static void
on_logged_out(gpointer data G_GNUC_UNUSED, const gchar *reason_phrase, RemoteViewer *self)
{
    loudplay_launcher_stop(self->loudplay_launcher);
#if  defined(_WIN32) || defined(__MACH__)
    native_rdp_launcher_stop(self->native_rdp_launcher);
#endif
    veil_messenger_hide(self->veil_messenger);
    x2go_launcher_stop(self->x2go_launcher);
    rdp_viewer_stop(self->rdp_viewer, NULL, TRUE);
    virt_viewer_app_stop(self->virt_viewer_obj, NULL);
    controller_manager_finish_job(self->controller_manager);
    vdi_manager_finish_job(self->vdi_manager);

    remote_viewer_connect_show(self->remote_viewer_connect, &self->conn_data, self->app_updater);
    util_set_message_to_info_label(GTK_LABEL(
            self->remote_viewer_connect->message_display_label), reason_phrase);
}

static void
connect_to_vm(gpointer data G_GNUC_UNUSED, RemoteViewer *self)
{
    VmRemoteProtocol protocol = ANOTHER_REMOTE_PROTOCOL;
    switch (self->conn_data.global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            protocol = vdi_session_get_current_remote_protocol();
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            protocol = self->conn_data.protocol_in_direct_app_mode;
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            protocol = controller_session_get_current_remote_protocol();
            break;
        }
    }

    // connect to vm depending on remote protocol
    if (protocol == RDP_PROTOCOL) {
        // В режиме прямого подключения к ВМ читаем настройки из стандартного RDP файла, если требуется
        if (self->conn_data.global_app_mode == GLOBAL_APP_MODE_DIRECT && self->conn_data.rdp_settings.use_rdp_file) {
            rdp_settings_read_standard_file(&self->conn_data.rdp_settings, NULL);
        } else {
            rdp_settings_set_connect_data(&self->conn_data.rdp_settings, self->conn_data.user,
                                          self->conn_data.password, self->conn_data.domain,
                                          self->conn_data.ip, self->conn_data.port);
        }
        rdp_viewer_start(self->rdp_viewer, &self->conn_data, self->veil_messenger, &self->conn_data.rdp_settings);
#if  defined(_WIN32) || defined(__MACH__)
        } else if (protocol == RDP_NATIVE_PROTOCOL) {
                rdp_settings_set_connect_data(&self->conn_data.rdp_settings, self->conn_data.user,
                                              self->conn_data.password, self->conn_data.domain,
                                              self->conn_data.ip, self->conn_data.port);
                native_rdp_launcher_start(self->native_rdp_launcher, NULL, &self->conn_data.rdp_settings);
#endif
    } else if (protocol == X2GO_PROTOCOL) {
        x2go_launcher_start(self->x2go_launcher, self->conn_data.user, self->conn_data.password, &self->conn_data);
    } else if (protocol == SPICE_PROTOCOL || protocol == SPICE_DIRECT_PROTOCOL) {
        virt_viewer_app_instant_start(self->virt_viewer_obj, &self->conn_data);
    } else if (protocol == LOUDPLAY_PROTOCOL) {
        loudplay_launcher_start(self->loudplay_launcher, NULL, &self->conn_data);
    } else {
        g_autofree gchar *msg = NULL;
        msg = g_strdup_printf(_("Protocol is not supported: %s"), util_remote_protocol_to_str(protocol));
        show_msg_box_dialog(NULL, msg);
        on_vm_connection_finished(NULL, self);
    }
}

static void
on_quit_requested(gpointer data G_GNUC_UNUSED, RemoteViewer *self)
{
    g_application_quit(G_APPLICATION(self));
}

static void
on_vm_connection_finished(gpointer data G_GNUC_UNUSED, RemoteViewer *self)
{
    switch (self->conn_data.global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            // show VDI manager window
            vdi_manager_show(self->vdi_manager, &self->conn_data);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            controller_manager_show(self->controller_manager, &self->conn_data);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT:
        default: {
            remote_viewer_connect_show(self->remote_viewer_connect, &self->conn_data, self->app_updater);
            break;
        }
    }
}

//
static void
show_vm_selector_window(gpointer data G_GNUC_UNUSED, RemoteViewer *self)
{
    switch (self->conn_data.global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            // show VDI manager window
            vdi_manager_show(self->vdi_manager, &self->conn_data);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            controller_manager_show(self->controller_manager, &self->conn_data);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT:
        default: {
            // В режиме подключения к ВМ на прямую подключаемся к ВМ
            connect_to_vm(NULL, self);
            break;
        }
    }
}
