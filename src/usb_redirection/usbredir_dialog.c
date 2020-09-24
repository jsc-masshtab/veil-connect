//
// Created by ubuntu on 09.09.2020.
//

#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#endif
#include <libusb.h>
#include <usbredirparser.h>

#include "vdi_session.h"
#include "usbredir_util.h"
#include "usbredir_dialog.h"
#include "remote-viewer-util.h"
#include "usbredir_controller.h"
#include "settingsfile.h"

#include "async.h"


#define MAX_USB_NUMBER 20

// Enums
enum {
    COLUMN_USB_NAME,
    COLUMN_USB_REDIR_TOGGLE,
    COLUMN_USB_REDIR_STATE,
    COLUMN_USB_COLOR,
    N_COLUMNS
};

typedef enum
{
    USB_STATE_NOT_USED,
    USB_STATE_BEING_REDIRECTED,
    USB_STATE_CANCELLING_REDIRECTION
} UsbState;

// Structs
typedef struct {

    uint8_t usbbus;
    uint8_t usbaddr;
    uint8_t pnum;

    UsbState usb_state;

} UsbDeviceData;

typedef struct {

    GtkWidget *main_window;
    GtkWidget *usb_devices_list_view;
    GtkWidget *tk_address_entry;
    GtkWidget *status_label;
    GtkWidget *status_spinner;
    GtkWidget *ok_btn;

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    GtkListStore *usb_list_store;

    UsbDeviceData usb_state_array[MAX_USB_NUMBER]; // Используем статич. массив так число usb ограничено и не велико.
    ssize_t cur_usb_devices_number;

} UsbredirMainDialogData;

///////////////////////////////////////////////////////////////////////////////////////

static const gchar *usbredir_dialog_get_usb_status_text(UsbState usb_state)
{
    switch (usb_state) {
        case USB_STATE_BEING_REDIRECTED:
            return "USB перенаправляется";
        case USB_STATE_CANCELLING_REDIRECTION:
            return"Завершаем перенаправление USB. Ожидайте";
        case USB_STATE_NOT_USED:
        default:
            return "";
    }
}

static void
usbredir_dialog_on_usb_device_toggled(GtkCellRendererToggle *cell_renderer G_GNUC_UNUSED,
        char *path, gpointer user_data)
{
    g_info("%s path %s", (const char*)__func__, path);

    UsbredirMainDialogData *priv = (UsbredirMainDialogData *)user_data;

    //
    int usb_index = atoi(path);
    if (usb_index < 0 || usb_index >= priv->cur_usb_devices_number) {
        g_critical("%s Logical error. path %s cur_usb_devices_number %i", (const char*)__func__,
                path, (int)priv->cur_usb_devices_number);
        return;
    }

    // handle
    gboolean is_usb_redir_toggled = FALSE;

    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(priv->usb_list_store), &iter, path);

    UsbState usb_state = priv->usb_state_array[usb_index].usb_state;
    switch (usb_state) {
        // Если юсб не используется, то перенаправляем
        case USB_STATE_NOT_USED: {
            gchar *address_str = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->tk_address_entry)));
            g_strstrip(address_str);

            UsbServerStartData start_data;
            start_data.ipv4_addr = address_str; // move ownership
            start_data.port = usbredir_controller_get_free_port();
            start_data.usbbus = priv->usb_state_array[usb_index].usbbus;
            start_data.usbaddr = priv->usb_state_array[usb_index].usbaddr;
            start_data.usbvendor = -1;
            start_data.usbproduct = -1;
            usbredir_controller_start_task(start_data);

            //
            is_usb_redir_toggled = TRUE;
            priv->usb_state_array[usb_index].usb_state = USB_STATE_BEING_REDIRECTED;
            break;
        }

        // Если юсб перпенаправляется, то отменяем пернаправление
        case USB_STATE_BEING_REDIRECTED: {
            // stop task
            usbredir_controller_stop_task(priv->usb_state_array[usb_index].usbbus,
                    priv->usb_state_array[usb_index].usbaddr);

            //
            priv->usb_state_array[usb_index].usb_state = USB_STATE_CANCELLING_REDIRECTION;
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
    const gchar *status_text = usbredir_dialog_get_usb_status_text(priv->usb_state_array[usb_index].usb_state);
    gtk_list_store_set(priv->usb_list_store, &iter,
                       COLUMN_USB_REDIR_TOGGLE, is_usb_redir_toggled,
                       COLUMN_USB_REDIR_STATE, status_text,
                       COLUMN_USB_COLOR, &color,
                       -1);
}

static void
usbredir_dialog_usb_task_finished_callback(UsbRedirTaskResaultData *res_data, gpointer user_data)
{
    if (!usbredir_controller_is_usb_tcp_window_shown())
        return;
    g_info("%s", (const char*)__func__);

    UsbredirMainDialogData *priv = (UsbredirMainDialogData *)user_data;
    if (!priv) {
        g_critical("%s priv == NULL. Logical error", (const char*)__func__);
        return;
    }

    // find USB
    int found_usb_index = -1;
    for (ssize_t i = 0; i < priv->cur_usb_devices_number; i++) {
        if (priv->usb_state_array[i].usbbus == res_data->usbbus &&
        priv->usb_state_array[i].usbaddr == res_data->usbaddr) {
            found_usb_index = i;
            break;
        }
    }

    if (found_usb_index == -1) {
        g_info("%s found_usb_index == -1", (const char*)__func__);
        return;
    }

    // set gui state
    gchar *path_string = g_strdup_printf("%i", found_usb_index);
    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(priv->usb_list_store), &iter, path_string);

    // set status messege color
    gdouble red = (res_data->code == USB_REDIR_FINISH_SUCCESS) ? 0.0 : 1.0;
    GdkRGBA color = {red, 0.0, 0.0, 0.0};

    gtk_list_store_set(priv->usb_list_store, &iter,
                   COLUMN_USB_REDIR_TOGGLE, 0,
                   COLUMN_USB_REDIR_STATE, res_data->message,
                   COLUMN_USB_COLOR, &color,
                   -1);

    free_memory_safely(&path_string);

    // set state
    priv->usb_state_array[found_usb_index].usb_state = USB_STATE_NOT_USED;
}

static gboolean
usbredir_dialog_window_deleted_cb(UsbredirMainDialogData *priv)
{
    priv->dialog_window_response = GTK_RESPONSE_CLOSE;
    shutdown_loop(priv->loop);
    return TRUE;
}

static void
usbredir_dialog_btn_ok_clicked_cb(GtkButton *button G_GNUC_UNUSED, UsbredirMainDialogData *priv)
{
    shutdown_loop(priv->loop);
}

static void
usbredir_dialog_add_columns_to_usb_view(UsbredirMainDialogData *priv)
{
    /// view
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // column 1
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("USB", renderer, "text", COLUMN_USB_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->usb_devices_list_view), column);

    // column 2
    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(renderer, "toggled", G_CALLBACK(usbredir_dialog_on_usb_device_toggled), priv);

    gtk_cell_renderer_toggle_set_active((GtkCellRendererToggle*)renderer, TRUE);

    gtk_cell_renderer_toggle_set_activatable((GtkCellRendererToggle *)renderer, TRUE);
    column = gtk_tree_view_column_new_with_attributes("", renderer, "active", TRUE, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->usb_devices_list_view), column);

    // column 3
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Состояние", renderer, "text", COLUMN_USB_REDIR_STATE,
                                                      "foreground-rgba", COLUMN_USB_COLOR, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->usb_devices_list_view), column);

    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(priv->usb_devices_list_view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
}

static void
usbredir_dialog_fill_usb_model(UsbredirMainDialogData *priv)
{
    // fill data
    memset(priv->usb_state_array, 0, sizeof(priv->usb_state_array));
    GtkTreeIter iter;

    // get data using libinfo
    libusb_context *ctx;
    usbredir_util_init_libusb_and_set_options(&ctx, usbredirparser_info);

    libusb_device **usb_list = NULL;
    struct libusb_device_descriptor desc;
    priv->cur_usb_devices_number = libusb_get_device_list(ctx, &usb_list);

    priv->cur_usb_devices_number = MIN(MAX_USB_NUMBER, priv->cur_usb_devices_number);
    for (ssize_t i = 0; i < priv->cur_usb_devices_number; i++)
    {
        libusb_device *dev = usb_list[i];

        // get USB data
        UsbDeviceData usb_device_data;
        usb_device_data.usbbus = libusb_get_bus_number(dev);
        usb_device_data.usbaddr = libusb_get_device_address(dev);
        usb_device_data.pnum = libusb_get_port_number(dev);

        libusb_get_device_descriptor(dev, &desc);

        // get text description for gui
        gchar *manufacturer = NULL, *product = NULL;
        usbredir_util_get_device_strings(usb_device_data.usbbus, usb_device_data.usbaddr,
                                          desc.idVendor, desc.idProduct,
                                          &manufacturer, &product);

        gchar *usb_name = g_strdup_printf("%04x:%04x %s %s", usb_device_data.usbbus, usb_device_data.usbaddr,
                                          manufacturer, product);
        g_info("USB  %s", usb_name);

        // determine state
        // Смотрим есть ли задача связанная с этой USB. Если есть то смотрим флаг running
        // Если он опущен, то считаем, что задача в процессе завершения.
        int usb_task_index = usbredir_controller_find_cur_task_index(usb_device_data.usbbus, usb_device_data.usbaddr);

        if (usb_task_index == -1) {
            usb_device_data.usb_state = USB_STATE_NOT_USED;
        }
        else {
            gboolean is_task_active = usbredir_controller_check_if_task_active(usb_task_index);
            if (is_task_active)
                usb_device_data.usb_state = USB_STATE_BEING_REDIRECTED;
            else
                usb_device_data.usb_state = USB_STATE_CANCELLING_REDIRECTION;
        }

        priv->usb_state_array[i] = usb_device_data;

        // Add a new row to the model
        gtk_list_store_append(priv->usb_list_store, &iter);

        const gchar *status_text = usbredir_dialog_get_usb_status_text(priv->usb_state_array[i].usb_state);
        gboolean usb_toggle_value = (usb_device_data.usb_state == USB_STATE_BEING_REDIRECTED);

        gtk_list_store_set(priv->usb_list_store, &iter,
                           COLUMN_USB_NAME, usb_name,
                           COLUMN_USB_REDIR_TOGGLE, usb_toggle_value,
                           COLUMN_USB_REDIR_STATE, status_text,
                           -1);
        g_free(usb_name);
    }

    // release
    libusb_free_device_list(usb_list, 1);
    libusb_exit(ctx);
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

    UsbredirMainDialogData *priv = (UsbredirMainDialogData *)user_data;

    // set GUI
    gtk_spinner_stop(GTK_SPINNER(priv->status_spinner));

    gboolean is_success = g_task_propagate_boolean(G_TASK(res), NULL);
    if (is_success) {
        gtk_label_set_text(GTK_LABEL(priv->status_label), "");
        gtk_widget_set_sensitive((GtkWidget*)priv->usb_devices_list_view, TRUE);
    } else {
        gtk_label_set_markup(GTK_LABEL(priv->status_label),
                "<span color=\"red\">Не удалось сбросить USB TCP устройства на виртуальной машине</span>");
    }
}

static void take_tk_address_from_ini(GtkEntry* tk_address_entry)
{
    gchar *current_tk_address = read_str_from_ini_file("General", "current_tk_address");
    if (current_tk_address) {
        gtk_entry_set_text(tk_address_entry, current_tk_address);
        g_free(current_tk_address);
    }
}

//Return address In case of success. Must be freed if its not NULL
static void usbredir_dialog_determine_tk_address(GTask    *task,
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
        g_info("GetAdaptersInfo call failed with %d", dwRetVal);
    }

    // 5
    PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
    while (pAdapter) {
        g_info("IP Address: %s", pAdapter->IpAddressList.IpAddress.String);
        g_info("Gateway: %s   %s", pAdapter->GatewayList.IpAddress.String, standard_output);
        // != NULL means standard_output contains  pAdapter->GatewayList.IpAddress.String
        if (strstr(standard_output, pAdapter->GatewayList.IpAddress.String) != NULL) {
            address = g_strdup(pAdapter->IpAddressList.IpAddress.String);
            break;
        }

        pAdapter = pAdapter->Next;
    }

    // 6
    if (pAdapterInfo)
        free(pAdapterInfo);
#endif

    clean_mark:
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

    UsbredirMainDialogData *priv = (UsbredirMainDialogData *)user_data;

    gpointer ptr_res = g_task_propagate_pointer(G_TASK (res), NULL);
    if (ptr_res) {
        gchar *tk_address = (gchar *)ptr_res;
        g_info("%s : %s", (const char*)__func__, tk_address);
        gtk_entry_set_text((GtkEntry*)priv->tk_address_entry, tk_address);

        free_memory_safely(&tk_address);
    } else { // get from ini if we didnt determine it
        take_tk_address_from_ini(GTK_ENTRY(priv->tk_address_entry));
    }
}

static void
usbredir_dialog_check_if_reset_required_and_reset(UsbredirMainDialogData *priv)
{
    if (usbredir_controller_is_tcp_usb_devices_reset_required()) {
        // set GUI
        gtk_widget_set_sensitive((GtkWidget*)priv->usb_devices_list_view, FALSE);
        gtk_spinner_start(GTK_SPINNER(priv->status_spinner));
        gtk_label_set_text(GTK_LABEL(priv->status_label), "Сбрасываем USB TCP устройства на ВМ");

        // send request to vdi (veil)
        DetachUsbData *detach_usb_data = calloc(1, sizeof(DetachUsbData));
        detach_usb_data->remove_all = TRUE;
        execute_async_task(usbredir_dialog_dettach_usb, usbredir_dialog_on_usb_tcp_reset_finished,
                detach_usb_data, priv);

        // try to get address on which the server will be created
        execute_async_task(usbredir_dialog_determine_tk_address, usbredir_dialog_on_determine_tk_address_finished,
                           NULL, priv);
    } else {
        take_tk_address_from_ini(GTK_ENTRY(priv->tk_address_entry));
    }

    usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(FALSE);
}

void
usbredir_dialog_start(GtkWindow *parent)
{
    UsbredirMainDialogData priv;
    usbredir_controller_set_gui_usb_tcp_window_data(TRUE, usbredir_dialog_usb_task_finished_callback, &priv);

    GtkBuilder *builder = remote_viewer_util_load_ui("usb_tcp_redir_form.ui");

    priv.main_window = get_widget_from_builder(builder, "main_window");
    priv.usb_devices_list_view = get_widget_from_builder(builder, "usb_devices_list_view");

    priv.tk_address_entry = get_widget_from_builder(builder, "tk_address_entry");
    gtk_entry_set_text(GTK_ENTRY(priv.tk_address_entry), "");

    priv.status_label = get_widget_from_builder(builder, "status_label");
    //gtk_label_set_selectable (GTK_LABEL(priv.status_label), TRUE);

    priv.status_spinner = get_widget_from_builder(builder, "status_spinner");
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv.usb_devices_list_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    priv.ok_btn = get_widget_from_builder(builder, "ok_btn");

    g_signal_connect_swapped(priv.main_window, "delete-event", G_CALLBACK(usbredir_dialog_window_deleted_cb), &priv);
    g_signal_connect(priv.ok_btn, "clicked", G_CALLBACK(usbredir_dialog_btn_ok_clicked_cb), &priv);

    /// view
    usbredir_dialog_add_columns_to_usb_view(&priv);

    // create model
    priv.usb_list_store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, GDK_TYPE_RGBA);
    ///
    // fill model
    usbredir_dialog_fill_usb_model(&priv);

    // set model to the view
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv.usb_devices_list_view), GTK_TREE_MODEL(priv.usb_list_store));

    // check if usb tcp reset required and reset
    usbredir_dialog_check_if_reset_required_and_reset(&priv);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(priv.main_window), parent);
    gtk_window_set_modal(GTK_WINDOW(priv.main_window), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(priv.main_window), TRUE);
    gtk_window_set_position(GTK_WINDOW(priv.main_window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size (GTK_WINDOW(priv.main_window),  1100, 50);
    gtk_widget_show_all(priv.main_window);

    create_loop_and_launch(&priv.loop);

    // save to ini
    const gchar *tk_address = gtk_entry_get_text((GtkEntry*)priv.tk_address_entry);
    write_str_to_ini_file("General", "current_tk_address", tk_address);

    // free
    g_object_unref(builder);
    gtk_widget_destroy(priv.main_window);
    usbredir_controller_set_gui_usb_tcp_window_data(FALSE, NULL, NULL);
}
