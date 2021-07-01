/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdlib.h>


#include "async.h"
#include "remote-viewer-util.h"
#include "usbredir_controller.h"
#include "usbredirserver.h"

static UsbRedirController *usbredir_controller_static = NULL;; // 1 object for application

G_DEFINE_TYPE( UsbRedirController, usbredir_controller, G_TYPE_OBJECT )

static void usbredir_controller_task_finished(GObject *source_object, GAsyncResult *res, gpointer user_data);

static void usbredir_controller_class_init( UsbRedirControllerClass *klass )
{
    g_info("%s", (const char *)__func__);
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass );

    g_signal_new("usb-redir-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(UsbRedirControllerClass, usb_redir_finished),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 4,
                 G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
}

static void usbredir_controller_init( UsbRedirController *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

UsbRedirController *usbredir_controller_new()
{
    g_info("%s", (const char *)__func__);
    UsbRedirController *usbredir_controller = USBREDIR_CONTROLLER( g_object_new( TYPE_USBREDIR_CONTROLLER, NULL ) );

    usbredir_controller->tasks_array = g_array_new(FALSE, FALSE, sizeof(UsbRedirRunningTask*));
    usbredir_controller->port = 17777;
    usbredir_controller->reset_tcp_usb_devices_required = TRUE;
    return usbredir_controller;
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
    g_array_append_val(usbredir_controller_static->tasks_array, task);

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
        UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array, UsbRedirRunningTask*, found_index);
        task->running_flag = 0;
    }
}

void usbredir_controller_stop_all_cur_tasks(gboolean with_sleep)
{
    if (usbredir_controller_static->tasks_array->len == 0)
        return;

    // lower running_flag for every task
    for (guint i = 0; i < usbredir_controller_static->tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array, UsbRedirRunningTask*, i);
        task->running_flag = 0;
    }

    // wait a bit to give the tasks a chance to finish correctly (when quitting app)
    if (with_sleep)
        g_usleep(1500000);
}
// We suppose this port will be free. Ofc there is a very small chance that another process took it.
int usbredir_controller_get_free_port(void)
{
    return (usbredir_controller_static->port + (int)usbredir_controller_static->tasks_array->len);
}

// Return found item index or -1
int usbredir_controller_find_cur_task_index(int usbbus, int usbaddr)
{
    if (!usbredir_controller_static->tasks_array)
        return -1;

    for (guint i = 0; i < usbredir_controller_static->tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array, UsbRedirRunningTask*, i);
        if (task->start_data.usbbus == usbbus && task->start_data.usbaddr == usbaddr) {
            return i;
        }
    }

    return -1;
}

// Активная зачада - та, которая существует и не находится в состоянии завершения.
gboolean usbredir_controller_check_if_task_active(int task_index)
{
    if (task_index < 0)
        return FALSE;

    if (!usbredir_controller_static->tasks_array || usbredir_controller_static->tasks_array->len <= (guint)task_index)
        return FALSE;

    UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array,
            UsbRedirRunningTask*, task_index);
    if (task)
        return task->running_flag;
    else
        return FALSE;
}

void usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(gboolean flag)
{
    usbredir_controller_static->reset_tcp_usb_devices_required = flag;
}

gboolean usbredir_controller_is_tcp_usb_devices_reset_required(void)
{
    return usbredir_controller_static->reset_tcp_usb_devices_required;
}

gboolean usbredir_controller_is_usb_tcp_window_shown(void)
{
    return usbredir_controller_static->is_usb_tcp_window_shown;
}

void usbredir_controller_set_usb_tcp_window_shown(gboolean is_shown)
{
    usbredir_controller_static->is_usb_tcp_window_shown = is_shown;
}

UsbRedirController *usbredir_controller_get_static()
{
    if (usbredir_controller_static == NULL)
        usbredir_controller_static = usbredir_controller_new();

    return usbredir_controller_static;
}

void usbredir_controller_deinit_static()
{
    if(usbredir_controller_static == NULL)
        return;

    g_info("%s", (const char *)__func__);
    usbredir_controller_stop_all_cur_tasks(TRUE);

    for (guint i = 0; i < usbredir_controller_static->tasks_array->len; ++i) {
        UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array, UsbRedirRunningTask*, i);
        if (task)
            free(task);
    }
    g_array_free(usbredir_controller_static->tasks_array, TRUE);
    usbredir_controller_static->tasks_array = NULL;

    g_object_unref(usbredir_controller_static);
}


// STATIC FUNCTIONS
static void free_usbredir_task_resault_data(UsbRedirTaskResaultData *usbredir_task_resault_data)
{
    if (!usbredir_task_resault_data)
        return;

    free_memory_safely(&usbredir_task_resault_data->message);
    free(usbredir_task_resault_data);
    usbredir_task_resault_data = NULL;
}

static void usbredir_controller_task_finished(GObject *source_object G_GNUC_UNUSED,
                                          GAsyncResult *res,
                                          gpointer user_data G_GNUC_UNUSED)
{
    GError *error = NULL;
    gpointer  ptr_res = g_task_propagate_pointer(G_TASK (res), &error);

    if(ptr_res == NULL) {
        const char *error_msg = "ptr_res == NULL. Its a logical error...";
        g_info("%s: %s", (const char *)__func__, error_msg);
        if (error)
            g_info("%s", error->message);
        g_clear_error(&error);

        g_signal_emit_by_name(usbredir_controller_static, "usb-redir-finished",
                USB_REDIR_FINISH_FAIL, error_msg, 0, 0);
        return;
    }

    UsbRedirTaskResaultData *task_resault_data = (UsbRedirTaskResaultData *)ptr_res;
    g_info("%s: USB redirection finished %i %i\n%s", (const char *)__func__, task_resault_data->usbbus,
           task_resault_data->usbaddr, task_resault_data->message);

    // Remove usb task data from tasks_array. Its cause task finished
    if (usbredir_controller_static->tasks_array) {
        int found_index = usbredir_controller_find_cur_task_index(task_resault_data->usbbus,
                                                                  task_resault_data->usbaddr);
        if (found_index != -1) {
            UsbRedirRunningTask *task = g_array_index(usbredir_controller_static->tasks_array,
                                                      UsbRedirRunningTask*, found_index);
            free_memory_safely(&task->start_data.ipv4_addr);
            free(task);
            g_info("%s: Before g_array_remove_index_fast %i", (const char *) __func__,
                   usbredir_controller_static->tasks_array->len);
            usbredir_controller_static->tasks_array = g_array_remove_index_fast(usbredir_controller_static->tasks_array,
                                                                        found_index);
            g_info("%s: After g_array_remove_index_fast %i", (const char *) __func__,
                   usbredir_controller_static->tasks_array->len);
        }
    }

    // invoke callback for usb selection form
    g_signal_emit_by_name(usbredir_controller_static, "usb-redir-finished", task_resault_data->code,
                          task_resault_data->message, task_resault_data->usbbus, task_resault_data->usbaddr);

    // free memory
    free_usbredir_task_resault_data(task_resault_data);
    g_clear_error(&error);
}
