//
// Created by solomin on 15.06.19.
//

#ifndef VIRT_VIEWER_VEIL_VDI_API_SESSION_H
#define VIRT_VIEWER_VEIL_VDI_API_SESSION_H

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>

#include "vdi_redis_client.h"
#include "vdi_ws_client.h"
#include "async.h"

#define HTTP_RESPONSE_TIOMEOUT 15

// remote protocol type
typedef enum{
    VDI_SPICE_PROTOCOL,
    VDI_SPICE_DIRECT_PROTOCOL,
    VDI_RDP_PROTOCOL,
    VDI_RDP_WINDOWS_NATIVE_PROTOCOL,
    VDI_ANOTHER_REMOTE_PROTOCOL
} VdiVmRemoteProtocol;

// vm operational system
typedef enum{
    VDI_VM_WIN,
    VDI_VM_LINUX,
    VDI_VM_ANOTHER_OS
} VdiVmOs;

// Инфа о виртуальной машине полученная от vdi
typedef struct{

    VdiVmOs os_type;

    gchar *vm_host;
    int vm_port;
    gchar *vm_password;
    gchar *vm_verbose_name;

    gchar *message;

    gint test_data;

} VdiVmData;

typedef struct{

    SoupSession *soup_session;

    gchar *vdi_username;
    gchar *vdi_password;
    gchar *vdi_ip;
    int vdi_port;

    gchar *api_url;
    gchar *auth_url;
    gchar *jwt;
    gboolean is_ldap;

    // data aboyr current pool and vm
    gchar *current_pool_id;
    VdiVmRemoteProtocol current_remote_protocol;
    gchar *current_vm_id;
    gchar *current_vm_verbose_name;
    gchar *current_controller_address;

    RedisClient redis_client;

} VdiSession;

// Data which passed to vdi_session_api_call
typedef struct{
    gchar *action_on_vm_str;
    gboolean is_action_forced;

} ActionOnVmData;

typedef struct {
    gchar *host_address;
    int host_port;
    int *p_running_flag;// pointer to a stack variable. never try to free it
} AttachUsbData;

typedef struct {
    gchar *usb_uuid;
    gboolean remove_all;
} DetachUsbData;

// Functions
// init session
void vdi_session_create(void);
// deinit session
void vdi_session_destroy(void);
// get vid server ip
const gchar *vdi_session_get_vdi_ip(void);
// get port
int vdi_session_get_vdi_port(void);
// get username
const gchar *vdi_session_get_vdi_username(void);
// get password
const gchar *vdi_session_get_vdi_password(void);
// cancell pending requests
void vdi_session_cancell_pending_requests(void);
// set vdi session credentials
void vdi_session_set_credentials(const gchar *username, const gchar *password, const gchar *ip,
                         int port, gboolean is_ldap);
// set current vm id
void vdi_session_set_current_pool_id(const gchar *current_pool_id);
// get current vm id
const gchar *vdi_session_get_current_pool_id(void);

// set current remote protocol
void vdi_session_set_current_remote_protocol(VdiVmRemoteProtocol remote_protocol);
// get current remote protocol
VdiVmRemoteProtocol vdi_session_get_current_remote_protocol(void);

VdiVmRemoteProtocol vdi_session_str_to_remote_protocol(const gchar *protocol_str);
const gchar *vdi_session_remote_protocol_str(VdiVmRemoteProtocol protocol);

// get current vm name
const gchar *vdi_session_get_current_vm_name(void);

const gchar *vdi_session_get_current_controller_address(void);

//void gInputStreamToBuffer(GInputStream *inputStream, gchar *responseBuffer);
// Do api call. Return response body
gchar *vdi_session_api_call(const char *method, const char *uri_string, const gchar *body_str, int *resp_code);

/// Functions for GTasks
// Fetch token
void vdi_session_log_in(GTask *task,
                   gpointer       source_object,
                   gpointer       task_data,
                   GCancellable  *cancellable);

// Запрашиваем список пулов
void vdi_session_get_vdi_pool_data(GTask *task,
                       gpointer       source_object,
                       gpointer       task_data,
                       GCancellable  *cancellable);

// Получаем виртуалку из пула
void vdi_session_get_vm_from_pool(GTask *task,
                      gpointer       source_object,
                      gpointer       task_data,
                      GCancellable  *cancellable);

// Do action on virtual machine
void vdi_session_do_action_on_vm(GTask *task,
                     gpointer       source_object,
                     gpointer       task_data,
                     GCancellable  *cancellable);

// Log out sync
gboolean vdi_session_logout(void);

// Attach USB
gchar *vdi_session_attach_usb(AttachUsbData *attach_usb_data);

// Detach USB
gboolean vdi_session_detach_usb(DetachUsbData *detach_usb_data);

// Do action on vm async
void vdi_api_session_execute_task_do_action_on_vm(const gchar *actionStr, gboolean isForced);


void vdi_api_session_free_action_on_vm_data(ActionOnVmData *action_on_vm_data);
void vdi_api_session_free_vdi_vm_data(VdiVmData *vdi_vm_data);
void vdi_api_session_free_attach_usb_data(AttachUsbData *attach_usb_data);
void vdi_api_session_free_detach_usb_data(DetachUsbData *detach_usb_data);

#endif //VIRT_VIEWER_VEIL_VDI_API_SESSION_H
