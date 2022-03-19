/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <freerdp/version.h>

#include <config.h>

#include "settingsfile.h"
#include "remote-viewer-connect.h"
#include "remote-viewer-util.h"
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <ctype.h>

#include "async.h"
#include "jsonhandler.h"
#include "remote_viewer_start_settings.h"
#include "usbredir_dialog.h"
#include "controller_client/controller_session.h"
#if defined(_WIN32)
#include "local_ipc/win_service_ipc.h"
#endif


G_DEFINE_TYPE( RemoteViewerConnect, remote_viewer_connect, G_TYPE_OBJECT )

// set gui state
static void
set_auth_dialog_state(AuthDialogState auth_dialog_state, RemoteViewerConnect *self)
{
    self->auth_dialog_state = auth_dialog_state;

    switch (auth_dialog_state) {
    case AUTH_GUI_DEFAULT_STATE: {
        // stop connect spinner
        gtk_spinner_stop(GTK_SPINNER(self->connect_spinner));
        gtk_widget_set_visible(GTK_WIDGET(self->btn_cancel_auth), FALSE);

        //// enable connect button if address_entry is not empty
        //if (gtk_entry_get_text_length(GTK_ENTRY(self->address_entry)) > 0)
        gtk_widget_set_sensitive(GTK_WIDGET(self->connect_button), TRUE);

        break;
    }
    case AUTH_GUI_CONNECT_TRY_STATE: {
        // clear message display
        gtk_label_set_text(GTK_LABEL(self->message_display_label), " ");

        // disable connect button
        gtk_widget_set_sensitive(GTK_WIDGET(self->connect_button), FALSE);

        // start connect spinner
        gtk_spinner_start(GTK_SPINNER(self->connect_spinner));
        gtk_widget_set_visible(GTK_WIDGET(self->btn_cancel_auth), TRUE);
        break;
    }
    default: {
        break;
    }
    }
}

static void remote_viewer_connect_finish_job(RemoteViewerConnect *self)
{
    g_info("%s", (const char *)__func__);
    if (!self->is_active)
        return;

    // interupt settings dialog loop
    shutdown_loop(self->connect_settings_dialog.loop);

    // forget password if required
    if (!self->p_conn_data->to_save_pswd)
        free_memory_safely(&self->p_conn_data->password);

    // save data to ini file
    settings_data_save_all(self->p_conn_data);

    // disconnect signals
    g_signal_handler_disconnect(self->p_app_updater, self->updates_checked_handle);

    g_object_unref(self->builder);
    gtk_widget_destroy(self->window);

    self->is_active = FALSE;
}

static void
take_from_gui(RemoteViewerConnect *self)
{
    const gchar *login = gtk_entry_get_text(GTK_ENTRY(self->login_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(self->password_entry));
    const gchar *disposable_password = gtk_entry_get_text(GTK_ENTRY(self->disposable_password_entry));
    gboolean is_ldap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->ldap_check_btn));

    switch (self->p_conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            vdi_session_set_credentials(login, password, disposable_password);
            vdi_session_set_ldap(is_ldap);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            update_string_safely(&self->p_conn_data->user, login);
            strstrip_safely(self->p_conn_data->user);
            update_string_safely(&self->p_conn_data->password, password);
            strstrip_safely(self->p_conn_data->password);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            controller_session_set_credentials(login, password);
            controller_session_set_ldap(is_ldap);
            break;
        }
    }
}

static void
fill_gui(RemoteViewerConnect *self)
{
    switch (self->p_conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            if (vdi_session_get_vdi_username())
                gtk_entry_set_text(GTK_ENTRY(self->login_entry), vdi_session_get_vdi_username());
            if (vdi_session_get_vdi_password())
                gtk_entry_set_text(GTK_ENTRY(self->password_entry), vdi_session_get_vdi_password());

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->ldap_check_btn), vdi_session_is_ldap());
            gtk_widget_set_sensitive(self->ldap_check_btn, TRUE);

            gtk_widget_set_visible(self->global_application_mode_image, FALSE);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            if (self->p_conn_data->user)
                gtk_entry_set_text(GTK_ENTRY(self->login_entry), self->p_conn_data->user);
            if (self->p_conn_data->password)
                gtk_entry_set_text(GTK_ENTRY(self->password_entry), self->p_conn_data->password);

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->ldap_check_btn), FALSE);
            gtk_widget_set_sensitive(self->ldap_check_btn, FALSE);

            gtk_widget_set_visible(self->global_application_mode_image, TRUE);
            gtk_widget_set_tooltip_text(self->global_application_mode_image, _("VM connection mode"));
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            if (get_controller_session_static()->username.string)
                gtk_entry_set_text(GTK_ENTRY(self->login_entry), get_controller_session_static()->username.string);
            if (get_controller_session_static()->password.string)
                gtk_entry_set_text(GTK_ENTRY(self->password_entry), get_controller_session_static()->password.string);

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->ldap_check_btn),
                                         get_controller_session_static()->is_ldap);
            gtk_widget_set_sensitive(self->ldap_check_btn, TRUE);

            gtk_widget_set_visible(self->global_application_mode_image, TRUE);
            gtk_widget_set_tooltip_text(self->global_application_mode_image, _("Controller connection mode"));
            break;
        }
    }
}

// token fetch callback (vdi)
static void
on_vdi_session_log_in_finished(GObject *source_object G_GNUC_UNUSED,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
    RemoteViewerConnect *self = user_data;

    set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, self);

    LoginData *login_data = g_task_propagate_pointer(G_TASK(res), NULL);
    if (login_data->reply_msg)
        g_info("%s: is_token_refreshed %s", (const char *)__func__, login_data->reply_msg);

    g_autofree gchar *token = NULL;
    token = vdi_session_get_token();
    if (token) {
        // update domain
        update_string_safely(&self->p_conn_data->domain, login_data->domain);

        vdi_session_refresh_login_time();
        take_from_gui(self);
        remote_viewer_connect_finish_job(self);
        g_signal_emit_by_name(self, "show-vm-selector-requested");
    } else {
        util_set_message_to_info_label(GTK_LABEL(self->message_display_label), login_data->reply_msg);
    }

    vdi_api_session_free_login_data(login_data);
}

// token fetch callback (controller)
static void
on_controller_session_log_in_finished(GObject *source_object G_GNUC_UNUSED,
                               GAsyncResult *res,
                               gpointer user_data)
{
    RemoteViewerConnect *self = user_data;

    set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, self);

    gboolean is_ok = g_task_propagate_boolean(G_TASK(res), NULL);

    if (is_ok) {
        take_from_gui(self);
        remote_viewer_connect_finish_job(self);
        g_signal_emit_by_name(self, "show-vm-selector-requested");
    } else {
        g_autofree gchar *last_error_phrase = NULL;
        last_error_phrase = atomic_string_get(&get_controller_session_static()->last_error_phrase);
        util_set_message_to_info_label(GTK_LABEL(self->message_display_label), last_error_phrase);
    }
}

// connect to VDI server
static void connect_to_vdi_server(RemoteViewerConnect *self)
{
    vdi_session_get_ws_client()->is_connect_initiated_by_user = TRUE;

    // set credential for connection to VDI server
    take_from_gui(self);
    set_auth_dialog_state(AUTH_GUI_CONNECT_TRY_STATE, self);

    //start token fetching task
    execute_async_task(vdi_session_log_in_task, on_vdi_session_log_in_finished, NULL, self);
}

// connect to controller (ECP)
static void connect_to_controller(RemoteViewerConnect *self)
{
    if (self->auth_dialog_state == AUTH_GUI_CONNECT_TRY_STATE)
        return;

    // set credential for connection to controller
    take_from_gui(self);
    set_auth_dialog_state(AUTH_GUI_CONNECT_TRY_STATE, self);

    //start token fetching task
    execute_async_task(controller_session_log_in_task, on_controller_session_log_in_finished, NULL, self);
}

static void
handle_connect_request(RemoteViewerConnect *self)
{
    switch (self->p_conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            if (strlen_safely(vdi_session_get_vdi_ip()) == 0) {
                util_set_message_to_info_label(GTK_LABEL(self->message_display_label),
                                          _("Connection address is not specified"));
                return;
            }

            if (self->auth_dialog_state == AUTH_GUI_CONNECT_TRY_STATE)
                return;

            connect_to_vdi_server(self);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            if (strlen_safely(self->p_conn_data->ip) == 0) {
                // "Не указан адрес подключения (Настройки->Основные)"
                util_set_message_to_info_label(GTK_LABEL(self->message_display_label),
                                          _("Connection address is not specified"));
                return;
            }

            take_from_gui(self);
            remote_viewer_connect_finish_job(self);
            g_signal_emit_by_name(self, "connect-to-vm-requested");
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            if (strlen_safely(get_controller_session_static()->address.string) == 0) {
                util_set_message_to_info_label(GTK_LABEL(self->message_display_label),
                                          _("Connection address is not specified"));
                return;
            }
            connect_to_controller(self);
            break;
        }
    }
}

static gboolean
window_deleted_cb(RemoteViewerConnect *self)
{
    remote_viewer_connect_finish_job(self);
    g_signal_emit_by_name(self, "quit-requested");
    return TRUE;
}

static gboolean
key_pressed_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event, gpointer data)
{
    RemoteViewerConnect *self = data;
    GtkWidget *window = self->window;
    gboolean retval;
    if (event->type == GDK_KEY_PRESS) {
        //g_info("GDK_KEY_PRESS event->key.keyval %i \n", event->key.keyval);
        switch (event->key.keyval) {
            case GDK_KEY_Return:
                g_info("GDK_KEY_Return\n");
                handle_connect_request(self);
                return TRUE;
            case GDK_KEY_Escape:
                g_signal_emit_by_name(window, "delete-event", NULL, &retval);
                return TRUE;
            default:
                return FALSE;
        }
    }

    return FALSE;
}

static void
settings_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    RemoteViewerConnect *self = data;
    GtkResponseType res = remote_viewer_start_settings_dialog(&self->connect_settings_dialog,
                                                              self->p_conn_data,
                                                              self->p_app_updater,
                                                              GTK_WINDOW(self->window));
    // update gui state
    if (res == GTK_RESPONSE_OK)
        fill_gui(self);
}

static void
on_check_btn_2fa_password_toggled(GtkToggleButton *button, gpointer data)
{
    RemoteViewerConnect *self = data;
    gboolean is_active = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(self->disposable_password_entry, is_active);
    gtk_entry_set_text(GTK_ENTRY(self->disposable_password_entry), "");
}

static void
connect_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    RemoteViewerConnect *self = (RemoteViewerConnect *)data;
    handle_connect_request(self);
}

static void
btn_cancel_auth_clicked_cb(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    RemoteViewerConnect *self = (RemoteViewerConnect *)data;

    switch (self->p_conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            vdi_session_cancel_pending_requests();
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            controller_session_cancel_pending_requests();
            break;
        }
        default:
            break;
    }
}

static void remote_viewer_on_updates_checked(gpointer data G_GNUC_UNUSED, int is_available,
                                             RemoteViewerConnect *self)
{
    g_info("%s  is_available %i", (const char *)__func__, is_available);

    if (is_available) {
        gtk_image_set_from_stock(GTK_IMAGE(self->new_version_available_image),
                                     "gtk-dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR);
        // Доступна новая версия. Чтобы обновиться нажмите Настройки->Служебные->Получить обновления.
        gtk_widget_set_tooltip_text(self->new_version_available_image,
            _("A new version available. Press Settings->Service->Get updates"));
    }
}

static void remote_viewer_connect_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( remote_viewer_connect_parent_class );
    ( *parent_class->finalize )( object );
}

static void remote_viewer_connect_class_init(RemoteViewerConnectClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = remote_viewer_connect_finalize;

    // signals
    g_signal_new("show-vm-selector-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RemoteViewerConnectClass, show_vm_selector_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("connect-to-vm-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RemoteViewerConnectClass, connect_to_vm_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("quit-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RemoteViewerConnectClass, quit_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void remote_viewer_connect_init(RemoteViewerConnect *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

RemoteViewerConnect *remote_viewer_connect_new()
{
    RemoteViewerConnect *obj = REMOTE_VIEWER_CONNECT( g_object_new( TYPE_REMOTE_VIEWER_CONNECT, NULL ) );
    return obj;
}

#if defined(_WIN32)
static void on_shared_memory_ipc_get_logon_data_finished(GObject *source_object G_GNUC_UNUSED,
        GAsyncResult *res, gpointer user_data) {

    RemoteViewerConnect *self = user_data;

    // Get system logon data and update GUI
    SharedMemoryIpcLogonData *shared_memory_ipc_logon_data = g_task_propagate_pointer(G_TASK(res), NULL);

    if (shared_memory_ipc_logon_data->login) {
        gtk_entry_set_text(GTK_ENTRY(self->login_entry), shared_memory_ipc_logon_data->login);
        gtk_entry_set_text(GTK_ENTRY(self->password_entry), shared_memory_ipc_logon_data->password);
        connect_to_vdi_server(self);
    } else {
        util_set_message_to_info_label(GTK_LABEL(self->message_display_label),
                                       _("Failed to fetch system credentials"));
        set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, self);
    }

    shared_memory_ipc_logon_data_free(shared_memory_ipc_logon_data);
}
#endif

// Path through auth or auto connect to prev pool
static void fast_forward_connect_if_enabled(RemoteViewerConnect *self)
{
    if (self->p_conn_data->global_app_mode == GLOBAL_APP_MODE_VDI) {
#if defined(_WIN32)
        if (self->p_conn_data->pass_through_auth && self->p_conn_data->not_pass_through_authenticated_yet) {
            self->p_conn_data->not_pass_through_authenticated_yet = FALSE;
            set_auth_dialog_state(AUTH_GUI_CONNECT_TRY_STATE, self);
            execute_async_task(shared_memory_ipc_get_logon_data,
                               on_shared_memory_ipc_get_logon_data_finished, NULL, self);
            return;
        }
#endif
        if (self->p_conn_data->connect_to_prev_pool && self->p_conn_data->not_connected_to_prev_pool_yet) {
            // connect to the prev pool if required
            connect_to_vdi_server(self);
        }
    }
}

/**
* remote_viewer_connect_show
*
* @brief Opens connect dialog for remote viewer
*/
void
remote_viewer_connect_show(RemoteViewerConnect *self, ConnectSettingsData *conn_data,
                           AppUpdater *app_updater)
{
    if (self->is_active)
        return;
    self->is_active = TRUE;

    // save pointers
    self->p_conn_data = conn_data;
    self->p_app_updater = app_updater;

    /* Create the widgets */
    GtkBuilder *builder = self->builder = remote_viewer_util_load_ui("remote-viewer-connect_veil.glade");

    self->window = GTK_WIDGET(gtk_builder_get_object(builder, "remote-viewer-connection-window"));

    self->settings_button = GTK_WIDGET(gtk_builder_get_object(builder, "btn_settings"));
    self->connect_button = GTK_WIDGET(gtk_builder_get_object(builder, "connect-button"));
    self->btn_cancel_auth = GTK_WIDGET(gtk_builder_get_object(builder, "btn_cancel_auth"));
    self->connect_spinner = GTK_WIDGET(gtk_builder_get_object(builder, "connect-spinner"));
    self->message_display_label = GTK_WIDGET(gtk_builder_get_object(builder, "message-display-label"));
    gtk_label_set_selectable(GTK_LABEL(self->message_display_label), TRUE);
    self->header_label = GTK_WIDGET(gtk_builder_get_object(builder, "header-label"));
    g_autofree gchar *header_label_tooltip_text = NULL;
    header_label_tooltip_text = g_strdup_printf("Built with freerdp version: %s Application build date: %s %s",
            FREERDP_VERSION_FULL, __DATE__, __TIME__);
    gtk_widget_set_tooltip_text(self->header_label, header_label_tooltip_text);
    gtk_label_set_text(GTK_LABEL(self->header_label), VERSION);
    self->new_version_available_image = GTK_WIDGET(gtk_builder_get_object(builder, "new-version-available-image"));

    self->app_name_label = GTK_WIDGET(gtk_builder_get_object(builder, "app_name_label"));
    gchar *uppercase_name = g_utf8_strup(APPLICATION_NAME, -1);
    gtk_label_set_text(GTK_LABEL(self->app_name_label), uppercase_name);
    g_free(uppercase_name);

    // password entry
    self->password_entry = GTK_WIDGET(gtk_builder_get_object(builder, "password-entry"));
    // login entry
    self->login_entry = GTK_WIDGET(gtk_builder_get_object(builder, "login-entry"));
    // 2fa password
    self->disposable_password_entry = GTK_WIDGET(gtk_builder_get_object(builder, "disposable_password_entry"));
    self->check_btn_2fa_password = GTK_WIDGET(gtk_builder_get_object(builder, "check_btn_2fa_password"));
    self->ldap_check_btn = get_widget_from_builder(builder, "ldap_check_btn");

    self->global_application_mode_image = GTK_WIDGET(gtk_builder_get_object(builder, "global_application_mode_image"));

    // Signal - callbacks connections
    g_signal_connect(self->window, "key-press-event", G_CALLBACK(key_pressed_cb), self);
    g_signal_connect_swapped(self->window, "delete-event", G_CALLBACK(window_deleted_cb), self);
    g_signal_connect(self->settings_button, "clicked", G_CALLBACK(settings_button_clicked_cb), self);
    g_signal_connect(self->connect_button, "clicked", G_CALLBACK(connect_button_clicked_cb), self);
    g_signal_connect(self->btn_cancel_auth, "clicked", G_CALLBACK(btn_cancel_auth_clicked_cb), self);
    self->updates_checked_handle = g_signal_connect(self->p_app_updater, "updates-checked",
                                          G_CALLBACK(remote_viewer_on_updates_checked), self);
    g_signal_connect(self->check_btn_2fa_password, "toggled", G_CALLBACK(on_check_btn_2fa_password_toggled), self);

    set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, self);
    fill_gui(self);

    /* show and wait for response */
    gtk_window_set_position(GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);
    //gtk_window_resize(GTK_WINDOW(window), 340, 340);

    gtk_widget_show_all(self->window);
    gtk_window_set_resizable(GTK_WINDOW(self->window), FALSE);

    // check if there is a new version
    app_updater_execute_task_check_updates(self->p_app_updater);

    fast_forward_connect_if_enabled(self);
}

void remote_viewer_connect_raise(RemoteViewerConnect *self)
{
    if (!self->is_active)
        return;

    if (self->window && gtk_widget_get_visible(self->window))
        gtk_window_present(GTK_WINDOW(self->window));
}