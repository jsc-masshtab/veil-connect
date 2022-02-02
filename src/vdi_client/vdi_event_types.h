/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_VDI_EVENT_TYPES_H
#define VEIL_CONNECT_VDI_EVENT_TYPES_H

typedef enum{
    VDI_EVENT_TYPE_UNKNOWN,
    VDI_EVENT_TYPE_VM_CONNECTED,
    VDI_EVENT_TYPE_VM_DISCONNECTED,
    VDI_EVENT_TYPE_CONN_ERROR
} VdiEventType;

#endif //VEIL_CONNECT_VDI_EVENT_TYPES_H
