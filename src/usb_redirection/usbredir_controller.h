//
// Created by solomin on 03.09.2020.
//

#ifndef VEIL_CONNECT_USBREDIR_CONTROLLER_H
#define VEIL_CONNECT_USBREDIR_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "usbredirserver_data.h"
//  int usbbus, int usbaddr, const char *message
typedef void (*UsbTaskFinishedCallback) (UsbRedirTaskResaultData *res_data, gpointer callback_data);

// UsbRedirController will only 1  static object in application
typedef struct{
    GArray *tasks_array;

    UsbTaskFinishedCallback usb_task_finished_callback;
    gpointer usb_task_finished_callback_data;

    int port;

    gboolean reset_tcp_usb_devices_required; // Если флаг поднят, то при отрытии gui окна для редиректа usb
    // первым делом отправляем запрос на удаление всех tcp usb.

    gboolean is_usb_tcp_window_shown; // Открыто ли в данный момент окно управления USB TCP
} UsbRedirController;


void usbredir_controller_init(void);
void usbredir_controller_deinit(void);
void usbredir_controller_start_task(UsbServerStartData start_data);
void usbredir_controller_stop_task(int usbbus, int usbaddr);
void usbredir_controller_stop_all_cur_tasks(gboolean with_sleep);

int usbredir_controller_get_free_port(void);
int usbredir_controller_find_cur_task_index(int usbbus, int usbaddr);
gboolean usbredir_controller_check_if_task_active(int task_index);

void usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(gboolean flag);
gboolean usbredir_controller_is_tcp_usb_devices_reset_required(void);

void usbredir_controller_set_gui_usb_tcp_window_data(gboolean is_shown,
        UsbTaskFinishedCallback usb_task_finished_callback,
        gpointer callback_data);
gboolean usbredir_controller_is_usb_tcp_window_shown(void);

#endif //VEIL_CONNECT_USBREDIR_CONTROLLER_H
