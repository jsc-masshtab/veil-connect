/*
 * Veil VDI thin client
 * Based on virt-viewer and freerdp
 *
 */

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

extern gboolean opt_manual_mode;


typedef enum
{
    AUTH_GUI_DEFAULT_STATE,
    AUTH_GUI_CONNECT_TRY_STATE

} AuthDialogState;

typedef struct
{
    RemoteViewer *p_remote_viewer;

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    // gui elements
    GtkWidget *login_entry;
    GtkWidget *password_entry;

    GtkWidget *window;
    GtkWidget *settings_button;
    GtkWidget *connect_button;
    GtkWidget *connect_spinner;
    GtkWidget *message_display_label;
    GtkWidget *header_label;
    GtkWidget *new_version_available_image;

    // remote viewer settings
    ConnectSettingsData *p_conn_data;

    // pointers to data
    gchar *current_pool_id;

    AuthDialogState auth_dialog_state;

} RemoteViewerConnData;

// set gui state
static void
set_auth_dialog_state(AuthDialogState auth_dialog_state, RemoteViewerConnData *ci)
{
    ci->auth_dialog_state = auth_dialog_state;

    switch (auth_dialog_state) {
    case AUTH_GUI_DEFAULT_STATE: {
        // stop connect spinner
        gtk_spinner_stop((GtkSpinner *)ci->connect_spinner);

        //// enable connect button if address_entry is not empty
        //if (gtk_entry_get_text_length(GTK_ENTRY(ci->address_entry)) > 0)
        gtk_widget_set_sensitive(GTK_WIDGET(ci->connect_button), TRUE);

        break;
    }
    case AUTH_GUI_CONNECT_TRY_STATE: {
        // clear message display
        gtk_label_set_text(GTK_LABEL(ci->message_display_label), " ");

        // disable connect button
        gtk_widget_set_sensitive(GTK_WIDGET(ci->connect_button), FALSE);

        // start connect spinner
        gtk_spinner_start((GtkSpinner *)ci->connect_spinner);
        break;
    }
    default: {
        break;
    }
    }
}
// header-label
static void
set_data_from_gui_in_outer_pointers(RemoteViewerConnData *ci)
{
    update_string_safely(&ci->p_conn_data->user, gtk_entry_get_text(GTK_ENTRY(ci->login_entry)));
    update_string_safely(&ci->p_conn_data->password, gtk_entry_get_text(GTK_ENTRY(ci->password_entry)));
    vdi_session_set_current_remote_protocol(ci->p_conn_data->remote_protocol_type);
}

// save data to ini file
static void
save_data_to_ini_file(RemoteViewerConnData *ci)
{
    const gchar *paramToFileGrpoup = get_cur_ini_param_group();
    write_str_to_ini_file(paramToFileGrpoup, "username", gtk_entry_get_text(GTK_ENTRY(ci->login_entry)));
    if (ci->p_conn_data->to_save_pswd)
        write_str_to_ini_file(paramToFileGrpoup, "password", gtk_entry_get_text(GTK_ENTRY(ci->password_entry)));
}

// set error message
static void
set_message_to_info_label(GtkLabel *label, const gchar *message)
{
    if (!message)
        return;

    const guint max_str_width = 120;

    gchar *message_str;

    const gchar *mask_string = "<span color=\"red\">%s</span>";
    // trim message
    if ( strlen(message) > max_str_width ) {
        gchar *trimmed_message = g_strndup(message, max_str_width);
        message_str = g_strdup_printf(mask_string, trimmed_message);
        g_free(trimmed_message);
    } else {
        message_str = g_strdup_printf(mask_string, message);
    }

    gtk_label_set_markup(label, message_str);
    g_free(message_str);
}

// get vm from pool callback
static void
on_vdi_session_get_vm_from_pool_finished(GObject *source_object G_GNUC_UNUSED,
                                         GAsyncResult *res,
                                         gpointer user_data G_GNUC_UNUSED)
{
    RemoteViewerConnData *ci = user_data;

    set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, ci);

    // GError *error = NULL;
    gpointer  ptr_res = g_task_propagate_pointer(G_TASK (res), NULL); // take ownership
    if (ptr_res == NULL) {
        set_message_to_info_label(GTK_LABEL(ci->message_display_label), "Не удалось получить вм из пула");
        return;
    }

    VdiVmData *vdi_vm_data = ptr_res;

    if (vdi_vm_data->server_reply_type != SERVER_REPLY_TYPE_DATA) {
        const gchar *user_message = vdi_vm_data->message ? vdi_vm_data->message : "Не удалось получить вм из пула";
        set_message_to_info_label(GTK_LABEL(ci->message_display_label), user_message);

    } else {
        ci->dialog_window_response = GTK_RESPONSE_OK;

        update_string_safely(&ci->p_conn_data->ip, vdi_vm_data->vm_host);
        ci->p_conn_data->port = vdi_vm_data->vm_port;
        free_memory_safely(&ci->p_conn_data->user);
        update_string_safely(&ci->p_conn_data->password, vdi_vm_data->vm_password);
        update_string_safely(&ci->p_conn_data->vm_verbose_name, vdi_vm_data->vm_verbose_name);

        shutdown_loop(ci->loop);
    }

    vdi_api_session_free_vdi_vm_data(vdi_vm_data);
}

// token fetch callback
static void
on_vdi_session_log_in_finished(GObject *source_object G_GNUC_UNUSED,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
    RemoteViewerConnData *ci = user_data;

    //GError *error = NULL;
    gboolean token_refreshed = g_task_propagate_boolean(G_TASK(res), NULL); // &error
    g_info("%s: is_token_refreshed %i", (const char *)__func__, token_refreshed);

    set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, ci);

    if (token_refreshed) {
        ci->dialog_window_response = GTK_RESPONSE_OK;
        set_data_from_gui_in_outer_pointers(ci);

        shutdown_loop(ci->loop);
    } else {
        set_message_to_info_label(GTK_LABEL(ci->message_display_label), "Не удалось авторизоваться");
    }
}

// connect to VDI server
void connect_to_vdi_server(RemoteViewerConnData *ci)
{
    if (ci->auth_dialog_state == AUTH_GUI_CONNECT_TRY_STATE)
        return;

    // set credential for connection to VDI server
    set_data_from_gui_in_outer_pointers(ci);
    vdi_session_set_credentials(ci->p_conn_data->user, ci->p_conn_data->password,
                                ci->p_conn_data->ip, ci->p_conn_data->port, ci->p_conn_data->is_ldap);

    set_auth_dialog_state(AUTH_GUI_CONNECT_TRY_STATE, ci);

    // 2 варианта: подключиться к сразу к предыдущему пулу, либо перейти к vdi менеджеру для выбора пула
    if (ci->p_conn_data->is_connect_to_prev_pool) {

        // get pool id from settings file
        gchar *last_pool_id = read_str_from_ini_file("RemoteViewerConnect", "last_pool_id");
        if (!last_pool_id) {
            set_message_to_info_label(GTK_LABEL(ci->message_display_label), "Нет информации о предыдущем пуле");
            set_auth_dialog_state(AUTH_GUI_DEFAULT_STATE, ci);
            return;
        }

        set_message_to_info_label(GTK_LABEL(ci->message_display_label), "Автоподключение к предыдущему пулу");
        vdi_session_set_current_pool_id(last_pool_id);
        free_memory_safely(&last_pool_id);

        VdiVmRemoteProtocol remote_protocol = read_int_from_ini_file("General",
                "cur_remote_protocol_index", VDI_SPICE_PROTOCOL);
        vdi_session_set_current_remote_protocol(remote_protocol);

        // start async task  vdi_session_get_vm_from_pool_task
        execute_async_task(vdi_session_get_vm_from_pool_task, on_vdi_session_get_vm_from_pool_finished, NULL, ci);
    } else {
        // fetch token task starting
        execute_async_task(vdi_session_log_in_task, on_vdi_session_log_in_finished, NULL, ci);
    }
}

static void
handle_connect_event(RemoteViewerConnData *ci)
{
    if (strlen_safely(ci->p_conn_data->ip) > 0) {
        // In manual mode we shudown the loop.
        if (opt_manual_mode) {
            ci->dialog_window_response = GTK_RESPONSE_OK;
            set_data_from_gui_in_outer_pointers(ci);

            shutdown_loop(ci->loop);
        } else {
            connect_to_vdi_server(ci);
        }
    } else {
        set_message_to_info_label(GTK_LABEL(ci->message_display_label), "Не указан адрес (Настройки->Основные)");
    }
}

static gboolean
window_deleted_cb(RemoteViewerConnData *ci)
{
    ci->dialog_window_response = GTK_RESPONSE_CLOSE;
    shutdown_loop(ci->loop);
    return TRUE;
}

static gboolean
key_pressed_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event, gpointer data)
{
    RemoteViewerConnData *ci = data;
    GtkWidget *window = ci->window;
    gboolean retval;
    if (event->type == GDK_KEY_PRESS) {
        //g_info("GDK_KEY_PRESS event->key.keyval %i \n", event->key.keyval);
        switch (event->key.keyval) {
            case GDK_KEY_Return:
                g_info("GDK_KEY_Return\n");
                handle_connect_event(ci);
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
    RemoteViewerConnData *ci = data;
    GtkResponseType res = remote_viewer_start_settings_dialog(ci->p_remote_viewer, ci->p_conn_data,
                                                              GTK_WINDOW(ci->window));
    (void)res;
}

static void
connect_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    RemoteViewerConnData *ci = data;
    handle_connect_event(ci);
}

static void
read_data_from_ini_file(RemoteViewerConnData *ci)
{
    // set params save group
    const gchar *paramToFileGrpoup = get_cur_ini_param_group();

    //password
    gchar *password_from_settings_file = read_str_from_ini_file(paramToFileGrpoup, "password");
    if (password_from_settings_file) {
        gtk_entry_set_text(GTK_ENTRY(ci->password_entry), password_from_settings_file);
        free_memory_safely(&password_from_settings_file);
    }
    // login
    gchar *user_from_settings_file = read_str_from_ini_file(paramToFileGrpoup, "username");
    if (user_from_settings_file) {
        gtk_entry_set_text(GTK_ENTRY(ci->login_entry), user_from_settings_file);
        free_memory_safely(&user_from_settings_file);
    }

    fill_p_conn_data_from_ini_file(ci->p_conn_data);
}

// В этом ёба режиме сразу автоматом пытаемся подрубиться к предыдущему пулу не дожидаясь действий пользователя.
// Поступаем так только один раз при старте приложения, чтоб у пользователя была возможносмть сменить
// логин пароль
static void fast_forward_connect_to_prev_pool_if_enabled(RemoteViewerConnData *ci)
{
    gboolean is_fastforward_conn_to_prev_pool =
            read_int_from_ini_file("RemoteViewerConnect", "is_conn_to_prev_pool_btn_checked", 0);

    static gboolean is_first_time = TRUE;
    if (is_fastforward_conn_to_prev_pool && is_first_time && !opt_manual_mode) {
        connect_to_vdi_server(ci);
        is_first_time = FALSE;
    }
}

static void remote_viewer_on_updates_checked(gpointer data G_GNUC_UNUSED, int is_available,
                                             RemoteViewerConnData *ci)
{
    g_info("%s  is_available %i", (const char *)__func__, is_available);

    if (is_available) {
        gtk_image_set_from_icon_name(GTK_IMAGE(ci->new_version_available_image),
                                     "gtk-dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_widget_set_tooltip_text(ci->new_version_available_image,
            "Доступна новая версия. Чтобы обновиться нажмите Настройки->Служебные->Получить обновления.");
    }
}

/**
* remote_viewer_connect_dialog
*
* @brief Opens connect dialog for remote viewer
*/
GtkResponseType
remote_viewer_connect_dialog(RemoteViewer *remote_viewer, ConnectSettingsData *connect_settings_data)
{
    GtkBuilder *builder;

    RemoteViewerConnData ci;
    memset(&ci, 0, sizeof(RemoteViewerConnData));
    ci.dialog_window_response = GTK_RESPONSE_CLOSE;

    // save pointers
    ci.p_remote_viewer = remote_viewer;
    ci.p_conn_data = connect_settings_data;

    /* Create the widgets */
    builder = remote_viewer_util_load_ui("remote-viewer-connect_veil.ui");
    g_return_val_if_fail(builder != NULL, GTK_RESPONSE_NONE);

    ci.window = GTK_WIDGET(gtk_builder_get_object(builder, "remote-viewer-connection-window"));

    ci.settings_button = GTK_WIDGET(gtk_builder_get_object(builder, "btn_settings"));
    ci.connect_button = GTK_WIDGET(gtk_builder_get_object(builder, "connect-button"));
    ci.connect_spinner = GTK_WIDGET(gtk_builder_get_object(builder, "connect-spinner"));
    ci.message_display_label = GTK_WIDGET(gtk_builder_get_object(builder, "message-display-label"));
    ci.header_label = GTK_WIDGET(gtk_builder_get_object(builder, "header-label"));
    gtk_label_set_text(GTK_LABEL(ci.header_label), VERSION);
    ci.new_version_available_image = GTK_WIDGET(gtk_builder_get_object(builder, "new-version-available-image"));

    // password entry
    ci.password_entry = GTK_WIDGET(gtk_builder_get_object(builder, "password-entry"));
    // login entry
    ci.login_entry = GTK_WIDGET(gtk_builder_get_object(builder, "login-entry"));

    // Signal - callbacks connections
    g_signal_connect(ci.window, "key-press-event", G_CALLBACK(key_pressed_cb), &ci);
    g_signal_connect_swapped(ci.window, "delete-event", G_CALLBACK(window_deleted_cb), &ci);
    g_signal_connect(ci.settings_button, "clicked", G_CALLBACK(settings_button_clicked_cb), &ci);
    g_signal_connect(ci.connect_button, "clicked", G_CALLBACK(connect_button_clicked_cb), &ci);
    gulong updates_checked_handle = g_signal_connect(remote_viewer->app_updater, "updates-checked",
                                          G_CALLBACK(remote_viewer_on_updates_checked), &ci);

    // read ini file
    read_data_from_ini_file(&ci);

    /* show and wait for response */
    gtk_window_set_position(GTK_WINDOW(ci.window), GTK_WIN_POS_CENTER);
    //gtk_window_resize(GTK_WINDOW(window), 340, 340);

    gtk_widget_show_all(ci.window);
    gtk_window_set_resizable(GTK_WINDOW(ci.window), FALSE);

    // check if there is a new version
    app_updater_execute_task_check_updates(remote_viewer->app_updater);

    // connect to the prev pool if requred
    fast_forward_connect_to_prev_pool_if_enabled(&ci);

    create_loop_and_launch(&ci.loop);

    // save data to ini file if required
    save_data_to_ini_file(&ci);

    // disconnect signals
    g_signal_handler_disconnect(remote_viewer->app_updater, updates_checked_handle);

    g_object_unref(builder);
    gtk_widget_destroy(ci.window);

    return ci.dialog_window_response;
}