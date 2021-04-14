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
    gboolean open_recent_dialog;
    GMainLoop *virt_viewer_loop;
};

G_DEFINE_TYPE (RemoteViewer, remote_viewer, VIRT_VIEWER_TYPE_APP)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), REMOTE_VIEWER_TYPE, RemoteViewerPrivate))

enum RemoteViewerProperties {
    PROP_0,
#ifdef HAVE_OVIRT
    PROP_OVIRT_FOREIGN_MENU,
#endif
};

#ifdef HAVE_OVIRT
#endif

static gboolean remote_viewer_start(VirtViewerApp *app, GError **error, RemoteViewerState remoteViewerState);
#ifdef HAVE_SPICE_GTK
static gboolean remote_viewer_activate(VirtViewerApp *app, GError **error);
static void remote_viewer_window_added(GtkApplication *app, GtkWindow *w);
#endif


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
    RemoteViewer *self = REMOTE_VIEWER(object);
    g_object_unref(self->app_updater);

    G_OBJECT_CLASS(remote_viewer_parent_class)->finalize(object);
}

static void
remote_viewer_deactivated(VirtViewerApp *app, gboolean connect_error)
{
    VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->deactivated(app, connect_error);
    if (virt_viewer_app_hide_windows_on_disconnect(app)) {
        // Закрыть окно virt-viewer и остановить луп
        virt_viewer_app_hide_all_windows_forced(app);
        shutdown_loop(REMOTE_VIEWER(app)->priv->virt_viewer_loop);
    }
    // Далее закрывать окна пока не будет явно указано обратное
    virt_viewer_app_set_hide_windows_on_disconnect(app, TRUE);
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
    GOptionGroup *group = g_option_group_new("virt-viewer", NULL, NULL, gapp, NULL);

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
    GtkApplicationClass *gtk_app_class = GTK_APPLICATION_CLASS(klass);
    VirtViewerAppClass *app_class = VIRT_VIEWER_APP_CLASS (klass);
    GApplicationClass *g_app_class = G_APPLICATION_CLASS(klass);

    g_type_class_add_private (klass, sizeof (RemoteViewerPrivate));

    object_class->dispose = remote_viewer_dispose;
    object_class->finalize = remote_viewer_finalize;

    g_app_class->local_command_line = remote_viewer_local_command_line;

    app_class->start = remote_viewer_start;
    app_class->deactivated = remote_viewer_deactivated;
#ifdef HAVE_SPICE_GTK
    app_class->activate = remote_viewer_activate;
    gtk_app_class->window_added = remote_viewer_window_added;
#else
    (void) gtk_app_class;
#endif

#ifdef HAVE_OVIRT
    g_object_class_install_property(object_class,
                                    PROP_OVIRT_FOREIGN_MENU,
                                    g_param_spec_object("ovirt-foreign-menu",
                                                        "oVirt Foreign Menu",
                                                        "Object which is used as interface to oVirt",
                                                        OVIRT_TYPE_FOREIGN_MENU,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
#endif
}

static void
remote_viewer_init(RemoteViewer *self)
{
    g_info("%s", (const char*)__func__);
    self->priv = GET_PRIVATE(self);

    self->net_speedometer = net_speedometer_new();
    net_speedometer_set_pointer_to_virt_viewer_app(self->net_speedometer, VIRT_VIEWER_APP(self));

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
    g_object_unref(self->net_speedometer);
    g_object_unref(self->app_updater);
    if (self->vdi_manager)
        g_object_unref(self->vdi_manager);

    if (self->veil_messenger)
        g_object_unref(self->veil_messenger);

    usbredir_controller_deinit_static();
    vdi_session_static_destroy();
}

static gboolean
remote_viewer_activate(VirtViewerApp *app, GError **error)
{
    gboolean ret = FALSE;

    g_return_val_if_fail(REMOTE_VIEWER_IS(app), FALSE);

    ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->activate(app, error);
    return ret;
}

static void
remote_viewer_window_added(GtkApplication *app,
                           GtkWindow *w)
{
    GTK_APPLICATION_CLASS(remote_viewer_parent_class)->window_added(app, w);
}

static void
remote_viewer_recent_add(gchar *uri, const gchar *mime_type)
{
    GtkRecentManager *recent;
    GtkRecentData meta = {
        .app_name     = (char*)"remote-viewer",
        .app_exec     = (char*)"remote-viewer %u",
        .mime_type    = (char*)mime_type,
    };

    if (uri == NULL)
        return;

    recent = gtk_recent_manager_get_default();
    meta.display_name = uri;
    if (!gtk_recent_manager_add_full(recent, uri, &meta))
        g_warning("Recent item couldn't be added");
}

static void
remote_viewer_session_connected(VirtViewerSession *session,
                                VirtViewerApp *self G_GNUC_UNUSED)
{
    gchar *uri = virt_viewer_session_get_uri(session);
    const gchar *mime = virt_viewer_session_mime_type(session);

    remote_viewer_recent_add(uri, mime);
    g_free(uri);
}

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

static GtkCssProvider * setup_css()
{
    GtkCssProvider *cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "css_style.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(cssProvider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
    return cssProvider;
}

static void connect_settings_data_free(ConnectSettingsData *connect_settings_data)
{
    g_info("%s", (const char *)__func__);
    free_memory_safely(&connect_settings_data->user);
    free_memory_safely(&connect_settings_data->password);
    free_memory_safely(&connect_settings_data->domain);
    free_memory_safely(&connect_settings_data->ip);
    free_memory_safely(&connect_settings_data->vm_verbose_name);
}

static void remote_viewer_vm_changed_notify(RemoteViewer *self, const gchar *vm_id, const gchar *vm_ip)
{
    vdi_ws_client_send_vm_changed(vdi_session_get_ws_client(), vm_id);
    net_speedometer_update_vm_ip(self->net_speedometer, vm_ip);
}

static gboolean
remote_viewer_start(VirtViewerApp *app, GError **err G_GNUC_UNUSED, RemoteViewerState remoteViewerState G_GNUC_UNUSED)
{
    g_return_val_if_fail(REMOTE_VIEWER_IS(app), FALSE);
    GError *error = NULL;
    ConnectSettingsData con_data = {};
    GtkCssProvider *css_provider = setup_css(); // CSS setup
    //create veil messenger
    REMOTE_VIEWER(app)->veil_messenger = veil_messenger_new();
    // remote connect dialog
retry_auth:
    {
        veil_messenger_hide(REMOTE_VIEWER(app)->veil_messenger);
        // Забираем из ui адрес и порт
        GtkResponseType dialog_window_response = remote_viewer_connect_dialog(REMOTE_VIEWER(app), &con_data);
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
            RemoteViewerState app_state = rdp_viewer_start(REMOTE_VIEWER(app),
                    con_data.user, con_data.password, con_data.domain, con_data.ip, con_data.port);
            if (app_state == APP_STATE_AUTH_DIALOG)
                goto retry_auth;
            else if (app_state == APP_STATE_EXITING)
                goto to_exit;

        } else { // spice by default
        set_spice_session_data(app, con_data.ip, con_data.port, con_data.user, con_data.password);
        // Создание сессии
        if (!virt_viewer_app_create_session(app, "spice", &error)) {
            virt_viewer_app_simple_message_dialog(app, _("Unable to connect: %s"), error->message);
            goto to_exit;
        }
        g_signal_connect(virt_viewer_app_get_session(app), "session-connected",
                         G_CALLBACK(remote_viewer_session_connected), app);

        // Коннект к машине/*
        if (!virt_viewer_app_initial_connect(app, &error)) {
            if (error == NULL) {
                g_set_error_literal(&error, VIRT_VIEWER_ERROR, VIRT_VIEWER_ERROR_FAILED,
                                    _("Failed to initiate connection"));
            }

            virt_viewer_app_simple_message_dialog(app, _("Unable to connect: %s"), error->message);
            goto to_exit;
        }

        virt_viewer_app_set_hide_windows_on_disconnect(app, TRUE);
        VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->start(app, NULL, APP_STATE_AUTH_DIALOG);
        create_loop_and_launch(&REMOTE_VIEWER(app)->priv->virt_viewer_loop);

        // go back to auth or quit
        if (virt_viewer_app_is_quitting(app))
            goto to_exit;
        else
            goto retry_auth;
    }
    /// VDI connect mode
    } else {
        // remember username
        if (con_data.user)
            g_object_set(app, "username", con_data.user, NULL);

        //Если is_connect_to_prev_pool true, то подключение к пред. запомненому пулу,
        // минуя vdi manager window
        if (!con_data.is_connect_to_prev_pool) {
            // show VDI manager window
            if (REMOTE_VIEWER(app)->vdi_manager == NULL)
                REMOTE_VIEWER(app)->vdi_manager = vdi_manager_new();
            RemoteViewerState next_app_state = vdi_manager_dialog(REMOTE_VIEWER(app)->vdi_manager, &con_data);
            if (next_app_state == APP_STATE_AUTH_DIALOG)
                goto retry_auth;
            else if (next_app_state == APP_STATE_EXITING)
                goto to_exit;
        }
        con_data.is_connect_to_prev_pool = FALSE;

        remote_viewer_vm_changed_notify(REMOTE_VIEWER(app), vdi_session_get_current_vm_id(), con_data.ip);
        // connect to vm depending using remote protocol
        RemoteViewerState next_app_state = APP_STATE_VDI_DIALOG;
        if (vdi_session_get_current_remote_protocol() == VDI_RDP_PROTOCOL) {
            next_app_state = rdp_viewer_start(REMOTE_VIEWER(app), vdi_session_get_vdi_username(),
                    vdi_session_get_vdi_password(), con_data.domain, con_data.ip, 0);
#ifdef _WIN32
        }else if (vdi_session_get_current_remote_protocol() == VDI_RDP_WINDOWS_NATIVE_PROTOCOL) {
                launch_windows_rdp_client(vdi_session_get_vdi_username(), vdi_session_get_vdi_password(),
                        con_data.ip, 0, con_data.domain);
#endif
        } else { // spice by default
            virt_viewer_app_set_window_name(app, con_data.vm_verbose_name);
            set_spice_session_data(app, con_data.ip, con_data.port, con_data.user, con_data.password);
            // start connect attempt timer
            virt_viewer_app_set_hide_windows_on_disconnect(app, TRUE);
            virt_viewer_app_start_reconnect_poll(app);
            // Показывается окно virt viewer // virt_viewer_app_default_start
            VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->start(app, NULL, APP_STATE_AUTH_DIALOG);
            create_loop_and_launch(&REMOTE_VIEWER(app)->priv->virt_viewer_loop);
            next_app_state = virt_viewer_get_next_app_state(app);
        }

        remote_viewer_vm_changed_notify(REMOTE_VIEWER(app), NULL, NULL);
        if (next_app_state == APP_STATE_EXITING)
            goto to_exit;
        else if (next_app_state == APP_STATE_AUTH_DIALOG)
            goto retry_auth;
        else
            goto retry_connect_to_vm;
    }

to_exit:
    connect_settings_data_free(&con_data);
    g_clear_error(&error);
    g_object_unref(css_provider);
    return FALSE;
}
