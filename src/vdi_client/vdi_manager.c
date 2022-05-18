/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <time.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <cairo-features.h>
#include <libsoup/soup-session.h>
#include <glib/gi18n.h>

#include "vdi_manager.h"
#include "vdi_app_selector.h"
#include "remote-viewer-util.h"
#include "vdi_ws_client.h"
#include "vdi_pool_widget.h"
#include "jsonhandler.h"
#include "settingsfile.h"
#include "vdi_user_settings_widget.h"
#include "native_rdp_launcher.h"

#define MAX_POOL_NUMBER 150
#define MSG_TRIM_LENGTH 140

G_DEFINE_TYPE( VdiManager, vdi_manager, G_TYPE_OBJECT )


typedef enum
{
    VDI_RECEIVED_RESPONSE,
    VDI_WAITING_FOR_POOL_DATA,
    VDI_WAITING_FOR_VM_FROM_POOL
} VdiClientState;


// functions declarations
static void on_vm_prep_progress_received(gpointer data, int request_id, int progress, const gchar *text,
        VdiManager *self);
static void set_vdi_client_state(VdiManager *self, VdiClientState vdi_client_state,
        const gchar *message, gboolean is_error_message);
static void refresh_vdi_pool_data_async(VdiManager *self);
static void unregister_all_pools(VdiManager *self);
static void register_pool(VdiManager *self, const gchar *pool_id, const gchar *pool_name, const gchar *os_type,
        const gchar *status, JsonArray *conn_types_json_array, gboolean favorite);
static VdiPoolWidget *get_vdi_pool_widget_by_id(VdiManager *self, const gchar *searched_id);

static void on_vdi_session_get_vdi_pool_data_finished(GObject *source_object, GAsyncResult *res, VdiManager *self);
static void on_vdi_session_get_vm_from_pool_finished(GObject *source_object, GAsyncResult *res, VdiManager *self);
static void set_ws_conn_state(VdiManager *self, gboolean is_vdi_online);

static gboolean on_window_deleted_cb(VdiManager *self);
static void on_button_renew_clicked(GtkButton *button, VdiManager *self);
static void on_button_quit_clicked(GtkButton *button, VdiManager *self);
static gboolean on_vm_start_button_released(GtkButton *button, GdkEventButton *event, VdiPoolWidget *vdi_pool_widget);

static void vdi_manager_finalize(GObject *object);

/////////////////////////////////// work functions//////////////////////////////////////

static void vdi_manager_set_msg(VdiManager *self, const gchar *message, gboolean is_error_message)
{
    if (!self->status_label)
        return;

    // message
    g_autofree gchar *trimmed_msg = NULL;
    trimmed_msg = g_strndup(message, MSG_TRIM_LENGTH);

    if (is_error_message)
        gtk_widget_set_name(self->status_label, "widget_with_red_text");
    else
        gtk_widget_set_name(self->status_label, "status_label");

    gtk_label_set_text(GTK_LABEL(self->status_label), trimmed_msg);

    // tooltip
    gtk_widget_set_tooltip_text(self->status_label, message);
}

// Set GUI state
static void set_vdi_client_state(VdiManager *self, VdiClientState vdi_client_state,
        const gchar *message, gboolean is_error_message)
{
    gboolean controls_blocked = FALSE;
    gboolean btn_cancel_sensitive = FALSE;

    switch (vdi_client_state) {
        case VDI_RECEIVED_RESPONSE: {
            if (self->main_vm_spinner)
                gtk_widget_hide (GTK_WIDGET(self->main_vm_spinner));
            controls_blocked = TRUE;
            break;
        }

        case VDI_WAITING_FOR_POOL_DATA: {
            if (self->main_vm_spinner)
                gtk_widget_show (GTK_WIDGET(self->main_vm_spinner));
            controls_blocked = FALSE;
            btn_cancel_sensitive = TRUE;
            break;
        }

        case VDI_WAITING_FOR_VM_FROM_POOL: {
            controls_blocked = FALSE;
            btn_cancel_sensitive = TRUE;
            break;
        }
        default:
            break;
    }

    // control widgets state
    if (self->gtk_flow_box)
        gtk_widget_set_sensitive(self->gtk_flow_box, controls_blocked);
    if (self->button_renew)
        gtk_widget_set_sensitive(self->button_renew, controls_blocked);
    if (self->button_quit)
        gtk_widget_set_sensitive(self->button_quit, controls_blocked);
    gtk_widget_set_sensitive(self->btn_cancel_requests, btn_cancel_sensitive);

    vdi_manager_set_msg(self, message, is_error_message);
}

static void enable_vm_prep_progress_messages(VdiManager *self, gboolean enabled)
{
    if (enabled) {
        if (!self->vm_prep_progress_handle)
            self->vm_prep_progress_handle = g_signal_connect(get_vdi_session_static(),
                                                             "vm-prep-progress-received",
                                                             G_CALLBACK(on_vm_prep_progress_received), self);
    } else {
        // unsubscribe from progress messages
        if (self->vm_prep_progress_handle)
            g_signal_handler_disconnect(get_vdi_session_static(), self->vm_prep_progress_handle);
        self->vm_prep_progress_handle = 0;
    }
}

// Get Vm from pool
static void refresh_vdi_get_vm_from_pool_async(VdiManager *self, const gchar *pool_id)
{
    vdi_session_set_current_pool_id(pool_id);
    // gui message  "Отправлен запрос на получение вм из пула"
    set_vdi_client_state(self, VDI_WAITING_FOR_VM_FROM_POOL, _("VM request sent"), FALSE);
    // start spinner on vm widget
    VdiPoolWidget *vdi_pool_widget = get_vdi_pool_widget_by_id(self, pool_id);
    vdi_pool_widget_enable_spinner(vdi_pool_widget, TRUE);

    // take from gui correct remote protocol
    if (vdi_pool_widget && vdi_pool_widget->is_valid) {
        VmRemoteProtocol remote_protocol = vdi_pool_widget_get_current_protocol(vdi_pool_widget);
        g_info("%s remote_protocol %s", (const char *) __func__, util_remote_protocol_to_str(remote_protocol));
        vdi_session_set_current_remote_protocol(remote_protocol);
    }

    // subscribe to receive progress messages
    enable_vm_prep_progress_messages(self, TRUE);

    self->current_vm_request_id = MAX(rand() % 10000, 1); // id чтобы распознать ws сообщения
    // относящиеся к запросу

    // execute task
    RequestVmFromPoolData *vm_request_data = (RequestVmFromPoolData*)calloc(1, sizeof(RequestVmFromPoolData));
    vm_request_data->request_id = self->current_vm_request_id;
    vm_request_data->redirect_time_zone = self->p_conn_data->redirect_time_zone;
    execute_async_task(vdi_session_get_vm_from_pool_task,
            (GAsyncReadyCallback)on_vdi_session_get_vm_from_pool_finished,
            vm_request_data, self);
}

// start asynchronous task to get vm data from vdi
static void refresh_vdi_pool_data_async(VdiManager *self)
{
    vdi_session_cancel_pending_requests();
    unregister_all_pools(self);
    // "Отправлен запрос на список пулов"
    set_vdi_client_state(self, VDI_WAITING_FOR_POOL_DATA, _("Pool data request sent"), FALSE);

    PoolsRequestTaskData *task_data = calloc(1, sizeof(PoolsRequestTaskData));
    task_data->get_favorite_only = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->btn_show_only_favorites));

    //task_data->get_favorite_only = TRUE;
    execute_async_task(vdi_session_get_vdi_pool_data_task,
            (GAsyncReadyCallback)on_vdi_session_get_vdi_pool_data_finished, task_data, self);
}

// clear array of virtual machine widgets
static void unregister_all_pools(VdiManager *self)
{
    // disconnect handler for pool options if it exists
    if (self->favorite_pool_menu_item_toggled_handle) {
        g_signal_handler_disconnect(G_OBJECT(self->favorite_pool_menu_item),
                                    self->favorite_pool_menu_item_toggled_handle);
        self->favorite_pool_menu_item_toggled_handle = 0;
    }
    // hide pool options popup if shown
    gtk_widget_hide(self->pool_options_menu);

    if (self->pool_widgets_array) {
        guint i;
        for (i = 0; i < self->pool_widgets_array->len; ++i) {
            VdiPoolWidget *vdi_pool_widget = g_array_index(self->pool_widgets_array, VdiPoolWidget*, i);
            destroy_vdi_pool_widget(vdi_pool_widget);
        }

        g_array_free(self->pool_widgets_array, TRUE);
        self->pool_widgets_array = NULL;
    }
}
// create virtual machine widget and add to GUI
static void register_pool(VdiManager *self, const gchar *pool_id, const gchar *pool_name, const gchar *os_type,
        const gchar *status, JsonArray *conn_types_json_array, gboolean favorite)
{
    // create array if required
    if (self->pool_widgets_array == NULL)
        self->pool_widgets_array = g_array_new (FALSE, FALSE, sizeof(VdiPoolWidget*));

    // add element
    VdiPoolWidget *vdi_pool_widget = build_pool_widget(pool_id, pool_name, os_type, status,
                                                      conn_types_json_array, self->gtk_flow_box);
    vdi_pool_widget->vdi_manager = (void*)self;
    // mark favorite pool
    vdi_pool_widget_set_favorite(vdi_pool_widget, favorite);

    g_array_append_val(self->pool_widgets_array, vdi_pool_widget);
    // connect start button to callback
    vdi_pool_widget->btn_click_sig_hadle = g_signal_connect(vdi_pool_widget->vm_start_button,
            "button-release-event", G_CALLBACK(on_vm_start_button_released), vdi_pool_widget);
}

// find a virtual machine widget by id
static VdiPoolWidget *get_vdi_pool_widget_by_id(VdiManager *self, const gchar *searched_id)
{
    VdiPoolWidget *searched_vdi_pool_widget = NULL;
    guint i;

    if (self->pool_widgets_array == NULL)
        return searched_vdi_pool_widget;

    for (i = 0; i < self->pool_widgets_array->len; ++i) {
        VdiPoolWidget *vdi_pool_widget = g_array_index(self->pool_widgets_array, VdiPoolWidget*, i);

        if (g_strcmp0(searched_id, vdi_pool_widget->pool_id) == 0) {
            searched_vdi_pool_widget = vdi_pool_widget;
            break;
        }
    }

    return searched_vdi_pool_widget;
}

//////////////////////////////// async task callbacks//////////////////////////////////////
// callback which is invoked when pool data request finished
static void on_vdi_session_get_vdi_pool_data_finished(GObject *source_object G_GNUC_UNUSED,
                                        GAsyncResult *res, VdiManager *self)
{
    g_info("%s", (const char *)__func__);

    g_autofree gchar *err_msg = NULL;
    err_msg = g_strdup(_("Failed to fetch pool data"));
    gpointer ptr_res = g_task_propagate_pointer(G_TASK(res), NULL); // take ownership
    if(ptr_res == NULL) { // "Не удалось получить список пулов"
        set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, err_msg, TRUE);
        return;
    }

    gchar *response_body_str = ptr_res; // example "[{\"id\":17,\"name\":\"sad\"}]"
    g_info("%s : %s", (const char *)__func__, response_body_str);

    // parse vm data  json
    JsonParser *parser = json_parser_new();
    JsonObject *root_object = get_root_json_object(parser, response_body_str);

    JsonNode *data_member = NULL;
    if (root_object)
        data_member = json_object_get_member(root_object, "data");

    JsonArray *json_array = NULL;
    if (data_member)
        json_array = json_node_get_array(data_member);

    if (!json_array) {
        g_object_unref(parser);
        g_free(ptr_res);
        set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, err_msg, TRUE);
        return;
    }

    // prepare  pool_widgets_array
    unregister_all_pools(self);

    // fill pool_widgets_array
    guint json_arrayLength = MIN(json_array_get_length(json_array), MAX_POOL_NUMBER);
    g_info("Number of vm pools: %i", json_arrayLength);

    for (int i = (int)json_arrayLength - 1; i >= 0; --i) {

        JsonObject *object = json_array_get_object_element(json_array, (guint)i);

        const gchar *pool_id = json_object_get_string_member_safely(object, "id");
        const gchar *pool_name = json_object_get_string_member_safely(object, "name");
        const gchar *os_type = json_object_get_string_member_safely(object, "os_type");
        const gchar *status = json_object_get_string_member_safely(object, "status");
        //g_info("os_type %s\n", os_type);
        //g_info("pool_name %s\n", pool_name);
        JsonArray *conn_types_json_array =
                json_object_get_array_member_safely(object, "connection_types");
        gint64 favorite = json_object_get_int_member_safely(object, "favorite");
        register_pool(self, pool_id, pool_name, os_type, status, conn_types_json_array, (gboolean)favorite);
    }

    // "Получен список пулов"
    set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, _("Pool data received"), FALSE);
    //
    g_object_unref(parser);
    if(ptr_res)
        g_free(ptr_res);
}

static void go_to_vm(VdiManager *self)
{
//#if !defined(__MACH__)
    vdi_manager_finish_job(self);
//#endif
    g_signal_emit_by_name(self, "connect-to-vm-requested");
}

// callback which is invoked when vm start request finished
static void on_vdi_session_get_vm_from_pool_finished(GObject *source_object G_GNUC_UNUSED,
                                         GAsyncResult *res, VdiManager *self)
{
    g_info("%s", (const char *)__func__);
    enable_vm_prep_progress_messages(self, FALSE);

    VdiPoolWidget *vdi_pool_widget = get_vdi_pool_widget_by_id(self, vdi_session_get_current_pool_id());
    vdi_pool_widget_enable_spinner(vdi_pool_widget, FALSE);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->vm_prep_progress_bar), 1);

    GError *error = NULL;
    g_autofree gchar *err_msg = NULL;
    err_msg = g_strdup(_("Failed to get VM fom pool"));
    gpointer ptr_res = g_task_propagate_pointer (G_TASK(res), &error); // take ownership
    if (ptr_res == NULL) { // "Не удалось получить вм из пула",
        g_info("%s : FAIL", (const char *)__func__);
        set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, err_msg, TRUE);
        return;
    }

    VeilVmData *vdi_vm_data = (VeilVmData *)ptr_res;

    if (vdi_vm_data->server_reply_type == SERVER_REPLY_TYPE_DATA) {
        VmRemoteProtocol protocol = vdi_session_get_current_remote_protocol();

        // save to settings file the last pool we connected to
        write_str_to_ini_file(get_cur_ini_group_vdi(), "pool_id", vdi_session_get_current_pool_id());

        update_string_safely(&self->p_conn_data->ip, vdi_vm_data->vm_host);
        self->p_conn_data->port = vdi_vm_data->vm_port;
        update_string_safely(&self->p_conn_data->password, vdi_vm_data->vm_password);
        update_string_safely(&self->p_conn_data->vm_verbose_name, vdi_vm_data->vm_verbose_name);
        switch (protocol) {
            case SPICE_PROTOCOL:
            case SPICE_DIRECT_PROTOCOL:{
                update_string_safely(&self->p_conn_data->password, vdi_vm_data->vm_password);
                break;
            }
            default: {
                update_string_safely(&self->p_conn_data->password, vdi_session_get_vdi_password());
                break;
            }
        }

        update_string_safely(&self->p_conn_data->user, vdi_session_get_vdi_username());

        // "Получена вм из пула"
        set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, _("VM received from pool"), FALSE);

        //clear app data
        self->p_conn_data->rdp_settings.is_remote_app = FALSE;
        free_memory_safely(&self->p_conn_data->rdp_settings.remote_app_program);
        free_memory_safely(&self->p_conn_data->rdp_settings.remote_app_options);

        // Если существует список приложений и если протокол RDP, то показываем окно выбора приложений
        if (vdi_vm_data->farm_array && vdi_vm_data->farm_array->len > 0 &&
                (protocol == RDP_PROTOCOL || protocol == RDP_NATIVE_PROTOCOL)) {

            // fill data
            AppSelectorResult selector_res = vdi_app_selector_start(vdi_vm_data, GTK_WINDOW(self->window),
                    self->p_conn_data->rdp_settings.remote_application_format);
            self->p_conn_data->rdp_settings.is_remote_app = selector_res.is_remote_app;
            self->p_conn_data->rdp_settings.remote_app_program = selector_res.remote_app_program; // take ownership

            if (selector_res.result_type != APP_SELECTOR_RESULT_NONE) {
                go_to_vm(self);
            }
        } else {

            go_to_vm(self);
        }

    } else {
        const gchar *final_err_msg = (strlen_safely(vdi_vm_data->message) != 0) ?
                                    vdi_vm_data->message : err_msg;
        set_vdi_client_state(self, VDI_RECEIVED_RESPONSE, final_err_msg, TRUE);

        if (strlen_safely(final_err_msg) > MSG_TRIM_LENGTH)
            show_msg_box_dialog(GTK_WINDOW(self->window), final_err_msg);
    }
    //
    util_free_veil_vm_data(vdi_vm_data);
}

// ws data callback    "<span color=\"red\">%s</span>"
static void set_ws_conn_state(VdiManager *self, gboolean is_vdi_online)
{
    if (self->label_is_vdi_online) {

        gchar *resource_path;
        if (is_vdi_online)
            resource_path = g_strdup(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/green_circle.png");
        else
            resource_path = g_strdup(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/red_circle.png");

        gtk_image_set_from_resource((GtkImage *)self->label_is_vdi_online, resource_path);
        g_free(resource_path);
    }
}

/////////////////////////////////// gui elements callbacks//////////////////////////////////////
//// windows show callback
//static gboolean mapped_user_function(GtkWidget *widget,GdkEvent  *event, gpointer   user_data)
//{
//    g_info("%s\n", (const char *)__func__);
//    return TRUE;
//}
// window close callback
static gboolean on_window_deleted_cb(VdiManager *self)
{
    g_info("%s", (const char *)__func__);
    vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server

    vdi_manager_finish_job(self);
    g_signal_emit_by_name(self, "quit-requested");

    return TRUE;
}
// open user settings
static void btn_open_user_settings_clicked(GtkButton *button G_GNUC_UNUSED, VdiManager *self) {

    g_info("%s", (const char *)__func__);
    vdi_user_settings_widget_show(GTK_WINDOW(self->window));
}
// refresh button pressed callback
static void on_button_renew_clicked(GtkButton *button G_GNUC_UNUSED, VdiManager *self) {

    g_info("%s", (const char *)__func__);
    vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server

    refresh_vdi_pool_data_async(self);
}
// cancel pending requests
static void on_btn_cancel_requests_clicked(GtkButton *button G_GNUC_UNUSED, VdiManager *self G_GNUC_UNUSED) {

    g_info("%s", (const char *)__func__); // "Текущие запросы отменены"
    vdi_manager_set_msg(self, _("Current requests cancelled"), FALSE);
    vdi_session_cancel_pending_requests();
}
// quit button pressed callback
static void on_button_quit_clicked(GtkButton *button G_GNUC_UNUSED, VdiManager *self)
{
    g_info("%s", (const char *)__func__);
    vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server

    // logout
    vdi_session_logout();
    g_signal_emit_by_name(self, "logged-out", "");
}

// ws conn state callback
static void
on_ws_conn_changed(gpointer data G_GNUC_UNUSED, int ws_connected, VdiManager *self)
{
    set_ws_conn_state(self, ws_connected);
}

static void
on_auth_fail_detected(gpointer data G_GNUC_UNUSED, VdiManager *self)
{
    vdi_session_logout();
    g_signal_emit_by_name(self, "logged-out", _("401 Authorization required"));
}

static void
on_pool_entitlement_changed(gpointer data G_GNUC_UNUSED, VdiManager *self)
{
    if (!self->is_active)
        return;

    refresh_vdi_pool_data_async(self);
}

static void
on_vm_prep_progress_received(gpointer data G_GNUC_UNUSED, int request_id, int progress, const gchar *text,
        VdiManager *self)
{
    // Игнорируем если данные не соответствуют актуальному запросу
    if (request_id != self->current_vm_request_id)
        return;

    //progress
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->vm_prep_progress_bar), (gdouble)progress / 100.0);

    //msg
    vdi_manager_set_msg(self, text, FALSE);
}

static void favorite_pool_menu_item_toggled(GtkCheckMenuItem* check_menu_item, VdiPoolWidget *vdi_pool_widget)
{
    VdiManager *self = (VdiManager *)vdi_pool_widget->vdi_manager;

    if (!self->is_favorites_supported_by_server) {
        vdi_manager_set_msg(self, _("VeiL Broker version > 4.0.0 is required"), TRUE);
        return;
    }

    gboolean is_active = gtk_check_menu_item_get_active(check_menu_item);
    vdi_pool_widget_set_favorite(vdi_pool_widget, is_active);

    // http request to server
    FavoritePoolTaskData *task_data = calloc(1, sizeof(FavoritePoolTaskData));
    task_data->is_favorite = is_active;
    task_data->pool_id = g_strdup(vdi_pool_widget->pool_id);

    execute_async_task(vdi_session_set_pool_favorite_task, NULL, task_data, NULL);
}

static void on_btn_show_only_favorites_toggled(GtkToggleButton *toggle_btn G_GNUC_UNUSED, gpointer user_data)
{
    VdiManager *self = (VdiManager *)user_data;
    vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server
    refresh_vdi_pool_data_async(self);
}

// vm start button pressed callback
static gboolean on_vm_start_button_released(GtkButton *button,
        GdkEventButton *event, VdiPoolWidget *vdi_pool_widget)
{
    VdiManager *self = (VdiManager *)vdi_pool_widget->vdi_manager;

    if (event->type == GDK_BUTTON_RELEASE) {
        vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server

        if (event->button == 1) { // left mouse btn
            // reset progress bar
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->vm_prep_progress_bar), 0);

            g_info("%s  %s", (const char *) __func__, vdi_pool_widget->pool_id);
            refresh_vdi_get_vm_from_pool_async(self, vdi_pool_widget->pool_id);

        } else if (event->button == 3) { // right mouse btn
            // Show pool options
            if (self->favorite_pool_menu_item_toggled_handle) {
                g_signal_handler_disconnect(G_OBJECT(self->favorite_pool_menu_item),
                                            self->favorite_pool_menu_item_toggled_handle);
            }
            self->favorite_pool_menu_item_toggled_handle = g_signal_connect(
                    self->favorite_pool_menu_item, "toggled",
                             G_CALLBACK(favorite_pool_menu_item_toggled), vdi_pool_widget);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                    self->favorite_pool_menu_item), vdi_pool_widget->is_favorite);
            gtk_widget_show_all(self->pool_options_menu);
            gtk_menu_popup_at_widget(GTK_MENU(self->pool_options_menu), GTK_WIDGET(button),
                                     GDK_GRAVITY_EAST, GDK_GRAVITY_EAST, NULL);
        }
    }
    return TRUE;
}

static void
save_data_to_ini_file()
{
    write_int_to_ini_file("General", "cur_remote_protocol_index", (gint)vdi_session_get_current_remote_protocol());
}

static void vdi_manager_finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    VdiManager *self = VDI_MANAGER(object);
    g_signal_handler_disconnect(get_vdi_session_static(), self->ws_conn_changed_handle);
    g_signal_handler_disconnect(get_vdi_session_static(), self->auth_fail_detected_handle);
    g_signal_handler_disconnect(get_vdi_session_static(), self->pool_entitlement_changed_handle);
    enable_vm_prep_progress_messages(self, FALSE);

    gtk_widget_destroy(self->pool_options_menu);
    unregister_all_pools(self);
    g_object_unref(self->builder);
    gtk_widget_destroy(self->window);

    GObjectClass *parent_class = G_OBJECT_CLASS( vdi_manager_parent_class );
    ( *parent_class->finalize )( object );
}

static void vdi_manager_class_init(VdiManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass );
    gobject_class->finalize = vdi_manager_finalize;

    // signals
    g_signal_new("logged-out",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiManagerClass, logged_out),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_STRING);
    g_signal_new("connect-to-vm-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiManagerClass, connect_to_vm_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("quit-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiManagerClass, quit_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void vdi_manager_init(VdiManager *self)
{
    g_info("%s", (const char *)__func__);
    /* Create the widgets */
    self->builder = remote_viewer_util_load_ui("vdi_manager_form.glade");
    self->window = GTK_WIDGET(gtk_builder_get_object(self->builder, "vdi-main-window"));
    self->btn_open_user_settings = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_open_user_settings"));
    self->btn_cancel_requests = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_cancel_requests"));
    self->btn_show_only_favorites = GTK_WIDGET(gtk_builder_get_object(
            self->builder, "btn_show_only_favorites"));
    self->button_renew = GTK_WIDGET(gtk_builder_get_object(self->builder, "button-renew"));
    self->button_quit = GTK_WIDGET(gtk_builder_get_object(self->builder, "button-quit"));
    self->vm_main_box = GTK_WIDGET(gtk_builder_get_object(self->builder, "vm_main_box"));
    self->status_label = GTK_WIDGET(gtk_builder_get_object(self->builder, "status_label"));
    self->vm_prep_progress_bar = GTK_WIDGET(gtk_builder_get_object(self->builder, "vm_prep_progress_bar"));

    self->gtk_flow_box = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(self->gtk_flow_box), 10);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX(self->gtk_flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX(self->gtk_flow_box), 6);
    gtk_box_pack_start(GTK_BOX(self->vm_main_box), self->gtk_flow_box, FALSE, TRUE, 0);

    self->main_vm_spinner = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_vm_spinner"));
    self->label_is_vdi_online = GTK_WIDGET(gtk_builder_get_object(self->builder, "label-is-vdi-online"));

    // Pool options menu
    self->pool_options_menu = gtk_menu_new();
    self->favorite_pool_menu_item = gtk_check_menu_item_new_with_label(_("Add to favorite"));
    gtk_container_add(GTK_CONTAINER(self->pool_options_menu), self->favorite_pool_menu_item);

    // connects
    g_signal_connect(self->btn_show_only_favorites, "toggled",
            G_CALLBACK(on_btn_show_only_favorites_toggled), self);
    g_signal_connect_swapped(self->window, "delete-event", G_CALLBACK(on_window_deleted_cb), self);
    g_signal_connect(self->btn_open_user_settings, "clicked", G_CALLBACK(btn_open_user_settings_clicked), self);
    g_signal_connect(self->button_renew, "clicked", G_CALLBACK(on_button_renew_clicked), self);
    g_signal_connect(self->btn_cancel_requests, "clicked", G_CALLBACK(on_btn_cancel_requests_clicked), self);
    g_signal_connect(self->button_quit, "clicked", G_CALLBACK(on_button_quit_clicked), self);
    self->ws_conn_changed_handle = g_signal_connect(get_vdi_session_static(),
                                                      "ws-conn-changed", G_CALLBACK(on_ws_conn_changed), self);
    self->auth_fail_detected_handle = g_signal_connect(get_vdi_session_static(), "auth-fail-detected",
                                             G_CALLBACK(on_auth_fail_detected), self);
    self->pool_entitlement_changed_handle = g_signal_connect(get_vdi_session_static(),
            "pool-entitlement-changed", G_CALLBACK(on_pool_entitlement_changed), self);
}

void vdi_manager_finish_job(VdiManager *self)
{
    if (!self->is_active)
        return;

    // clear
    vdi_session_cancel_pending_requests();
    // save data to ini file
    save_data_to_ini_file();

    unregister_all_pools(self);

    gtk_widget_hide(self->window);

    self->is_active = FALSE;
}

void vdi_manager_show(VdiManager *self, ConnectSettingsData *conn_data)
{
    if (self->is_active)
        return;
    self->is_active = TRUE;

    self->p_conn_data = conn_data;

    // favorite pools
    self->is_favorites_supported_by_server =
            virt_viewer_compare_version(get_vdi_session_static()->vdi_version.string, "4.0.0") > 0;
    gtk_widget_set_sensitive(self->btn_show_only_favorites, self->is_favorites_supported_by_server);
    if (self->is_favorites_supported_by_server) {
        gtk_widget_set_tooltip_text(self->btn_show_only_favorites, "");
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->btn_show_only_favorites), FALSE);
        gtk_widget_set_tooltip_text(self->btn_show_only_favorites, _("VeiL Broker version > 4.0.0 is required"));
    }

    // show window
    g_autofree gchar *title = NULL; // %s  Время входа: %s  -  %s
    title = g_strdup_printf(_("%s  Login time: %s  -  %s"),
            vdi_session_get_vdi_username(),
            vdi_session_get_login_time(),
            APPLICATION_NAME);
    gtk_window_set_title(GTK_WINDOW(self->window), title);
    gtk_window_set_position(GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(self->window), 900, 600);
    gtk_widget_show_all(self->window);

    SoupWebsocketState state = vdi_ws_client_get_conn_state(vdi_session_get_ws_client());
    g_info("%s ws state %i", (const char *)__func__, state);
    set_ws_conn_state(self, state == SOUP_WEBSOCKET_STATE_OPEN);

    // connect_to_prev_pool_if_enabled
    g_autofree gchar *last_pool_id = NULL;
    last_pool_id = read_str_from_ini_file(get_cur_ini_group_vdi(), "pool_id");
    if (!last_pool_id)
        conn_data->connect_to_prev_pool = FALSE;

    if(conn_data->connect_to_prev_pool && conn_data->not_connected_to_prev_pool_yet) {
        // Получение ВМ из предыдущего пула
        refresh_vdi_get_vm_from_pool_async(self, last_pool_id);
    } else {
        // Пытаемся соединиться с vdi и получить список пулов. Получив список пулов нужно сгенерить
        // соответствующие кнопки  в скрол области.
        // get pool data
        refresh_vdi_pool_data_async(self);
    }
    conn_data->not_connected_to_prev_pool_yet = FALSE; // lower flag
}

void vdi_manager_raise(VdiManager *self)
{
    if (!self->is_active)
        return;

    if (self->window && gtk_widget_get_visible(self->window))
        gtk_window_present(GTK_WINDOW(self->window));
}

VdiManager *vdi_manager_new(void)
{
    return VDI_MANAGER( g_object_new( TYPE_VDI_MANAGER, NULL ) );
}
