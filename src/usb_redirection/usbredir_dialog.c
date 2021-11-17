/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#endif

#include <glib/gi18n.h>

#include "vdi_session.h"
#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "async.h"

#include "usbredir_dialog.h"
#include "usbredir_controller.h"
#include "usb_selector_widget.h"


typedef struct {
    UsbSelectorWidget *widget;

} UsbredirMainDialog;

///////////////////////////////////////////////////////////////////////////////////////

static void
usbredir_dialog_on_usb_device_toggled(GtkCellRendererToggle *cell_renderer G_GNUC_UNUSED,
        char *path, gpointer user_data)
{
    g_info("%s path %s", (const char*)__func__, path);

    UsbredirMainDialog *self = (UsbredirMainDialog *)user_data;

    //
    int usb_index = atoi(path);
    if (usb_index < 0 || usb_index >= self->widget->cur_usb_devices_number) {
        g_warning("%s Logical error. path %s cur_usb_devices_number %i", (const char*)__func__,
                path, (int)self->widget->cur_usb_devices_number);
        return;
    }

    // handle
    gboolean is_usb_redir_toggled = FALSE;

    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(self->widget->usb_list_store), &iter, path)) {
        g_warning("%s gtk_tree_model_get_iter_from_string failed", (const char*)__func__);
        return;
    }

    UsbState usb_state = self->widget->usb_dev_data_array[usb_index].usb_state;
    switch (usb_state) {
        // Если юсб не используется, то перенаправляем
        case USB_STATE_NOT_USED: {
            gchar *address_str = g_strdup(gtk_entry_get_text(GTK_ENTRY(self->widget->tk_address_entry)));
            g_strstrip(address_str);

            UsbServerStartData start_data;
            start_data.ipv4_addr = address_str; // move ownership
            start_data.port = usbredir_controller_get_free_port();
            start_data.usbbus = self->widget->usb_dev_data_array[usb_index].usbbus;
            start_data.usbaddr = self->widget->usb_dev_data_array[usb_index].usbaddr;
            start_data.usbvendor = -1;
            start_data.usbproduct = -1;
            usbredir_controller_start_task(start_data);

            //
            is_usb_redir_toggled = TRUE;
            self->widget->usb_dev_data_array[usb_index].usb_state = USB_STATE_BEING_REDIRECTED;
            break;
        }

        // Если юсб перпенаправляется, то отменяем пернаправление
        case USB_STATE_BEING_REDIRECTED: {
            // stop task
            usbredir_controller_stop_task(self->widget->usb_dev_data_array[usb_index].usbbus,
                    self->widget->usb_dev_data_array[usb_index].usbaddr);

            //
            self->widget->usb_dev_data_array[usb_index].usb_state = USB_STATE_CANCELLING_REDIRECTION;
            break;
        }

        // Если происходит завершения перенаправления, то ничего не делаем
        case USB_STATE_CANCELLING_REDIRECTION:
        default: {
            return;
        }
    }

    // gui
    GdkRGBA color = {0.0, 0.0, 0.0, 0.0};
    const gchar *status_text = usb_selector_widget_get_usb_status_text(self->widget, usb_index);
    gtk_list_store_set(self->widget->usb_list_store, &iter,
                       COLUMN_USB_REDIR_TOGGLE, is_usb_redir_toggled,
                       COLUMN_USB_REDIR_STATE, status_text,
                       COLUMN_USB_COLOR, &color,
                       -1);
}

static void
usbredir_dialog_usb_task_finished(gpointer source G_GNUC_UNUSED, int code, const gchar *message,
        int usbbus, int usbaddr, UsbredirMainDialog *self)
{
    g_info("%s", (const char*)__func__);
    if (!usbredir_controller_is_usb_tcp_window_shown())
        return;

    if (!self) {
        g_critical("%s self == NULL. Logical error", (const char*)__func__);
        return;
    }

    // find USB
    int found_usb_index = -1;
    for (ssize_t i = 0; i < self->widget->cur_usb_devices_number; i++) {
        if (self->widget->usb_dev_data_array[i].usbbus == usbbus &&
        self->widget->usb_dev_data_array[i].usbaddr == usbaddr) {
            found_usb_index = i;
            break;
        }
    }

    if (found_usb_index == -1) {
        g_info("%s found_usb_index == -1", (const char*)__func__);
        return;
    }

    // set gui state
    g_autofree gchar *path_string = NULL;
    path_string = g_strdup_printf("%i", found_usb_index);
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(self->widget->usb_list_store), &iter, path_string)) {

        // set status message color
        gdouble red = (code == USB_REDIR_FINISH_SUCCESS) ? 0.0 : 1.0;
        GdkRGBA color = {red, 0.0, 0.0, 0.0};

        gtk_list_store_set(self->widget->usb_list_store, &iter,
                           COLUMN_USB_REDIR_TOGGLE, 0,
                           COLUMN_USB_REDIR_STATE, message,
                           COLUMN_USB_COLOR, &color,
                           -1);
    }

    // set state
    self->widget->usb_dev_data_array[found_usb_index].usb_state = USB_STATE_NOT_USED;
}

static void
usbredir_dialog_dettach_usb(GTask    *task,
                             gpointer       source_object G_GNUC_UNUSED,
                             gpointer       task_data G_GNUC_UNUSED,
                             GCancellable  *cancellable G_GNUC_UNUSED)
{
    DetachUsbData *detach_usb_data = g_task_get_task_data(task);
    gboolean res = vdi_session_detach_usb(detach_usb_data);
    g_task_return_boolean(task, res);
}

static void
usbredir_dialog_on_usb_tcp_reset_finished(GObject *source_object G_GNUC_UNUSED,
                                         GAsyncResult *res,
                                         gpointer user_data G_GNUC_UNUSED)
{
    g_info("%s", (const char*)__func__);

    // do nothing if gui is not open right now
    if (!usbredir_controller_is_usb_tcp_window_shown())
        return;

    UsbredirMainDialog *self = (UsbredirMainDialog *)user_data;

    // set GUI
    gtk_spinner_stop(GTK_SPINNER(self->widget->status_spinner));

    gboolean is_success = g_task_propagate_boolean(G_TASK(res), NULL);
    if (is_success) {
        gtk_widget_set_sensitive((GtkWidget*)self->widget->usb_devices_list_view, TRUE);
    } else {
        // "<span color=\"red\">Не удалось сбросить USB TCP устройства на виртуальной машине</span>"
        gtk_label_set_text(GTK_LABEL(self->widget->status_label),
                _("Unable to detach USB TCP devices from remote machine"));
    }
}

static void take_tk_address_from_ini(UsbredirMainDialog *self, GtkEntry* tk_address_entry)
{
    gchar *current_tk_address = read_str_from_ini_file("General", "current_tk_address");
    gboolean is_address_correct = TRUE;
    if (current_tk_address) {
        if(strlen_safely(current_tk_address) == 0)
            is_address_correct = FALSE;
        else
            gtk_entry_set_text(tk_address_entry, current_tk_address);
        g_free(current_tk_address);
    } else {
        is_address_correct = FALSE;
    }

    // "<span color=\"red\">Не удалось определить ip адрес ТК. Задайте его вручную</span>"
    if (!is_address_correct)
        gtk_label_set_text(GTK_LABEL(self->widget->status_label),
                           _("Unable to determine thin client address. Set it manually"));
}

//Return address In case of success. Must be freed if its not NULL
static void usbredir_dialog_determine_tk_address_task(GTask    *task,
                                                 gpointer       source_object G_GNUC_UNUSED,
                                                 gpointer       task_data G_GNUC_UNUSED,
                                                 GCancellable  *cancellable G_GNUC_UNUSED)
{
    if ( !vdi_session_get_current_controller_address() ) {
        g_task_return_pointer(task, NULL, NULL);
        return;
    }

    gchar *address = NULL;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status = 0;

#ifdef __linux__
    gchar *command_line = g_strdup_printf("ip route get %s", vdi_session_get_current_controller_address());

    gboolean cmd_res = g_spawn_command_line_sync(command_line,
                              &standard_output,
                              &standard_error,
                              &exit_status,
                              NULL);

    if (cmd_res) {
        GRegex *regex = g_regex_new("(?<=src )(\\d{1,3}.){4}", 0, 0, NULL);

        GMatchInfo *match_info;
        g_regex_match(regex, standard_output, 0, &match_info);

        gboolean is_success = g_match_info_matches(match_info);
        if (is_success) {
            address = g_match_info_fetch(match_info, 0);
            g_print("Found address: %s\n", address);
        }

        g_match_info_free(match_info);
        g_regex_unref(regex);
    }
#elif __APPLE__ || __MACH__
    gchar *command_line = NULL;
#elif _WIN32
    // Get gateway from tracert     get_ip.bat %s
    GError *error = NULL;
    gchar *command_line = g_strdup_printf("tracert -h 1 %s",
                                          vdi_session_get_current_controller_address());
    g_info("command_line: %s", command_line);

    gboolean cmd_res = g_spawn_command_line_sync(command_line,
                                                 &standard_output,
                                                 &standard_error,
                                                 &exit_status,
                                                 &error);
    if (error)
        g_info("error->message: %s", error->message);
    g_clear_error(&error);

    if (!cmd_res || !standard_output)
        goto clean_mark;
    g_strstrip(standard_output);

    // 1
    IP_ADAPTER_INFO  *pAdapterInfo;
    ULONG            ulOutBufLen;
    DWORD            dwRetVal;

    // 2
    pAdapterInfo = (IP_ADAPTER_INFO *) malloc( sizeof(IP_ADAPTER_INFO) );
    ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    // 3
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) != ERROR_SUCCESS) {
        free (pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc( ulOutBufLen );
    }

    // 4
    if ((dwRetVal = GetAdaptersInfo( pAdapterInfo, &ulOutBufLen)) != ERROR_SUCCESS) {
        g_info("GetAdaptersInfo call failed with %i", (int)dwRetVal);
    }

    // 5
    PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
    while (pAdapter) {
        g_info("IP Address: %s", pAdapter->IpAddressList.IpAddress.String);
        g_info("Gateway: %s   %s", pAdapter->GatewayList.IpAddress.String, standard_output);
        // != NULL means standard_output contains  pAdapter->GatewayList.IpAddress.String
        // strstr checks if gateway is in standard_output. If it is, than takr the address
        if (strstr(standard_output, pAdapter->GatewayList.IpAddress.String) != NULL) {
            address = g_strdup(pAdapter->IpAddressList.IpAddress.String);
            break;
        }

        pAdapter = pAdapter->Next;
    }

    // 6
    if (pAdapterInfo)
        free(pAdapterInfo);

    clean_mark:
#endif
    g_free(command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    g_task_return_pointer(task, address, NULL); // adress will be freed in callback
}

static void
usbredir_dialog_on_determine_tk_address_finished(GObject *source_object G_GNUC_UNUSED,
                                                 GAsyncResult *res,
                                                 gpointer user_data)
{
    if (!usbredir_controller_is_usb_tcp_window_shown())
        return;
    g_info("%s", (const char*)__func__);

    UsbredirMainDialog *self = (UsbredirMainDialog *)user_data;

    gpointer ptr_res = g_task_propagate_pointer(G_TASK (res), NULL);
    if (ptr_res) {
        gchar *tk_address = (gchar *)ptr_res;
        g_info("%s : %s", (const char*)__func__, tk_address);
        gtk_entry_set_text((GtkEntry*)self->widget->tk_address_entry, tk_address);

        free_memory_safely(&tk_address);
    } else { // get from ini if we didnt determine it
        take_tk_address_from_ini(self, GTK_ENTRY(self->widget->tk_address_entry));
    }
}

static void
usbredir_dialog_check_if_reset_required_and_reset(UsbredirMainDialog *self)
{
    if (usbredir_controller_is_tcp_usb_devices_reset_required()) {
        // set GUI
        gtk_widget_set_sensitive((GtkWidget*)self->widget->usb_devices_list_view, FALSE);
        gtk_spinner_start(GTK_SPINNER(self->widget->status_spinner));

        // send request to vdi (veil)
        DetachUsbData *detach_usb_data = calloc(1, sizeof(DetachUsbData));
        detach_usb_data->remove_all = TRUE;
        execute_async_task(usbredir_dialog_dettach_usb, usbredir_dialog_on_usb_tcp_reset_finished,
                detach_usb_data, self);

        // try to get address on which the server will be created
        execute_async_task(usbredir_dialog_determine_tk_address_task, usbredir_dialog_on_determine_tk_address_finished,
                           NULL, self);
    } else {
        take_tk_address_from_ini(self, GTK_ENTRY(self->widget->tk_address_entry));
    }

    usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(FALSE);
}

static void usbredir_dialog_set_initial_gui_state(UsbredirMainDialog *self)
{
    //GtkTreeIter iter;
    //gboolean iter_ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self->widget->usb_list_store), &iter);
    //while (iter_ok) {
    //
    //    iter_ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(self->widget->usb_list_store), &iter);
    //}
    for (ssize_t i = 0; i < self->widget->cur_usb_devices_number; i++) {

        // Смотрим есть ли задача связанная с этой USB. Если есть то смотрим флаг running
        // Если он опущен, то считаем, что задача в процессе завершения.
        int usb_task_index = usbredir_controller_find_cur_task_index(
                self->widget->usb_dev_data_array[i].usbbus, self->widget->usb_dev_data_array[i].usbaddr);

        if (usb_task_index == -1) {
            self->widget->usb_dev_data_array[i].usb_state = USB_STATE_NOT_USED;
        }
        else {
            gboolean is_task_active = usbredir_controller_check_if_task_active(usb_task_index);
            if (is_task_active)
                self->widget->usb_dev_data_array[i].usb_state = USB_STATE_BEING_REDIRECTED;
            else
                self->widget->usb_dev_data_array[i].usb_state = USB_STATE_CANCELLING_REDIRECTION;
        }

        const gchar *status_text = usb_selector_widget_get_usb_status_text(self->widget, i);
        gboolean usb_toggle_value = (self->widget->usb_dev_data_array[i].usb_state == USB_STATE_BEING_REDIRECTED);

        // set to gui
        GtkTreeIter iter;
        g_autofree gchar *path_string = NULL;
        path_string = g_strdup_printf("%zi", i);
        if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(self->widget->usb_list_store), &iter, path_string)) {
            gtk_list_store_set(self->widget->usb_list_store, &iter,
                               COLUMN_USB_REDIR_TOGGLE, usb_toggle_value,
                               COLUMN_USB_REDIR_STATE, status_text,
                               -1);
        }
    }
}

void
usbredir_dialog_start(GtkWindow *parent, const gchar *title)
{
    usbredir_controller_set_usb_tcp_window_shown(TRUE);

    UsbredirMainDialog *self = calloc(1, sizeof(UsbredirMainDialog));
    self->widget = usb_selector_widget_new(title);

    // set initial gui state
    usbredir_dialog_set_initial_gui_state(self);

    // signals
    gulong usb_redir_finished_handle = g_signal_connect(usbredir_controller_get_static(), "usb-redir-finished",
                                              G_CALLBACK(usbredir_dialog_usb_task_finished), self);
    g_signal_connect(self->widget->usb_toggle_cell_renderer, "toggled",
            G_CALLBACK(usbredir_dialog_on_usb_device_toggled), self);

    // check if usb tcp reset required and reset
    usbredir_dialog_check_if_reset_required_and_reset(self);

    // show window
    usb_selector_widget_show_and_start_loop(self->widget, parent);

    // disconnect signals
    g_signal_handler_disconnect(usbredir_controller_get_static(), usb_redir_finished_handle);

    // save to ini
    const gchar *tk_address = gtk_entry_get_text((GtkEntry*)self->widget->tk_address_entry);
    write_str_to_ini_file("General", "current_tk_address", tk_address);

    // free
    usb_selector_widget_free(self->widget);
    free(self);

    usbredir_controller_set_usb_tcp_window_shown(FALSE);
}
