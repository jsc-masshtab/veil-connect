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
#include "connect_settings_data.h"

#include "usbredir_controller.h"

#include "rdp_viewer.h"
#ifdef _WIN32
#include "windows_rdp_launcher.h"
#endif


extern gboolean opt_manual_mode;

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

static void remote_viewer_start(RemoteViewer *self, RemoteViewerState remoteViewerState);

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
    virt_viewer_app_setup(self->virt_viewer_obj);

    GtkCssProvider *css_provider = setup_css(); // CSS setup
    remote_viewer_start(self, APP_STATE_AUTH_DIALOG);
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
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
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

    // virt-viewer
    self->virt_viewer_obj = virt_viewer_app_new();
    virt_viewer_app_set_app_pointer(self->virt_viewer_obj, GTK_APPLICATION(self));

    // network stats monitor
    self->net_speedometer = net_speedometer_new();
    net_speedometer_set_pointer_to_virt_viewer_app(self->net_speedometer, self->virt_viewer_obj);

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
    g_object_unref(self->net_speedometer);
    g_object_unref(self->app_updater);
    if (self->vdi_manager)
        g_object_unref(self->vdi_manager);

    if (self->veil_messenger)
        g_object_unref(self->veil_messenger);

    usbredir_controller_deinit_static();
    vdi_session_static_destroy();
}

// akward method
static void set_spice_session_data(VirtViewerApp *app, gchar *ip, int port, gchar *user, gchar *password)
{
    g_info("%s port %i\n", (const char *)__func__, port);
    g_info("%s user %s\n", (const char *)__func__, user);

    gchar *guri = g_strdup_printf("spice://%s:%i", ip, port);
    g_strstrip(guri);
    g_object_set(app, "guri", guri, NULL);
    g_free(guri);

    virt_viewer_session_spice_set_credentials(user, password);
}

static void connect_settings_data_clear(ConnectSettingsData *connect_settings_data)
{
    g_info("%s", (const char *)__func__);
    free_memory_safely(&connect_settings_data->user);
    free_memory_safely(&connect_settings_data->password);
    free_memory_safely(&connect_settings_data->domain);
    free_memory_safely(&connect_settings_data->ip);
    free_memory_safely(&connect_settings_data->vm_verbose_name);

    rdp_settings_clear(&connect_settings_data->rdp_settings);
}

static void remote_viewer_vm_changed_notify(RemoteViewer *self, const gchar *vm_id, const gchar *vm_ip)
{
    vdi_ws_client_send_vm_changed(vdi_session_get_ws_client(), vm_id);
    net_speedometer_update_vm_ip(self->net_speedometer, vm_ip);
}

static void
remote_viewer_start(RemoteViewer *self, RemoteViewerState remoteViewerState G_GNUC_UNUSED)
{
    ConnectSettingsData con_data = {};
    //create veil messenger
    self->veil_messenger = veil_messenger_new();
    // remote connect dialog
retry_auth:
    {
        veil_messenger_hide(self->veil_messenger);
        // Забираем из ui адрес и порт
        GtkResponseType dialog_window_response = remote_viewer_connect_dialog(self, &con_data);
        if (dialog_window_response == GTK_RESPONSE_CLOSE)
            goto to_exit;
    }
    // После такого как забрали адресс с логином и паролем действуем в зависимости от opt_manual_mode
    // 1) в мануальном режиме сразу подключаемся к удаленноиу раб столу
    // 2) В дефолтном режиме вызываем vdi manager. В нем пользователь выберет машину для подключения
retry_connect_to_vm:
    /// instant connect attempt
    if (opt_manual_mode) {
        if (vdi_session_get_current_remote_protocol() == VDI_RDP_PROTOCOL) {
            // Либо берем данные ГУИ, либо парсим rdp файл
            gboolean read_from_std_rdp_file = read_int_from_ini_file("RDPSettings", "use_rdp_file", 0);
            if (read_from_std_rdp_file) {
                rdp_settings_read_standard_file(&con_data.rdp_settings, NULL);
            } else {
                rdp_settings_read_ini_file(&con_data.rdp_settings, TRUE);
                rdp_settings_set_connect_data(&con_data.rdp_settings, con_data.user, con_data.password,
                        con_data.domain, con_data.ip, con_data.port);
            }

            RemoteViewerState app_state = rdp_viewer_start(self, &con_data.rdp_settings);
            if (app_state == APP_STATE_AUTH_DIALOG)
                goto retry_auth;
            else if (app_state == APP_STATE_EXITING)
                goto to_exit;

        } else { // spice by default
        set_spice_session_data(self->virt_viewer_obj, con_data.ip, con_data.port, con_data.user, con_data.password);
        virt_viewer_app_instant_start(self->virt_viewer_obj);
        // go back to auth or quit
        if (virt_viewer_app_is_quitting(self->virt_viewer_obj))
            goto to_exit;
        else
            goto retry_auth;
    }
    /// VDI connect mode
    } else {
        // remember username
        if (con_data.user)
            g_object_set(self->virt_viewer_obj, "username", con_data.user, NULL);

        //Если is_connect_to_prev_pool true, то подключение к пред. запомненому пулу,
        // минуя vdi manager window
        if (!con_data.is_connect_to_prev_pool) {
            // show VDI manager window
            if (self->vdi_manager == NULL)
                self->vdi_manager = vdi_manager_new();
            RemoteViewerState next_app_state = vdi_manager_dialog(self->vdi_manager, &con_data);
            if (next_app_state == APP_STATE_AUTH_DIALOG)
                goto retry_auth;
            else if (next_app_state == APP_STATE_EXITING)
                goto to_exit;
        }
        con_data.is_connect_to_prev_pool = FALSE; // reset the flag

        remote_viewer_vm_changed_notify(self, vdi_session_get_current_vm_id(), con_data.ip);
        // connect to vm depending remote protocol
        RemoteViewerState next_app_state = APP_STATE_VDI_DIALOG;
        if (vdi_session_get_current_remote_protocol() == VDI_RDP_PROTOCOL) {
            // Читаем из ini настройки remote_app только если они еще не установлены ранее (они могут быть получены от
            // RDS пула, например)
            rdp_settings_read_ini_file(&con_data.rdp_settings, !con_data.rdp_settings.is_remote_app);
            rdp_settings_set_connect_data(&con_data.rdp_settings, vdi_session_get_vdi_username(),
                            vdi_session_get_vdi_password(), con_data.domain, con_data.ip, 0);
            next_app_state = rdp_viewer_start(self, &con_data.rdp_settings);
#ifdef _WIN32
        }else if (vdi_session_get_current_remote_protocol() == VDI_RDP_WINDOWS_NATIVE_PROTOCOL) {
            rdp_settings_read_ini_file(&con_data.rdp_settings, !con_data.rdp_settings.is_remote_app);
            rdp_settings_set_connect_data(&con_data.rdp_settings, vdi_session_get_vdi_username(),
                                          vdi_session_get_vdi_password(), con_data.domain, con_data.ip, 0);
            launch_windows_rdp_client(&con_data.rdp_settings);
#endif
        } else { // spice by default
            set_spice_session_data(self->virt_viewer_obj, con_data.ip, con_data.port, con_data.user, con_data.password);
            virt_viewer_app_set_window_name(self->virt_viewer_obj, con_data.vm_verbose_name);
            virt_viewer_app_start_connect_attempts(self->virt_viewer_obj);
            next_app_state = virt_viewer_get_next_app_state(self->virt_viewer_obj);
        }

        remote_viewer_vm_changed_notify(self, NULL, NULL);
        if (next_app_state == APP_STATE_EXITING)
            goto to_exit;
        else if (next_app_state == APP_STATE_AUTH_DIALOG)
            goto retry_auth;
        else
            goto retry_connect_to_vm;
    }

to_exit:
    connect_settings_data_clear(&con_data);
}
