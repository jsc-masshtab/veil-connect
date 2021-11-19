/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "vdi_session.h"
#include "vdi_event.h"

// Данные события
typedef struct{
    VdiEventType event;

    gchar *vm_id;

    gchar *conn_error_str;
    int conn_error_code;

} TkEventData;

static void vdi_event_free_tk_event_data(TkEventData *event_data)
{
    g_free(event_data->vm_id);
    g_free(event_data->conn_error_str);
    free(event_data);
}

// Выполняется в основном потоке при возникновении событий
static gboolean vdi_event_occurred(TkEventData *event_data)
{
    g_info("%s  EVENT: %i", (const char *)__func__, (int)event_data->event);

    // В основном потоке шлем сообщения VDI серверу
    if (event_data->event == VDI_EVENT_TYPE_VM_CHANGED) {
        vdi_ws_client_send_vm_changed(vdi_session_get_ws_client(), event_data->vm_id);
    }
    else if (event_data->event == VDI_EVENT_TYPE_CONN_ERROR) {
        vdi_ws_client_send_conn_error(vdi_session_get_ws_client(),
                                      event_data->conn_error_code, event_data->conn_error_str);
    }

    vdi_event_free_tk_event_data(event_data);
    return FALSE;
}

void vdi_event_vm_changed_notify(const gchar *vm_id)
{
    TkEventData *event_data = calloc(1, sizeof(TkEventData));
    event_data->event = VDI_EVENT_TYPE_VM_CHANGED;
    event_data->vm_id = g_strdup(vm_id);

    gdk_threads_add_idle((GSourceFunc)vdi_event_occurred, event_data);
}

void vdi_event_conn_error_notify(guint32 conn_error_code, const gchar *conn_error_str)
{
    TkEventData *event_data = calloc(1, sizeof(TkEventData));
    event_data->event = VDI_EVENT_TYPE_CONN_ERROR;
    event_data->conn_error_code = conn_error_code;
    event_data->conn_error_str = g_strdup(conn_error_str);

    gdk_threads_add_idle((GSourceFunc)vdi_event_occurred, event_data);
}
