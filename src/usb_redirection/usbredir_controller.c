//
// Created by solomin on 03.09.2020.
//

#include <stdlib.h>

#include "async.h"
#include "remote-viewer-util.h"
#include "usbredir_controller.h"
#include "usbredirserver.h"

static UsbRedirController usbredir_controller; // 1 object for application

static void usbredir_controller_task_finished(GObject *source_object, GAsyncResult *res, gpointer user_data);

void usbredir_controller_init()
{
    usbredir_controller.usb_task_finished_callback = NULL;
    usbredir_controller.usb_task_finished_callback_data = NULL;

    usbredir_controller.tasks_array = g_array_new(FALSE, FALSE, sizeof(UsbRedirRunningTask*));
    usbredir_controller.port = 17777;
    usbredir_controller.reset_tcp_usb_devices_required = TRUE;
}

void usbredir_controller_deinit()
{
    usbredir_controller.usb_task_finished_callback = NULL;
    usbredir_controller_stop_all_cur_tasks(TRUE);

    for (int i = 0; i < usbredir_controller.tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array, UsbRedirRunningTask*, i);
        if (task)
            free(task);
    }
    g_array_free(usbredir_controller.tasks_array, TRUE);
    usbredir_controller.tasks_array = NULL;

}

void usbredir_controller_start_task(UsbServerStartData start_data)
{
    // Проверить данные. Reject if usb data already exists in array иначе все развалится, так как мы опираемся на
    // уникальность этой инфы
    int found_index = usbredir_controller_find_cur_task_index(start_data.usbbus, start_data.usbaddr);
    if (found_index != -1) {
        g_warning("%s: USB already redirected %i %i", (const char *)__func__, start_data.usbbus, start_data.usbaddr);
        return;
    }

    // add to array
    UsbRedirRunningTask *task = calloc(1, sizeof(UsbRedirRunningTask));
    task->start_data = start_data;
    g_array_append_val(usbredir_controller.tasks_array, task);

    // launch task
    execute_async_task(usbredirserver_launch_task, usbredir_controller_task_finished, task, NULL);
}

// Определяю нужную  таску по параметрам USB. Считаем, что эта пара уникальна и достаточна для идентификации usb/таски
// Опускаем флаг для остановки таски
void usbredir_controller_stop_task(int usbbus, int usbaddr)
{
    int found_index = usbredir_controller_find_cur_task_index(usbbus, usbaddr);
    g_info("%s %i", (const char *)__func__, found_index);
    if (found_index != -1) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array, UsbRedirRunningTask*, found_index);
        task->running_flag = 0;
    }
}

void usbredir_controller_stop_all_cur_tasks(gboolean with_sleep)
{
    if (usbredir_controller.tasks_array->len == 0)
        return;

    // lower running_flag for every task
    for (int i = 0; i < usbredir_controller.tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array, UsbRedirRunningTask*, i);
        task->running_flag = 0;
    }

    // wait a bit to give the tasks a chance to finish correctly (when quitting app)
    if (with_sleep)
        g_usleep(1500000);
}
// We suppose this port will be free. Ofc there is a very small chance that another process took it.
int usbredir_controller_get_free_port(void)
{
    return (usbredir_controller.port + (int)usbredir_controller.tasks_array->len);
}

// Return found item index or -1
int usbredir_controller_find_cur_task_index(int usbbus, int usbaddr)
{
    if (!usbredir_controller.tasks_array)
        return -1;

    for (int i = 0; i < usbredir_controller.tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array, UsbRedirRunningTask*, i);
        if (task->start_data.usbbus == usbbus && task->start_data.usbaddr == usbaddr) {
            return i;
        }
    }

    return -1;
}

// Активная зачада - та, которая существует и не находится в состоянии завершения.
gboolean usbredir_controller_check_if_task_active(int task_index)
{
    if (!usbredir_controller.tasks_array || usbredir_controller.tasks_array->len <= task_index)
        return FALSE;

    UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array, UsbRedirRunningTask*, task_index);
    if (task)
        return task->running_flag;
    else
        return FALSE;
}

void usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(gboolean flag)
{
    usbredir_controller.reset_tcp_usb_devices_required = flag;
}

gboolean usbredir_controller_is_tcp_usb_devices_reset_required(void)
{
    return usbredir_controller.reset_tcp_usb_devices_required;
}

void usbredir_controller_set_gui_usb_tcp_window_data(gboolean is_shown,
                                                     UsbTaskFinishedCallback usb_task_finished_callback,
                                                     gpointer callback_data)
{
    usbredir_controller.is_usb_tcp_window_shown = is_shown;
    usbredir_controller.usb_task_finished_callback = usb_task_finished_callback;
    usbredir_controller.usb_task_finished_callback_data = callback_data;
}

gboolean usbredir_controller_is_usb_tcp_window_shown(void)
{
    return usbredir_controller.is_usb_tcp_window_shown;
}

// STATIC FUNCTIONS
static void usbredir_controller_task_finished(GObject *source_object G_GNUC_UNUSED,
                                          GAsyncResult *res,
                                          gpointer user_data G_GNUC_UNUSED)
{
    GError *error = NULL;
    gpointer  ptr_res = g_task_propagate_pointer(G_TASK (res), &error);

    if(ptr_res == NULL) {
        g_info("%s: ptr_res == NULL. Its a logical error...", (const char *)__func__);
        if (error)
            g_info("%s", error->message);
        g_clear_error(&error);
        return;
    }

    UsbRedirTaskResaultData *task_resault_data = (UsbRedirTaskResaultData *)ptr_res;
    g_info("%s: USB redirection finished %i %i\n%s", (const char *)__func__, task_resault_data->usbbus,
           task_resault_data->usbaddr, task_resault_data->message);

    // Remove usb task data from tasks_array. Its cause task finished
    if (usbredir_controller.tasks_array) {
        int found_index = usbredir_controller_find_cur_task_index(task_resault_data->usbbus,
                                                                  task_resault_data->usbaddr);
        if (found_index != -1) {
            UsbRedirRunningTask *task = g_array_index(usbredir_controller.tasks_array,
                                                      UsbRedirRunningTask*, found_index);
            free_memory_safely(&task->start_data.ipv4_addr);
            free(task);
            g_info("%s: Before g_array_remove_index_fast %i", (const char *) __func__,
                   usbredir_controller.tasks_array->len);
            usbredir_controller.tasks_array = g_array_remove_index_fast(usbredir_controller.tasks_array,
                                                                        found_index);
            g_info("%s: After g_array_remove_index_fast %i", (const char *) __func__,
                   usbredir_controller.tasks_array->len);
        }
    }

    // invoke callback for usb selection form (if its open)
    if (usbredir_controller.usb_task_finished_callback)
        usbredir_controller.usb_task_finished_callback(task_resault_data,
                                                       usbredir_controller.usb_task_finished_callback_data);

    // free memory
    free_memory_safely(&task_resault_data->message);
    free(task_resault_data);
    g_clear_error(&error);
}

