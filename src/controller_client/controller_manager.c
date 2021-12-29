/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include "controller_manager.h"
#include "controller_session.h"
#include "config.h"

G_DEFINE_TYPE( ControllerManager, controller_manager, G_TYPE_OBJECT )


#define MAX_VMS_IN_TABLE 100

// Enums
enum {
    COLUMN_VM_VERBOSE_NAME,
    COLUMN_NODE_VERBOSE_NAME,
    COLUMN_VM_IP,
    COLUMN_STATUS,
    COLUMN_VM_ID,
    N_COLUMNS
};

typedef enum {
    GUI_STATE_DEFAULT,
    GUI_STATE_FETCHING_DATA
} GuiState;


static void controller_manager_finalize(GObject *object)
{
    g_info("%s", (const char *) __func__);
    ControllerManager *self = CONTROLLER_MANAGER(object);

    g_object_unref(self->builder);
    gtk_widget_destroy(self->window);
}

static void controller_manager_class_init(ControllerManagerClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = controller_manager_finalize;
}

static void controller_manager_set_status(ControllerManager *self, const gchar *message, gboolean is_error_msg)
{
    util_set_message_to_info_label(GTK_LABEL(self->status_label), message);
    if (is_error_msg)
        gtk_widget_set_name(self->status_label, "label_error");
    else
        gtk_widget_set_name(self->status_label, "status_label");
}

static void controller_manager_set_gui_state(ControllerManager *self, GuiState gui_state)
{
    switch (gui_state) {
        case GUI_STATE_DEFAULT: {
            gtk_spinner_stop(GTK_SPINNER(self->main_vm_spinner));
            gtk_widget_set_sensitive(self->btn_update, TRUE);
            gtk_widget_set_sensitive(self->vm_verbose_name_entry, TRUE);
            gtk_widget_set_sensitive(self->btn_cancel_requests, FALSE);

            break;
        }
        case GUI_STATE_FETCHING_DATA: {
            gtk_spinner_start(GTK_SPINNER(self->main_vm_spinner));
            gtk_widget_set_sensitive(self->btn_update, FALSE);
            gtk_widget_set_sensitive(self->vm_verbose_name_entry, FALSE);
            gtk_widget_set_sensitive(self->btn_cancel_requests, TRUE);
            break;
        }
        default:
            break;
    }
}

static void controller_manager_add_columns_to_view(ControllerManager *self)
{
    /// view
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // column 1
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COLUMN_VM_VERBOSE_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->vm_list_tree_view), column);

    // column 2
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Server", renderer, "text", COLUMN_NODE_VERBOSE_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->vm_list_tree_view), column);

    // column 3
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("IP-address", renderer, "text", COLUMN_VM_IP, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->vm_list_tree_view), column);

    // column 4
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Status", renderer, "text", COLUMN_STATUS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->vm_list_tree_view), column);

    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(self->vm_list_tree_view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
}

static void on_controller_session_get_vm_list_task_finished(GObject *source_object G_GNUC_UNUSED,
                                                             GAsyncResult *res, ControllerManager *self)
{
    controller_manager_set_gui_state(self, GUI_STATE_DEFAULT);
    controller_manager_set_status(self, "", FALSE);

    GtkTreeIter iter;

    g_autofree gchar *err_msg = NULL;
    err_msg = g_strdup(_("Failed to fetch list of VMs."));
    gpointer ptr_res = g_task_propagate_pointer(G_TASK(res), NULL); // take ownership
    if(ptr_res == NULL) {
        controller_manager_set_status(self, err_msg, TRUE);
        return;
    }

    gchar *response_body_str = ptr_res;
    //g_info("%s : %s", (const char *)__func__, response_body_str);

    gtk_list_store_clear(self->vm_list_store);

    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object_ecp(parser, response_body_str, &server_reply_type);

    if (reply_json_object && server_reply_type == SERVER_REPLY_TYPE_DATA) {

        gint64 count = json_object_get_int_member_safely(reply_json_object, "count");

        g_autofree gchar *info_msg = NULL;
        info_msg = g_strdup_printf(_("List of VMs received. Amount: %li."), count);
        controller_manager_set_status(self, info_msg, FALSE);

        JsonArray *json_array = json_object_get_array_member_safely(reply_json_object, "results");
        if (json_array) {
            guint json_array_length = MIN(json_array_get_length(json_array), MAX_VMS_IN_TABLE);

            for (guint i = 0; i < json_array_length; i++) {
                JsonObject *object = json_array_get_object_element(json_array, (guint) i);
                if (object == NULL)
                    continue;

                const gchar *vm_id = json_object_get_string_member_safely(object, "id");
                const gchar *vm_verbose_name = json_object_get_string_member_safely(object, "verbose_name");
                const gchar *vm_status = json_object_get_string_member_safely(object, "status");
                JsonObject *node_object = json_object_get_object_member_safely(object, "node");
                const gchar *node_verbose_name = json_object_get_string_member_safely(node_object, "verbose_name");
                const gchar *ipv4_0 = controller_session_extract_ipv4(object);

                //g_info("DATA: %s %s %s %s", vm_id, vm_verbose_name, vm_status, node_verbose_name);

                // Add to table
                // Add a new row to the model
                gtk_list_store_append(self->vm_list_store, &iter);
                // initial state
                gtk_list_store_set(self->vm_list_store, &iter,
                                   COLUMN_VM_VERBOSE_NAME, vm_verbose_name,
                                   COLUMN_NODE_VERBOSE_NAME, node_verbose_name,
                                   COLUMN_VM_IP, ipv4_0 ? ipv4_0 : "-",
                                   COLUMN_STATUS, vm_status,
                                   COLUMN_VM_ID, vm_id,
                                   -1);
            }
        }
    } else {
        const gchar *message = reply_json_object ?
                               json_object_get_string_member_safely(reply_json_object, "detail") : err_msg;
        controller_manager_set_status(self, message, TRUE);
    }

    g_object_unref(parser);
    g_free(ptr_res);
}

static void on_controller_session_get_vm_data_task_finished(GObject *source_object G_GNUC_UNUSED,
                                                             GAsyncResult *res, ControllerManager *self)
{
    controller_manager_set_gui_state(self, GUI_STATE_DEFAULT);

    g_autofree gchar *err_msg = NULL;
    err_msg = g_strdup(_("Failed to fetch vm data."));
    gpointer ptr_res = g_task_propagate_pointer(G_TASK(res), NULL); // take ownership
    if(ptr_res == NULL) { // "Не удалось получить данные"
        controller_manager_set_status(self, err_msg, TRUE);
        return;
    }

    // Parse
    VeilVmData *vm_data = (VeilVmData *)ptr_res;

    if (vm_data->server_reply_type == SERVER_REPLY_TYPE_DATA) {

        controller_manager_set_status(self, _("VM data received."), FALSE);

        update_string_safely(&self->p_conn_data->vm_verbose_name, vm_data->vm_verbose_name);
        update_string_safely(&self->p_conn_data->ip, vm_data->vm_host);
        update_string_safely(&self->p_conn_data->user, vm_data->vm_username);
        update_string_safely(&self->p_conn_data->password, vm_data->vm_password);
        self->p_conn_data->port = vm_data->vm_port;
        //
        self->ci.response = TRUE;
        self->ci.next_app_state = APP_STATE_CONNECT_TO_VM;
        shutdown_loop(self->ci.loop);
    } else {
        const gchar *message = vm_data->message ? vm_data->message : err_msg;
        controller_manager_set_status(self, message, TRUE);
    }

    util_free_veil_vm_data(vm_data);
}

static void controller_manager_get_vms_async(ControllerManager *self)
{
    // Setup gui
    controller_manager_set_status(self, _("List of VMs requested."), FALSE);
    controller_manager_set_gui_state(self, GUI_STATE_FETCHING_DATA);

    // Request
    controller_session_cancel_pending_requests();

    VmListRequestData *vm_list_request_data = calloc(1, sizeof(VmListRequestData)); // will be free in task
    vm_list_request_data->name_filter = g_strdup(gtk_entry_get_text(GTK_ENTRY(self->vm_verbose_name_entry)));
    vm_list_request_data->ordering = g_strdup("verbose_name");
    vm_list_request_data->limit = MAX_VMS_IN_TABLE;

    execute_async_task(controller_session_get_vm_list_task,
                       (GAsyncReadyCallback)on_controller_session_get_vm_list_task_finished,
                       vm_list_request_data, self);
}

static void on_btn_update_clicked(GtkButton *button G_GNUC_UNUSED, ControllerManager *self)
{
    g_info("%s", (const char *)__func__);
    controller_manager_get_vms_async(self);
}

static void on_btn_logout_clicked(GtkButton *button G_GNUC_UNUSED, ControllerManager *self)
{
    g_info("%s", (const char *)__func__);

    // logout
    controller_session_logout();

    self->ci.response = FALSE;
    self->ci.next_app_state = APP_STATE_AUTH_DIALOG;
    shutdown_loop(self->ci.loop);
}

static void on_btn_cancel_requests_clicked(GtkButton *button G_GNUC_UNUSED, ControllerManager *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    controller_session_cancel_pending_requests();
}

static gboolean on_window_deleted_cb(ControllerManager *self)
{
    g_info("%s", (const char *)__func__);

    self->ci.response = FALSE;
    self->ci.next_app_state = APP_STATE_EXITING;
    shutdown_loop(self->ci.loop);
    return TRUE;
}

static gboolean vm_list_tree_view_mouse_btn_pressed(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event,
        gpointer data)
{
    if (event->type == GDK_BUTTON_PRESS) {
        ControllerManager *self = (ControllerManager *)data;

        GtkTreeModel *model = GTK_TREE_MODEL(self->vm_list_store);
        GtkTreePath *path = NULL;

        gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(self->vm_list_tree_view),
                                      event->x,
                                      event->y,
                                      &path,
                                      NULL,
                                      NULL,
                                      NULL);
        if (path) {
            // extract selected vm id
            GtkTreeIter iter;
            gtk_tree_model_get_iter(model, &iter, path);
            g_autofree gchar *vm_verbose_name = NULL;
            gchar *vm_id = NULL; // will be freed in task
            gtk_tree_model_get(model, &iter,
                    COLUMN_VM_VERBOSE_NAME, &vm_verbose_name,
                    COLUMN_VM_ID, &vm_id,
                    -1);
            g_info("NAME: %s  vm_id: %s", vm_verbose_name, vm_id);
            gtk_tree_path_free(path);

            // Set remote protocol
            const gchar *protocol_str = gtk_combo_box_get_active_id((GtkComboBox*)self->remote_protocol_combobox);
            VmRemoteProtocol protocol = util_str_to_remote_protocol(protocol_str);
            controller_session_set_current_remote_protocol(protocol);

            // get VM specific data
            controller_manager_set_status(self, _("Vm data requested."), FALSE);
            execute_async_task(controller_session_get_vm_data_task,
                               (GAsyncReadyCallback)on_controller_session_get_vm_data_task_finished,
                               vm_id, self);
        }
    }

    return TRUE;
}

static void on_vm_verbose_name_entry_activated(GtkEntry* entry G_GNUC_UNUSED, ControllerManager *self)
{
    controller_manager_get_vms_async(self);
}

static void controller_manager_init(ControllerManager *self)
{
    g_info("%s", (const char *) __func__);

    self->ci.response = FALSE;
    self->ci.loop = NULL;
    self->ci.next_app_state = APP_STATE_AUTH_DIALOG;

    self->builder = remote_viewer_util_load_ui("controller_manager_form.glade");
    self->window = GTK_WIDGET(gtk_builder_get_object(self->builder, "main-window"));

    self->vm_list_tree_view = GTK_WIDGET(gtk_builder_get_object(self->builder, "vm_list_tree_view"));
    // setup tree_view
    gtk_tree_view_set_hover_selection(GTK_TREE_VIEW(self->vm_list_tree_view), TRUE);

    controller_manager_add_columns_to_view(self);
    self->vm_list_store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
            G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self->vm_list_tree_view), GTK_TREE_MODEL(self->vm_list_store));

    self->vm_verbose_name_entry = GTK_WIDGET(gtk_builder_get_object(self->builder, "vm_verbose_name_entry"));
    self->btn_update = GTK_WIDGET(gtk_builder_get_object(self->builder, "button-renew"));
    self->btn_logout = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_logout"));
    self->btn_cancel_requests = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_cancel_requests"));

    self->remote_protocol_combobox = GTK_WIDGET(gtk_builder_get_object(self->builder, "remote_protocol_combobox"));
#if  defined(_WIN32) || defined(__MACH__)
    const gchar *rdp_native = util_remote_protocol_to_str(RDP_NATIVE_PROTOCOL);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->remote_protocol_combobox), rdp_native, rdp_native);
#endif
    self->status_label = GTK_WIDGET(gtk_builder_get_object(self->builder, "status_label"));
    self->main_vm_spinner = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_vm_spinner"));

    // connects
    g_signal_connect_swapped(self->window, "delete-event", G_CALLBACK(on_window_deleted_cb), self);
    g_signal_connect(self->btn_update, "clicked", G_CALLBACK(on_btn_update_clicked), self);
    g_signal_connect(self->btn_logout, "clicked", G_CALLBACK(on_btn_logout_clicked), self);
    g_signal_connect(self->btn_cancel_requests, "clicked",
            G_CALLBACK(on_btn_cancel_requests_clicked), self);
    g_signal_connect(self->vm_list_tree_view, "button-press-event",
            G_CALLBACK(vm_list_tree_view_mouse_btn_pressed), self);
    g_signal_connect(self->vm_verbose_name_entry, "activate",
            G_CALLBACK(on_vm_verbose_name_entry_activated), self);
}

RemoteViewerState controller_manager_dialog(ControllerManager *self, ConnectSettingsData *conn_data)
{
    self->p_conn_data = conn_data;

    g_autofree gchar *login_time_str = NULL;
    login_time_str = controller_session_get_login_time();
    g_autofree gchar *title = NULL; // %s  Время входа: %s  -  %s
    title = g_strdup_printf(_("%s  Login time: %s  -  %s"),
                            get_controller_session_static()->username.string,
                            login_time_str,
                            APPLICATION_NAME);
    gtk_window_set_title(GTK_WINDOW(self->window), title);
    gtk_window_set_position(GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(self->window), 650, 500);
    gtk_widget_show_all(self->window);

    // Clear tree view
    gtk_list_store_clear(self->vm_list_store);

    // Request vm list
    controller_manager_get_vms_async(self);

    create_loop_and_launch(&self->ci.loop);

    // free, hide
    gtk_list_store_clear(self->vm_list_store);
    gtk_widget_hide(self->window);

    return self->ci.next_app_state;
}

ControllerManager *controller_manager_new()
{
    g_info("%s", (const char *)__func__);
    ControllerManager *controller_manager = CONTROLLER_MANAGER( g_object_new( TYPE_CONTROLLER_MANAGER, NULL ) );
    return controller_manager;
}