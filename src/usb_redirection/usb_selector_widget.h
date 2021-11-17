/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_USB_SELECTOR_WIDGET_H
#define VEIL_CONNECT_USB_SELECTOR_WIDGET_H

#include <gtk/gtk.h>


#define MAX_USB_NUMBER 32

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

    uint16_t idVendor;
    uint16_t idProduct;

    uint8_t pnum; // port

    UsbState usb_state;

} UsbDeviceData;

typedef struct {
    GtkBuilder *builder;

    GtkWidget *main_window;
    GtkWidget *usb_devices_list_view;
    GtkWidget *tk_address_entry;
    GtkWidget *tk_address_header;
    GtkWidget *status_label;
    GtkWidget *status_spinner;
    GtkWidget *ok_btn;
    GtkCellRenderer *usb_toggle_cell_renderer;

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    GtkListStore *usb_list_store;

    UsbDeviceData usb_dev_data_array[MAX_USB_NUMBER]; // Используем статич. массив так число usb ограничено и не велико.
    ssize_t cur_usb_devices_number;

} UsbSelectorWidget;

UsbSelectorWidget *usb_selector_widget_new(const gchar *title);
void usb_selector_widget_enable_auto_toggle(UsbSelectorWidget *self);
void usb_selector_widget_show_and_start_loop(UsbSelectorWidget *self, GtkWindow *parent);
void usb_selector_widget_free(UsbSelectorWidget *self);
const gchar *usb_selector_widget_get_usb_status_text(UsbSelectorWidget *self, int usb_index);

void usb_selector_widget_set_selected_usb_str(UsbSelectorWidget *self, const gchar *usb_devices);
gchar *usb_selector_widget_get_selected_usb_str(UsbSelectorWidget *self);

#endif //VEIL_CONNECT_USB_SELECTOR_WIDGET_H
