/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */


#ifndef VEIL_CONNECT_VDI_EVENT_H
#define VEIL_CONNECT_VDI_EVENT_H

#include <glib.h>

#include "vdi_event_types.h"
#include "vdi_ws_client.h"


void vdi_event_vm_changed_notify(const gchar *vm_id, VdiEventType event);
void vdi_event_conn_error_notify(guint32 conn_error_code, const gchar *conn_error_str);


#endif //VEIL_CONNECT_VDI_EVENT_H
