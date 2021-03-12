/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_USBREDIR_CONTROLLER_H
#define VEIL_CONNECT_USBREDIR_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "usbredirserver_data.h"

#define TYPE_USBREDIR_CONTROLLER   ( usbredir_controller_get_type( ) )
#define USBREDIR_CONTROLLER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_USBREDIR_CONTROLLER, UsbRedirController) )


typedef struct{
    GObject parent;

    GArray *tasks_array;

    int port;

    gboolean reset_tcp_usb_devices_required; // Если флаг поднят, то при отрытии gui окна для редиректа usb
    // первым делом отправляем запрос на удаление всех tcp usb.

    gboolean is_usb_tcp_window_shown; // Открыто ли в данный момент окно управления USB TCP
} UsbRedirController;

typedef struct
{
    GObjectClass parent_class;

    /* signals */
    void (*usb_redir_finished)(UsbRedirController *self, int code, const gchar *message, int usbbus, int usbaddr);

} UsbRedirControllerClass;

GType usbredir_controller_get_type( void ) G_GNUC_CONST;

UsbRedirController *usbredir_controller_new(void);

void usbredir_controller_start_task(UsbServerStartData start_data);
void usbredir_controller_stop_task(int usbbus, int usbaddr);
void usbredir_controller_stop_all_cur_tasks(gboolean with_sleep);

int usbredir_controller_get_free_port(void);
int usbredir_controller_find_cur_task_index(int usbbus, int usbaddr);
gboolean usbredir_controller_check_if_task_active(int task_index);

void usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(gboolean flag);
gboolean usbredir_controller_is_tcp_usb_devices_reset_required(void);

gboolean usbredir_controller_is_usb_tcp_window_shown(void);
void usbredir_controller_set_usb_tcp_window_shown(gboolean is_shown);

UsbRedirController *usbredir_controller_get_static(void);
void usbredir_controller_deinit_static(void);

#endif //VEIL_CONNECT_USBREDIR_CONTROLLER_H
