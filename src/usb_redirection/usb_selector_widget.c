/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <libusb.h>
#include <usbredirparser.h>

#include "usbredir_util.h"

#include "remote-viewer-util.h"
#include "usb_selector_widget.h"


static gboolean
usb_selector_widget_window_deleted_cb(UsbSelectorWidget *self)
{
    self->dialog_window_response = GTK_RESPONSE_CLOSE;
    shutdown_loop(self->loop);
    return TRUE;
}

static void
usb_selector_widget_btn_ok_clicked_cb(GtkButton *button G_GNUC_UNUSED, UsbSelectorWidget *self)
{
    shutdown_loop(self->loop);
}

static void
usb_selector_widget_fill_usb_model(UsbSelectorWidget *self)
{
    // fill data
    memset(self->usb_dev_data_array, 0, sizeof(self->usb_dev_data_array));
    GtkTreeIter iter;

    // get data using libinfo
    libusb_context *ctx;
    usbredir_util_init_libusb_and_set_options(&ctx, usbredirparser_info);

    libusb_device **usb_list = NULL;
    struct libusb_device_descriptor desc;
    self->cur_usb_devices_number = libusb_get_device_list(ctx, &usb_list);

    self->cur_usb_devices_number = MIN(MAX_USB_NUMBER, self->cur_usb_devices_number);
    for (ssize_t i = 0; i < self->cur_usb_devices_number; i++)
    {
        libusb_device *dev = usb_list[i];
        // get USB data
        UsbDeviceData usb_device_data;
        usb_device_data.usb_state = USB_STATE_NOT_USED;
        usb_device_data.usbbus = libusb_get_bus_number(dev);
        usb_device_data.usbaddr = libusb_get_device_address(dev);
        usb_device_data.pnum = libusb_get_port_number(dev);

        libusb_get_device_descriptor(dev, &desc);
        usb_device_data.idVendor = desc.idVendor;
        usb_device_data.idProduct = desc.idProduct;

        // get text description for gui
        gchar *manufacturer = NULL, *product = NULL;
        usbredir_util_get_device_strings(usb_device_data.usbbus, usb_device_data.usbaddr,
                                         desc.idVendor, desc.idProduct,
                                         &manufacturer, &product);

        gchar *usb_name = g_strdup_printf("%03i:%03i %s %s", usb_device_data.usbbus, usb_device_data.usbaddr,
                                          manufacturer, product);
        free_memory_safely(&manufacturer);
        free_memory_safely(&product);
        g_info("USB  %s", usb_name);

        self->usb_dev_data_array[i] = usb_device_data;
        //
        // Add a new row to the model
        gtk_list_store_append(self->usb_list_store, &iter);
        // initial state
        gtk_list_store_set(self->usb_list_store, &iter,
                           COLUMN_USB_NAME, usb_name,
                           COLUMN_USB_REDIR_TOGGLE, FALSE,
                           COLUMN_USB_REDIR_STATE, "",
                           -1);
        g_free(usb_name);
    }

    // release
    libusb_free_device_list(usb_list, 1);
    libusb_exit(ctx);
}

static void
usb_selector_widget_add_columns_to_usb_view(UsbSelectorWidget *self)
{
    /// view
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // column 1
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("USB", renderer, "text", COLUMN_USB_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->usb_devices_list_view), column);

    // column 2
    self->usb_toggle_cell_renderer = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_toggle_set_active((GtkCellRendererToggle*)self->usb_toggle_cell_renderer, TRUE);

    gtk_cell_renderer_toggle_set_activatable((GtkCellRendererToggle *)self->usb_toggle_cell_renderer, TRUE);
    column = gtk_tree_view_column_new_with_attributes("", self->usb_toggle_cell_renderer, "active", TRUE, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self->usb_devices_list_view), column);

    // column 3
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Состояние", renderer, "text", COLUMN_USB_REDIR_STATE,
                                                      "foreground-rgba", COLUMN_USB_COLOR, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->usb_devices_list_view), column);

    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(self->usb_devices_list_view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
}

// Инвертировать значение
static void
usb_selector_widget_on_usb_device_toggled(GtkCellRendererToggle *cell_renderer G_GNUC_UNUSED,
                      char *path, gpointer user_data)
{
    UsbSelectorWidget *usb_selector_widget = (UsbSelectorWidget *)user_data;

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(usb_selector_widget->usb_list_store), &iter, path)) {
        // get  value
        gboolean is_toggled;
        gtk_tree_model_get(GTK_TREE_MODEL(usb_selector_widget->usb_list_store), &iter,
                           COLUMN_USB_REDIR_TOGGLE, &is_toggled, -1);
        // toggle value
        gtk_list_store_set(usb_selector_widget->usb_list_store, &iter,
                           COLUMN_USB_REDIR_TOGGLE, !is_toggled,
                           -1);
    }
}

//static gchar *get_usb_id_str(UsbSelectorWidget *self, guint i)
//{
//    return g_strdup_printf("%04x:%04x",
//                           self->usb_dev_data_array[i].idVendor,
//                           self->usb_dev_data_array[i].idProduct);
//}

static gchar *get_usb_addr_str(UsbSelectorWidget *self, guint i)
{
    return g_strdup_printf("%02x:%02x",
                           self->usb_dev_data_array[i].usbbus,
                           self->usb_dev_data_array[i].usbaddr);
}

UsbSelectorWidget *usb_selector_widget_new()
{
    UsbSelectorWidget *self = calloc(1, sizeof(UsbSelectorWidget));

    self->builder = remote_viewer_util_load_ui("usb_selector_form.ui");

    self->main_window = get_widget_from_builder(self->builder, "main_window");
    self->usb_devices_list_view = get_widget_from_builder(self->builder, "usb_devices_list_view");

    self->tk_address_header = get_widget_from_builder(self->builder, "tk_address_header");
    self->tk_address_entry = get_widget_from_builder(self->builder, "tk_address_entry");
    gtk_entry_set_text(GTK_ENTRY(self->tk_address_entry), "");

    self->status_label = get_widget_from_builder(self->builder, "status_label");

    self->status_spinner = get_widget_from_builder(self->builder, "status_spinner");
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->usb_devices_list_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    self->ok_btn = get_widget_from_builder(self->builder, "ok_btn");

    // connect signals
    g_signal_connect_swapped(self->main_window, "delete-event",
            G_CALLBACK(usb_selector_widget_window_deleted_cb), self);
    g_signal_connect(self->ok_btn, "clicked", G_CALLBACK(usb_selector_widget_btn_ok_clicked_cb), self);

    /// create view
    usb_selector_widget_add_columns_to_usb_view(self);

    // create model
    self->usb_list_store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, GDK_TYPE_RGBA);
    ///
    // fill model
    usb_selector_widget_fill_usb_model(self);

    // set model to the view
    gtk_tree_view_set_model(GTK_TREE_VIEW(self->usb_devices_list_view), GTK_TREE_MODEL(self->usb_list_store));

    return self;
}

// Toggle value upon "toggled" signal
void usb_selector_widget_enable_auto_toggle(UsbSelectorWidget *self)
{
    g_signal_connect(self->usb_toggle_cell_renderer, "toggled",
                     G_CALLBACK(usb_selector_widget_on_usb_device_toggled), self);
}

void usb_selector_widget_show_and_start_loop(UsbSelectorWidget *self, GtkWindow *parent)
{
    gtk_window_set_transient_for(GTK_WINDOW(self->main_window), parent);
    gtk_window_set_modal(GTK_WINDOW(self->main_window), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(self->main_window), TRUE);
    gtk_window_set_position(GTK_WINDOW(self->main_window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size (GTK_WINDOW(self->main_window),  1100, 50);
    gtk_widget_show_all(self->main_window);

    create_loop_and_launch(&self->loop);

    gtk_widget_hide(self->main_window);
}

void usb_selector_widget_free(UsbSelectorWidget *self)
{
    g_object_unref(self->builder);
    gtk_widget_destroy(self->main_window);
    free(self);
}

const gchar *usb_selector_widget_get_usb_status_text(UsbSelectorWidget *self, int usb_index)
{
    switch (self->usb_dev_data_array[usb_index].usb_state) {
        case USB_STATE_BEING_REDIRECTED:
            return "USB перенаправляется";
        case USB_STATE_CANCELLING_REDIRECTION:
            return"Завершаем перенаправление USB. Ожидайте";
        case USB_STATE_NOT_USED:
        default:
            return "";
    }
}

// Setup GUI based on save USBs
void usb_selector_widget_set_selected_usb_str(UsbSelectorWidget *self, const gchar *usb_devices)
{
    if (!usb_devices)
        return;

    for (ssize_t i = 0; i < self->cur_usb_devices_number; i++)
    {
        GtkTreeIter iter;
        g_autofree gchar *path_string = NULL;
        path_string = g_strdup_printf("%zi", i);
        if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(self->usb_list_store), &iter, path_string)) {

            // toggle if string 'usb_devices' contains USB
            g_autofree gchar *usb_addr_str = NULL;
            usb_addr_str = get_usb_addr_str(self, i);

            if (strstr(usb_devices, usb_addr_str) != NULL) {
                gtk_list_store_set(self->usb_list_store, &iter, COLUMN_USB_REDIR_TOGGLE, TRUE, -1);
            }
        }
    }
}

// Возвращает строку содержащую id тех USB что выбраны на ГУИ
// Must be freed
gchar *usb_selector_widget_get_selected_usb_str(UsbSelectorWidget *self)
{
    gchar *final_string = NULL;

    for (ssize_t i = 0; i < self->cur_usb_devices_number; i++) {

        GtkTreeIter iter;
        g_autofree gchar *path_string = NULL;
        path_string = g_strdup_printf("%zi", i);
        if(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(self->usb_list_store), &iter, path_string)) {
            gboolean is_toggled;
            gtk_tree_model_get(GTK_TREE_MODEL(self->usb_list_store), &iter,
                               COLUMN_USB_REDIR_TOGGLE, &is_toggled, -1);
            // Если toggled то забираем
            if (is_toggled) {
                if (final_string == NULL) {
                    final_string = get_usb_addr_str(self, i);
                } else {
                    g_autofree gchar *usb_addr_str = NULL;
                    usb_addr_str = get_usb_addr_str(self, i);
                    gchar *new_final_string = g_strdup_printf("%s#%s", final_string, usb_addr_str);
                    g_free(final_string);
                    final_string = new_final_string;
                }
            }
        }
    }

    return final_string;
}
//   /usb:addr:01:0f